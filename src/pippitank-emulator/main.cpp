#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include "pippitank-protocol.hpp"
#include "fd.hpp"

namespace {

ptank::EnginePayload build_payload(std::uint32_t tick) noexcept {
    ptank::EnginePayload p{};
    p.uptime_ms    = tick * 20;
    p.adc_i_esc_1  = static_cast<std::uint16_t>(tick);
    p.adc_i_esc_2  = static_cast<std::uint16_t>(tick + 10);
    p.adc_i_sys    = static_cast<std::uint16_t>(tick + 20);
    p.adc_v_cell_1 = static_cast<std::uint16_t>(tick + 30);
    p.adc_v_cell_2 = static_cast<std::uint16_t>(tick + 40);
    p.adc_v_q      = static_cast<std::uint16_t>(tick + 50);
    p.adc_temp     = static_cast<std::uint16_t>(tick + 60);
    return p;
}

}

// Tips#1: socat -d -d pty,raw,echo=0 pty,raw,echo=0
// Tips#2: xxd -c 23 < /dev/pts/#
int main(int argc, char* argv[]) {
    if (argc >= 3 && std::string_view(argv[1]) == "--dump-fixture") {
        auto count = static_cast<std::uint32_t>(std::atoi(argv[2]));
        for (std::uint32_t i = 0; i < count; i++) {
            ptank::FrameEngine f{};
            auto p = build_payload(i);
            ptank::encode(f, p);

            ssize_t n = ::write(STDOUT_FILENO, &f, sizeof(f));

            if (n != static_cast<ssize_t>(sizeof(f))) {
                std::cerr << "write failed: n=" << n << " errno=" << strerror(errno) << "\n";
                return 1;
            }
        }
        return 0;
    }

    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <pty>\n";
        return 1;
    }

    const ptank::Fd pty(::open(argv[1], O_RDWR | O_NOCTTY));

    if (!pty.valid()) {
        std::cerr << "open failed: " << strerror(errno) << "\n";
        return 1;
    }

    struct termios termios_options;

    if (tcgetattr(pty.get(), &termios_options) < 0) {
        std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
        return 1;
    }

    cfsetispeed(&termios_options, B19200);
    cfsetospeed(&termios_options, B19200);
    cfmakeraw(&termios_options);

    if (tcsetattr(pty.get(), TCSADRAIN, &termios_options) < 0) {
        std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
        return 1;
    }

    using namespace std::chrono;

    auto start = steady_clock::now();
    auto next_tick = start + 20ms;

    while (true) {
        auto elapsed_ms = duration_cast<milliseconds>(steady_clock::now() - start).count();
        auto tick       = static_cast<std::uint32_t>(elapsed_ms / 20);

        ptank::FrameEngine f{};
        auto p = build_payload(tick);
        ptank::encode(f, p);

        ssize_t n = ::write(pty.get(), &f, sizeof(f));

        if (n != static_cast<ssize_t>(sizeof(f))) {
            std::cerr << "write failed: n=" << n << " errno=" << strerror(errno) << "\n";
            break;
        }

        std::this_thread::sleep_until(next_tick);
        next_tick += 20ms;
    }

    return 0;
}
