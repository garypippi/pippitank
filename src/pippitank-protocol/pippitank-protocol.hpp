#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <tl/expected.hpp>
#include <utility>

namespace ptank {

inline constexpr std::uint8_t SYNC        = 0xA5;
inline constexpr std::size_t  PAYLOAD_MAX = 64;

enum class Source : std::uint8_t {
    Engine   = 0x01,
    Chassis  = 0x02,
    Turret   = 0x03,
    Rpi      = 0x10,
};

enum class Kind : std::uint8_t {
    Telemetry = 0x01,
    Event     = 0x02,
    Command   = 0x03,
    Ack       = 0x04,
    CmdDrive  = 0x10,
};

enum class DecodeError : std::uint8_t {
    ShortFrame,
    BadSync,
    BadCrc,
    BadLength,
};

struct __attribute__((packed)) Header {
    std::uint8_t syn;
    std::uint8_t source;
    std::uint8_t kind;
    std::uint8_t length;
};

struct __attribute__((packed)) EnginePayload {
    std::uint32_t uptime_ms;
    std::uint16_t adc_i_esc_1;
    std::uint16_t adc_i_esc_2;
    std::uint16_t adc_i_sys;
    std::uint16_t adc_v_cell_1;
    std::uint16_t adc_v_cell_2;
    std::uint16_t adc_v_q;
    std::uint16_t adc_temp;
};

struct __attribute__((packed)) FrameEngine {
    Header        header;
    EnginePayload payload;
    std::uint8_t  crc;
};

struct __attribute__((packed)) CmdDrivePayload {
    std::int16_t throttle_l;
    std::int16_t throttle_r;
};

struct __attribute__((packed)) FrameCmdDrive {
    Header          header;
    CmdDrivePayload payload;
    std::uint8_t    crc;
};

static_assert(sizeof(Header)          == 4);
static_assert(sizeof(EnginePayload)   == 4 + 7 * 2); // 18
static_assert(sizeof(FrameEngine)     == 4 + 18 + 1);
static_assert(sizeof(CmdDrivePayload) == 4);
static_assert(sizeof(FrameCmdDrive)   == 4 + 4 + 1);

constexpr std::uint8_t crc8(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint8_t crc = 0x00;
    for (std::size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x07) : static_cast<std::uint8_t>(crc << 1);
        }
    }
    return crc;
}

inline void encode(FrameEngine& f, const EnginePayload& p) noexcept {
    f.payload = p;
    f.header.syn = SYNC;
    f.header.source = static_cast<std::uint8_t>(Source::Engine);
    f.header.kind = static_cast<std::uint8_t>(Kind::Telemetry);
    f.header.length = sizeof(EnginePayload);
    f.crc = crc8(reinterpret_cast<const std::uint8_t*>(&f.header) + sizeof(f.header.syn),
            sizeof(Header) - sizeof(f.header.syn) + sizeof(f.payload));
}

inline void encode(FrameCmdDrive& f, const CmdDrivePayload& p) noexcept {
    f.payload = p;
    f.header.syn = SYNC;
    f.header.source = static_cast<std::uint8_t>(Source::Rpi);
    f.header.kind = static_cast<std::uint8_t>(Kind::CmdDrive);
    f.header.length = sizeof(CmdDrivePayload);
    f.crc = crc8(reinterpret_cast<const std::uint8_t*>(&f.header) + sizeof(f.header.syn),
            sizeof(Header) - sizeof(f.header.syn) + sizeof(f.payload));
}

inline tl::expected<FrameEngine,DecodeError>
decode_engine(const std::uint8_t* bytes, std::size_t length) noexcept {
    if (length < sizeof(FrameEngine))
        return tl::make_unexpected(DecodeError::ShortFrame);
    if (bytes[0] != SYNC)
        return tl::make_unexpected(DecodeError::BadSync);
    if (bytes[3] != sizeof(EnginePayload))
        return tl::make_unexpected(DecodeError::BadLength);

    const std::uint8_t crc =
        crc8(bytes + 1, sizeof(Header) - 1 + sizeof(EnginePayload));

    if (crc != bytes[sizeof(Header) + sizeof(EnginePayload)])
        return tl::make_unexpected(DecodeError::BadCrc);

    FrameEngine f{};
    std::memcpy(&f, bytes, sizeof(FrameEngine));
    return f;
}

class Receiver {
    public:
        Receiver() noexcept = default;

        inline std::optional<std::pair<const std::uint8_t*, std::size_t>>
        feed(std::uint8_t byte) noexcept {
            switch (state_) {
            case State::WaitSync:
                if (byte == SYNC) {
                    pos_ = 0;
                    buf_[pos_++] = byte;
                    remain_ = sizeof(Header) - 1;
                    state_ = State::ReadHeader;
                }
                break;
            case State::ReadHeader:
                buf_[pos_++] = byte;
                if (--remain_ == 0) {
                    const std::uint8_t payload_len = buf_[3];
                    if (payload_len > PAYLOAD_MAX) {
                        state_ = State::WaitSync;
                    } else if (payload_len == 0) {
                        state_ = State::ReadCrc;
                    } else {
                        remain_ = payload_len;
                        state_ = State::ReadPayload;
                    }
                }
                break;
            case State::ReadPayload:
                buf_[pos_++] = byte;
                if (--remain_ == 0)
                    state_ = State::ReadCrc;
                break;
            case State::ReadCrc:
                buf_[pos_++] = byte;
                state_ = State::WaitSync;
                return std::make_pair(static_cast<const std::uint8_t*>(buf_), pos_);
            }
            return std::nullopt;
        }

    private:
        enum class State : std::uint8_t { WaitSync, ReadHeader, ReadPayload, ReadCrc };
        State state_        = State::WaitSync;
        std::uint8_t buf_[sizeof(Header) + PAYLOAD_MAX + 1] = {};
        std::size_t pos_    = 0;
        std::size_t remain_ = 0;
};

}
