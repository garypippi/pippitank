#include "pippitank-protocol.hpp"
#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <fixture.bin>\n";
        return 2;
    }

    std::FILE* fp = std::fopen(argv[1], "rb");
    if (!fp) {
        std::perror("fopen");
        return 2;
    }

    std::vector<std::uint8_t> data;
    std::uint8_t b;
    while (std::fread(&b, 1, 1, fp))
        data.push_back(b);

    std::fclose(fp);

    ptank::Receiver rx;
    int frames = 0;
    int fail = 0;

    for (std::uint8_t byte : data) {
        auto span = rx.feed(byte);
        if (!span) continue;

        auto dec = ptank::decode_engine(span->first, span->second);
        if (!dec) {
            std::cerr << "frame: " << frames << " decode error " << static_cast<int>(dec.error());
            ++fail;
            ++frames;
            continue;
        }

        ptank::FrameEngine re{};
        ptank::encode(re, dec->payload);
        if (std::memcmp(&re, span->first, sizeof(ptank::FrameEngine)) != 0) {
            std::cerr << "frame " << frames << " re-encode mismatch \n";
            ++fail;
        }
        ++frames;
    }

    std::cout << "decoded " << frames << " frames, " << fail << " failures.\n";

    return (frames == 50 && fail == 0) ? 0 : 1;
}
