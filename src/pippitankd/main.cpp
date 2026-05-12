#include <iostream>
#include <optional>
#include <string>
#include <cstring>
#include <cstdint>
#include <string_view>
#include <charconv>
#include <system_error>
#include <memory>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <netdb.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <tl/expected.hpp>
#include <iomanip>
#include "fd.hpp"

namespace {

constexpr std::size_t POLL_SERVER = 0;
constexpr std::size_t POLL_SERIAL = 1;
constexpr std::size_t POLL_CLIENT = 2;
constexpr std::size_t POLL_MAX    = 3;

struct Args {
    std::optional<std::string> serial;
    std::string                listen_host;
    std::uint16_t              listen_port = 0;
};

tl::expected<void, std::string>
parse_listen(std::string_view spec, Args& args) {
    auto col = spec.find(':');
    if (col == std::string_view::npos)
        return tl::make_unexpected("--listen: missing ':'");

    auto host_sv = spec.substr(0, col);
    auto port_sv = spec.substr(col + 1);

    if (host_sv.empty()) return tl::make_unexpected("--listen: empty host");
    if (port_sv.empty()) return tl::make_unexpected("--listen: empty port");

    std::uint16_t port = 0;

    auto [ptr, ec] = std::from_chars(
        port_sv.data(), port_sv.data() + port_sv.size(), port);

    if (ec != std::errc{} || ptr != port_sv.data() + port_sv.size())
        return tl::make_unexpected("--listen: bad port: " + std::string(port_sv));

    args.listen_host = std::string(host_sv);
    args.listen_port = port;

    return {};
}

tl::expected<Args, std::string>
parse_args(int argc, char* argv[]) {
    Args args{};
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "--serial") {
            if (++i >= argc) return tl::make_unexpected("--serial requires value");
            args.serial = argv[i];
        } else if (arg == "--listen") {
            if (++i >= argc) return tl::make_unexpected("--listen requires value");
            if (auto listen_r = parse_listen(argv[i], args); !listen_r)
                return tl::make_unexpected(listen_r.error());
        } else {
            return tl::make_unexpected(std::string("unknown option: ") + std::string(arg));
        }
    }
    return args;
}

tl::expected<ptank::Fd, std::string>
open_serial(const std::string& path) {
    ptank::Fd tty(::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK));
    if (!tty.valid())
        return tl::make_unexpected("open failed: " + std::string(::strerror(errno)));
    ::termios opts{};
    if (::tcgetattr(tty.get(), &opts))
        return tl::make_unexpected("tcgetattr failed: " + std::string(::strerror(errno)));
    ::cfsetispeed(&opts, B19200);
    ::cfsetospeed(&opts, B19200);
    ::cfmakeraw(&opts);
    if (::tcsetattr(tty.get(), TCSADRAIN, &opts) < 0)
        return tl::make_unexpected("tcsetattr failed: " + std::string(::strerror(errno)));
    return tty;
}

using AddrInfoPtr = std::unique_ptr<::addrinfo, decltype(&::freeaddrinfo)>;

tl::expected<ptank::Fd, std::string>
open_listen(const std::string& host, std::uint16_t port) {
    ::addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    ::addrinfo* raw = nullptr;
    int rc = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &raw);
    if (rc != 0)
        return tl::make_unexpected("getaddrinfo failed: " + std::string(::gai_strerror(rc)));

    AddrInfoPtr res(raw, &::freeaddrinfo);

    ptank::Fd sock(::socket(res->ai_family, res->ai_socktype, res->ai_protocol));
    if (!sock.valid())
        return tl::make_unexpected("socket failed: " + std::string(::strerror(errno)));

    int yes = 1;
    if (::setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        return tl::make_unexpected("setsockopt failed: " + std::string(::strerror(errno)));
    if (::bind(sock.get(), res->ai_addr, res->ai_addrlen) < 0)
        return tl::make_unexpected("bind failed: " + std::string(::strerror(errno)));
    if (::listen(sock.get(), 5) < 0)
        return tl::make_unexpected("listen failed: " + std::string(::strerror(errno)));

    return sock;
}

}

int main(int argc, char* argv[]) {
    auto args_r = parse_args(argc, argv);
    if (!args_r) { std::cerr << args_r.error() << "\n"; return 1; }
    const auto& args = *args_r;

    ptank::Fd tty;
    if (args.serial) {
        auto tty_r = open_serial(*args.serial);
        if (!tty_r) { std::cerr << tty_r.error(); return 1; }
        tty = std::move(*tty_r);
    }

    auto svr_r = open_listen(args.listen_host, args.listen_port);
    if (!svr_r) { std::cerr << svr_r.error() << "\n"; return 1; }
    ptank::Fd svr = std::move(*svr_r);

    ::pollfd fds[POLL_MAX] = {};

    fds[POLL_SERVER].fd     = svr.get();
    fds[POLL_SERVER].events = POLLIN;
    fds[POLL_SERIAL].fd     = tty.get();
    fds[POLL_SERIAL].events = POLLIN;
    fds[POLL_CLIENT].fd     = -1;
    fds[POLL_CLIENT].events = POLLIN;

    ptank::Fd client;
    std::size_t nfds = POLL_MAX;

    while (true) {
        int rc = ::poll(fds, nfds, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            std::cerr << "poll failed: " << ::strerror(errno) << "\n";
            return 1;
        }
        if (fds[POLL_SERVER].revents & POLLIN) {
            ptank::Fd new_client(::accept(svr.get(), nullptr, nullptr));
            if (!new_client.valid()) {
                if (errno != EAGAIN && errno != ECONNABORTED && errno != EINTR) {
                    std::cerr << "accept failed: " << ::strerror(errno) << "\n";
                    return 1;
                }
            } else if (client.valid()) {
                std::cerr << "[INFO] Reject: client already connected.\n";
            } else {
                int yes = 1;
                ::setsockopt(new_client.get(), IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
                ::setsockopt(new_client.get(), SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
                ::fcntl(new_client.get(), F_SETFL, O_NONBLOCK);
                client = std::move(new_client);
                fds[POLL_CLIENT].fd = client.get();
                std::cerr << "[INFO] client connected: fd=" << client.get() << "\n";
            }
        }
        if (fds[POLL_SERIAL].revents & POLLIN) {
            std::uint8_t buf[64];
            while (true) {
                ssize_t n = ::read(tty.get(), buf, sizeof(buf));
                if (n > 0) {
                    //std::cerr << "[SERIAL]";
                    //std::cerr << std::hex << std::setfill('0');
                    //for (int i = 0; i < n; i++)
                    //    std::cerr << std::setw(2) << static_cast<unsigned>(buf[i]) << " ";
                    //std::cerr << std::dec << std::setfill(' ');
                    //std::cerr << "\n";

                    // Send to a client.
                    if (client.valid()) {
                        ssize_t remain = n;
                        ssize_t offset = 0;
                        while (remain > 0) {
                            ssize_t w = ::send(client.get(), buf + offset, remain, MSG_NOSIGNAL);
                            if (w > 0) {
                                offset += w;
                                remain -= w;
                                continue;
                            }
                            if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            }
                            client = ptank::Fd{};
                            fds[POLL_CLIENT].fd = -1;
                            std::cerr << "[INFO] client disconnected. (" << strerror(errno) << ")\n";
                            break;
                        }
                    }

                    continue;
                }
                if (n == 0) {
                    std::cerr << "[ERROR] Fatal: serial closed.\n";
                    return 1;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                std::cerr << "[ERROR] Fatal: serial read failed " << ::strerror(errno) << "\n";
                return 1;
            }
        }
        if (fds[POLL_CLIENT].revents & POLLIN) {
            std::uint8_t buf[64];
            while (true) {
                ssize_t n = ::recv(client.get(), buf, sizeof(buf), 0);
                if (n > 0) {
                    if (tty.valid()) {
                        ssize_t remain = n;
                        ssize_t offset = 0;
                        while (remain > 0) {
                            ssize_t w = ::write(tty.get(), buf + offset, remain);
                            if (w > 0) {
                                offset += w;
                                remain -= w;
                                continue;
                            }
                            if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            }
                            std::cerr << "[ERROR] serial write failed: " << ::strerror(errno) << "\n";
                            return 1;
                        }
                    }

                    continue;
                }
                if (n == 0) {
                    std::cerr << "[INFO] client disconnected (EOF)\n";
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    std::cerr << "[ERROR] client read failed\n" << ::strerror(errno) << "\n";
                }

                client = ptank::Fd();
                fds[POLL_CLIENT].fd = -1;
                break;
            }
        }
    }
}
