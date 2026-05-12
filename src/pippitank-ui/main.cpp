#include <fstream>
#include <opencv2/opencv.hpp>
#include <string>
#include <string_view>
//#include <csignal>
#include <algorithm>
#include <chrono>
#include <iomanip>
//#include <cstdio>
#include <mutex>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <tl/expected.hpp>
#include <sys/wait.h>
//#include <iomanip>
#include <vector>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <charconv>
#include <sstream>
#include "pippitank-protocol.hpp"
#include "fd.hpp"
#include "telemetry.hpp"
#include <linux/input.h>

namespace {

class FfmpegSubprocess {
public:
    FfmpegSubprocess() = default;
    ~FfmpegSubprocess() {
        if (pid_ > 0) {
            ::kill(pid_, SIGKILL);
            ::waitpid(pid_, nullptr, 0);
        }
    }
    FfmpegSubprocess(const FfmpegSubprocess&) = delete;
    FfmpegSubprocess& operator=(const FfmpegSubprocess&) = delete;
    FfmpegSubprocess(FfmpegSubprocess&& o) noexcept
        : pid_(o.pid_), fd_(std::move(o.fd_)) { o.pid_ = -1; }
    FfmpegSubprocess& operator=(FfmpegSubprocess&& o) noexcept {
        if (this != &o) {
            if (pid_ > 0) {
                ::kill(pid_, SIGKILL);
                ::waitpid(pid_, nullptr, 0);
            }
            pid_ = o.pid_;
            fd_ = std::move(o.fd_);
            o.pid_ = -1;
        }
        return *this;
    }
    int fd() const { return fd_.get(); }
    ::pid_t pid() const { return pid_; }
private:
    friend tl::expected<FfmpegSubprocess, std::string>
    spawn_ffmpeg(const std::string& url); //{

    ::pid_t pid_ = -1;
    ptank::Fd fd_;
};

tl::expected<FfmpegSubprocess, std::string>
spawn_ffmpeg(const std::string& url) {
    int pipefd[2];
    if (::pipe(pipefd) < 0)
        return tl::make_unexpected("[ERROR] pipe failed: " + std::string(::strerror(errno)));

    ::pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return tl::make_unexpected("[ERROR] fork failed: " + std::string(::strerror(errno)));
    }

    if (pid == 0) {
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::close(pipefd[1]);
        ::execlp("ffmpeg", "ffmpeg",
                 "-nostdin",
                 "-hide_banner", "-loglevel", "error",
                 "-fflags", "nobuffer", "-flags", "low_delay",
                 "-i", url.c_str(),
                 "-f", "rawvideo", "-pix_fmt", "bgr24", "-",
                 nullptr);
        ::_exit(127);
    }

    ::close(pipefd[1]);
    FfmpegSubprocess sp;
    sp.pid_ = pid;
    sp.fd_ = ptank::Fd(pipefd[0]);
    return sp;
}

struct Args {
    std::string video = "0";
    std::string server_host;
    std::uint16_t server_port = 0;
    std::string input;
    std::string log;
};

struct CaptureState {
    std::mutex mtx;
    cv::Mat latest;
    std::atomic<uint64_t> seq{0};
    std::atomic<bool> running{true};
};

constexpr int CANVAS_W = 1280;
constexpr int CANVAS_H = 720;
constexpr int CAMERA_W = 640;
constexpr int CAMERA_H = 480;
constexpr int CAMERA_X = CANVAS_W - CAMERA_W;
constexpr int CAMERA_Y = 0;
constexpr int FRAME_BYTES = CAMERA_W * CAMERA_H * 3; // BGR24

void capture_loop(int fd, CaptureState& st) {
    std::vector<uchar> buf(FRAME_BYTES);
    while (st.running.load()) {
        ::ssize_t total = 0;
        while (total < FRAME_BYTES) {
            ::ssize_t n = ::read(fd, buf.data() + total, FRAME_BYTES - total);
            if (n > 0) { total += n; continue; }
            if (n == 0) std::cerr << "[INFO] ffmpeg EOF. \n";
            else std::cerr << "[ERROR] read failed: " << ::strerror(errno) << "\n";
            st.running.store(false);
            return;
        }

        cv::Mat frame(CAMERA_H, CAMERA_W, CV_8UC3, buf.data());
        {
            std::lock_guard<std::mutex> lk(st.mtx);
            st.latest = frame.clone();
        }
        st.seq.fetch_add(1, std::memory_order_release);
    }
}

using AddrInfoPtr = std::unique_ptr<::addrinfo, decltype(&::freeaddrinfo)>;

tl::expected<ptank::Fd, std::string>
open_connect(const std::string& host, std::uint16_t port) {
    ::addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = 0;

    ::addrinfo* raw = nullptr;
    int rc = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &raw);
    if (rc != 0)
        return tl::make_unexpected("getaddrinfo failed: " + std::string(::gai_strerror(rc)));

    AddrInfoPtr res(raw, &::freeaddrinfo);

    ptank::Fd sock(::socket(res->ai_family, res->ai_socktype, res->ai_protocol));
    if (!sock.valid())
        return tl::make_unexpected("socket failed: " + std::string(::strerror(errno)));

    int yes = 1;
    if (::setsockopt(sock.get(), IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
        return tl::make_unexpected("setsockopt failed: " + std::string(::strerror(errno)));
    if (::connect(sock.get(), res->ai_addr, res->ai_addrlen) < 0)
        return tl::make_unexpected("connect failed: " + std::string(::strerror(errno)));

    return sock;
}

tl::expected<void, std::string>
parse_server(std::string_view spec, Args& args) {
    auto col = spec.find(':');
    if (col == std::string_view::npos)
        return tl::make_unexpected("--server: missing ':'");

    auto host_sv = spec.substr(0, col);
    auto port_sv = spec.substr(col + 1);

    if (host_sv.empty()) return tl::make_unexpected("--server: empty host");
    if (port_sv.empty()) return tl::make_unexpected("--server: empty port");

    std::uint16_t port = 0;

    auto [ptr, ec] = std::from_chars(
            port_sv.data(), port_sv.data() + port_sv.size(), port);

    if (ec != std::errc{} || ptr != port_sv.data() + port_sv.size())
        return tl::make_unexpected("--server: bad port: " + std::string(port_sv));

    args.server_host = std::string(host_sv);
    args.server_port = port;

    return {};
}

struct TelemetryState {
    std::mutex mtx;
    ptank::FrameEngine latest{};
    std::atomic<uint64_t> seq{0};
    std::atomic<bool> running{true};
    std::atomic<uint64_t> decoded{0};
    std::atomic<uint64_t> errors{0};
};

class CsvLogger {
public:
    CsvLogger() = default;
    ~CsvLogger() = default;
    CsvLogger(const CsvLogger&) = delete;
    CsvLogger& operator=(const CsvLogger&) = delete;

    tl::expected<void, std::string> open(const std::string& path) {
        ofs_.open(path, std::ios::out | std::ios::trunc);
        if (!ofs_)
            return tl::make_unexpected("log open failed: " + path + " (" + std::strerror(errno) + ")");
        ofs_ << "t_unix_ms,l,r,v_cell_1,v_bat,v_q,i_esc_1,i_esc_2,i_sys,temp_c,uptime_ms\n";
        ofs_.flush();
        return {};
    }

    bool active() const {
        return ofs_.is_open();
    }

    void write_row(std::int64_t t_ms, std::int16_t l, std::int16_t r, const ptank::EngineTelemetry& t) {
        ofs_ << t_ms << ","
             << l    << ","
             << r    << ","
             << std::fixed << std::setprecision(3)
             << t.v_cell_1  << ","
             << t.v_bat     << ","
             << t.v_q       << ","
             << t.i_esc_1   << ","
             << t.i_esc_2   << ","
             << t.i_sys     << ","
             << t.temp_c    << ","
             << t.uptime_ms << "\n";
        ofs_.flush();
    }
private:
    std::ofstream ofs_;
};

struct DriveState {
    std::atomic<int16_t> l{0};
    std::atomic<int16_t> r{0};
};

std::int64_t now_unix_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

void telemetry_loop(int fd, TelemetryState& st, CsvLogger& logger, DriveState& drv_st) {
    ptank::Receiver rx;
    std::uint8_t buf[256];
    while(st.running.load()) {
        ::ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) {
            std::cerr << "[INFO] read length is 0\n";
            st.running.store(false);
            break;
        }
        for (::ssize_t i = 0; i < n; i++) {
            if (auto frame = rx.feed(buf[i])) {
                auto [p, len] = *frame;
                const auto src = static_cast<ptank::Source>(p[1]);
                if (src == ptank::Source::Engine) {
                    if (auto f = ptank::decode_engine(p, len)) {
                        st.decoded.fetch_add(1, std::memory_order_release);
                        {
                            std::lock_guard<std::mutex> lk(st.mtx);
                            st.latest = *f;
                        }
                        st.seq.fetch_add(1, std::memory_order_release);

                        if (logger.active()) {
                            auto t = ptank::to_telemetry(*f);
                            logger.write_row(
                                    now_unix_ms(),
                                    drv_st.l.load(std::memory_order_acquire),
                                    drv_st.r.load(std::memory_order_acquire),
                                    t);
                        }
                    } else {
                        std::cerr << "[ERROR]: decode_error" << static_cast<int>(f.error()) << "\n";
                        st.errors.fetch_add(1, std::memory_order_release);
                    }
                }
            }
        }
    }
}

struct RateMeter {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    std::uint64_t base = 0;
    double hz = 0.0;
    void update(std::uint64_t count_now) {
        auto now = std::chrono::steady_clock::now();
        double el = std::chrono::duration<double>(now - t0).count();
        if (el >= 0.5) {
            hz = (count_now - base) / el;
            base = count_now;
            t0 = now;
        }
    }
    std::string to_string() {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << hz;
        return ss.str();
    }
};

std::string telem_fmt_v(float v) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

std::string telem_fmt_a(float a) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << a;
    return ss.str();
}

std::string telem_fmt_temp(float t) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << t;
    return ss.str();
}

std::string telem_fmt_ms(std::uint32_t ms) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ms / 1000.0f;
    return ss.str();
}

void draw_telem_line(cv::Mat& canvas, const std::string& text, cv::Point org) {
    constexpr int FONT     = cv::FONT_HERSHEY_COMPLEX;
    constexpr double SCALE = 0.7;
    constexpr int THICK    = 1;
    const cv::Scalar BG{30, 30, 30};
    const cv::Scalar FG{155, 155, 155};
    int baseline = 0;
    auto sz = cv::getTextSize(text, FONT, SCALE, THICK, &baseline);
    cv::Rect clear(org.x - 2, org.y - sz.height - 2, sz.width + 4, sz.height + baseline + 4);
    canvas(clear) = cv::Scalar(BG);
    cv::putText(canvas, text, org, FONT, SCALE, FG, THICK, cv::LINE_AA);
}

struct Keys {
    std::atomic<bool> w{false};
    std::atomic<bool> a{false};
    std::atomic<bool> s{false};
    std::atomic<bool> d{false};
    std::atomic<bool> lshift{false};
    std::atomic<bool> rshift{false};
};

void evdev_loop(int fd, Keys& keys, std::atomic<bool>& running) {
    while (running.load()) {
        struct input_event ev;
        ssize_t n = ::read(fd, &ev, sizeof(ev));
        if (n == sizeof(ev)) {
            if (ev.type != EV_KEY) continue;
            if (ev.value == 2) continue; // ignore auto-repeat
            const bool down = (ev.value == 1);
            switch (ev.code) {
            case KEY_W:          keys.w.store(down); break;
            case KEY_A:          keys.a.store(down); break;
            case KEY_S:          keys.s.store(down); break;
            case KEY_D:          keys.d.store(down); break;
            case KEY_LEFTSHIFT:  keys.lshift.store(down); break;
            case KEY_RIGHTSHIFT: keys.rshift.store(down); break;
            default: break;
            }
            std::cerr << "[KEY] code=" << ev.code << " val=" << ev.value
                      << " | W" << keys.w << " A" << keys.a
                      << " S" << keys.s << " D" << keys.d
                      << " SH" << (keys.lshift.load() || keys.rshift.load()) << "\n";
            continue;
        }
        if (n == 0) {
            std::cerr << "[INFO] evdev EOF\n";
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (errno == EINTR) continue;
        std::cerr << "[ERROR] evdev read failed: " << ::strerror(errno) << "\n";
        break;
    }
}

struct RampState {
    float fwd  = 0.0f;
    float turn = 0.0f;
};

constexpr float RAMP_PER_TICK  = 15.0f;
constexpr float DECAY_PER_TICK = 20.0f;
constexpr float TARGET_MAX     = 500.0f;
constexpr int16_t CLIP         = 500;

struct DriveOut {
    std::int16_t l, r;
};

DriveOut tick_ramp(RampState& st, const Keys& k) {
    const bool shift = k.lshift.load() || k.rshift.load();

    float target_fwd = 0.0f;
    if (k.w.load()) target_fwd += TARGET_MAX;
    if (k.s.load()) target_fwd -= TARGET_MAX;

    float target_turn = 0.0f;
    if (k.d.load()) target_turn += TARGET_MAX;
    if (k.a.load()) target_turn -= TARGET_MAX;

    if (!shift) {
        float diff = target_fwd - st.fwd;
        float step = (target_fwd != 0.0f) ? RAMP_PER_TICK : DECAY_PER_TICK;
        if      (diff >  step) st.fwd += step;
        else if (diff < -step) st.fwd -= step;
        else                   st.fwd  = target_fwd;

        diff = target_turn - st.turn;
        step = (target_turn != 0.0f) ? RAMP_PER_TICK : DECAY_PER_TICK;
        if      (diff >  step) st.turn += step;
        else if (diff < -step) st.turn -= step;
        else                   st.turn = target_turn;
    }

    float l = st.fwd + st.turn;
    float r = st.fwd - st.turn;

    l = std::clamp(l, -static_cast<float>(CLIP), static_cast<float>(CLIP));
    r = std::clamp(r, -static_cast<float>(CLIP), static_cast<float>(CLIP));

    return {
        static_cast<int16_t>(l),
        static_cast<int16_t>(r),
    };
}


void cmd_send_loop(int sock_fd, DriveState& st, Keys& keys, std::atomic<bool>& running) {
    RampState rs;
    while (running.load()) {
        auto out = tick_ramp(rs, keys);
        st.l.store(out.l, std::memory_order_release);
        st.r.store(out.r, std::memory_order_release);
        ptank::CmdDrivePayload payload{out.l, out.r};
        ptank::FrameCmdDrive frame;
        ptank::encode(frame, payload);
        ssize_t w = ::send(sock_fd, &frame, sizeof(frame), MSG_NOSIGNAL);

        if (w < 0 && errno != EPIPE && errno != ECONNRESET) {
            std::cerr << "[ERROR] cmd send failed: " << ::strerror(errno) << "\n";
        }

        //std::cerr << "[DRIVE] l=" << out.l << " r=" << out.r
        //          << " fwd=" << rs.fwd << " turn=" << rs.turn << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

constexpr int BOTTOM_Y = CAMERA_H;
constexpr int HALF_W   = CANVAS_W / 2;

void draw_drive_bars(cv::Mat& canvas, std::int16_t l, std::int16_t r) {
    const cv::Scalar BG{30, 30, 30};
    const cv::Scalar GREEN{0, 200, 0};
    const cv::Scalar RED{0, 0, 200};

    cv::rectangle(canvas, {0, BOTTOM_Y + 1}, {CANVAS_W, CANVAS_H}, BG, cv::FILLED);

    int l_len = std::abs(static_cast<int>(l)) * HALF_W / CLIP;
    cv::rectangle(canvas, {0, BOTTOM_Y + 1}, {l_len, CANVAS_H}, (l >= 0) ? GREEN : RED, cv::FILLED);

    int r_len = std::abs(static_cast<int>(r)) * HALF_W / CLIP;
    cv::rectangle(canvas, {CANVAS_W - r_len, BOTTOM_Y + 1}, {CANVAS_W, CANVAS_H}, (r >= 0) ? GREEN : RED, cv::FILLED);
}

}

int main(int argc, char* argv[]) {
    // Parse args.
    Args args;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "--video") {
            if (++i >= argc) {
                std::cerr << "[ERROR] --video requires value\n";
                return 1;
            }
            args.video = argv[i];
        } else if (arg == "--server") {
            if (++i >= argc)  {
                std::cerr << "[ERROR] --server requires value\n";
                return 1;
            }
            if (auto server_r = parse_server(argv[i], args); !server_r) {
                std::cerr << "[ERROR] " << server_r.error() << "\n";
                return 1;
            }
        } else if (arg == "--input") {
            if (++i >= argc) {
                std::cerr << "[ERROR] --input requires value\n";
                return 1;
            }
            args.input = argv[i];
        } else if (arg == "--log") {
            if (++i >= argc) {
                std::cerr << "[ERROR] --log requires value\n";
                return 1;
            }
            args.log = argv[i];
        } else {
            std::cerr << "[ERROR] unknown option (" << arg << ")\n";
            return 1;
        }
    }

    auto sp_r = spawn_ffmpeg(args.video);

    if (!sp_r) {
        std::cerr << sp_r.error();
        return 1;
    }

    auto& sp = *sp_r;
    std::cerr << "[INFO] ffmpeg spawned, pid=" << sp.pid() << "\n";

    ptank::Fd sock{};
    if (!args.server_host.empty()) {
        auto sock_r = open_connect(args.server_host, args.server_port);
        if (sock_r) {
            sock = std::move(*sock_r);
            std::cerr << "[INFO] telemetry connected " << args.server_host
                      << ":" << args.server_port << "\n";
        } else {
            std::cerr << sock_r.error() << "\n";
        }
    }

    ptank::Fd evdev_fd;
    if (!args.input.empty()) {
        evdev_fd = ptank::Fd(::open(args.input.c_str(), O_RDONLY | O_NONBLOCK));
        if (!evdev_fd.valid()) {
            std::cerr << "[ERROR] evdev open failed: " << args.input
                      << " (" << ::strerror(errno) << ")\n";
            return 1;
        }
        std::cerr << "[INFO] evdev opened: " << args.input
                  << " fd=" << evdev_fd.get() << "\n";
    }

    CsvLogger logger;
    if (!args.log.empty()) {
        if (auto r = logger.open(args.log); !r) {
            std::cerr << "[ERROR] " << r.error() << "\n";
            return 1;
        }
        std::cerr << "[INFO] logging to " << args.log << "\n";
    }

    RateMeter ui_rate{};
    RateMeter video_rate{};
    RateMeter telem_rate{};
    std::uint64_t ui_frames = 0;

    CaptureState st;
    std::uint64_t last_seen = 0;

    std::thread t(capture_loop, sp.fd(), std::ref(st));

    TelemetryState telem_st;
    std::thread telem_t;
    std::uint64_t telem_last_seen = 0;
    ptank::FrameEngine f_engine{};

    std::atomic<bool> ui_running{true};
    Keys keys;
    std::thread evdev_thread;
    std::thread cmd_thread;
    DriveState drv_st{};

    if (evdev_fd.valid())
        evdev_thread = std::thread(evdev_loop, evdev_fd.get(), std::ref(keys), std::ref(ui_running));


    if (sock.valid()) {
        telem_t = std::thread(telemetry_loop, sock.get(), std::ref(telem_st), std::ref(logger), std::ref(drv_st));
        cmd_thread = std::thread(cmd_send_loop, sock.get(), std::ref(drv_st), std::ref(keys), std::ref(ui_running));
    } else {
        telem_st.running.store(false);
    }

    const std::string WIN = "pippitank-ui";
    cv::namedWindow(WIN, cv::WINDOW_AUTOSIZE);
    cv::Mat canvas(CANVAS_H, CANVAS_W, CV_8UC3, cv::Scalar(30, 30, 30));

    canvas.setTo(cv::Scalar(30, 30, 30));
    cv::line(canvas, {CAMERA_X, 0}, {CAMERA_X, CAMERA_H}, {80, 80, 80}, 1);
    cv::line(canvas, {0, CAMERA_H}, {CANVAS_W, CAMERA_H}, {80, 80, 80}, 1);

    while (ui_running.load()) {


        if (st.running.load()) {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lk(st.mtx);
                uint64_t cur = st.seq.load(std::memory_order_acquire);
                if (cur != last_seen && !st.latest.empty()) {
                    frame = st.latest;
                    last_seen = cur;
                    video_rate.update(cur);
                }
            }

            if (!frame.empty()) {
                frame.copyTo(canvas(cv::Rect(CAMERA_X, CAMERA_Y, CAMERA_W, CAMERA_H)));
                draw_telem_line(canvas, "VIDEO FPS: " + video_rate.to_string(), {20, 65});
            }
        }

        if (telem_st.running.load()) {
            {
                std::lock_guard<std::mutex> lk(telem_st.mtx);
                std::uint64_t cur = telem_st.seq.load(std::memory_order_acquire);
                if (cur != telem_last_seen) {
                    f_engine = telem_st.latest;
                    telem_last_seen = cur;
                    telem_rate.update(cur);
                }
            }


            if (telem_last_seen > 0) {
                auto t = ptank::to_telemetry(f_engine);

                draw_telem_line(canvas,
                        "TELEM FPS: " + telem_rate.to_string(), {20, 90});
                draw_telem_line(canvas,
                        "CELL#1(V): " + telem_fmt_v(t.v_cell_1), {20, 125});
                draw_telem_line(canvas,
                        "BAT(V): " + telem_fmt_v(t.v_bat), {20, 150});
                draw_telem_line(canvas,
                        "Q(V): " + telem_fmt_v(t.v_q), {20, 175});
                draw_telem_line(canvas,
                        "ESC#1(A): " + telem_fmt_a(t.i_esc_1), {20, 200});
                draw_telem_line(canvas,
                        "ESC#2(A): " + telem_fmt_a(t.i_esc_2), {20, 225});
                draw_telem_line(canvas,
                        "SYS(A): " + telem_fmt_a(t.i_sys), {20, 250});
                draw_telem_line(canvas,
                        "TEMP(C): " + telem_fmt_temp(t.temp_c), {20, 275});
                draw_telem_line(canvas,
                        "UP TIME(s): " + telem_fmt_ms(t.uptime_ms), {20, 300});
            }

        }

        ui_rate.update(++ui_frames);
        draw_telem_line(canvas, "UI FPS: " + ui_rate.to_string(), {20, 40});

        draw_drive_bars(canvas, drv_st.l.load(std::memory_order_acquire), drv_st.r.load(std::memory_order_acquire));

        cv::imshow(WIN, canvas);

        if ((cv::waitKey(16) & 0xFF) == 'q') {
            ui_running.store(false);
            st.running.store(false);
            telem_st.running.store(false);
            ::kill(sp.pid(), SIGKILL);
            if (sock.valid())
                ::shutdown(sock.get(), SHUT_RDWR);
            break;
        }

    }

    t.join();

    if (telem_t.joinable())
        telem_t.join();
    if (cmd_thread.joinable())
        cmd_thread.join();
    if (evdev_thread.joinable())
        evdev_thread.join();

    return 0;
}
