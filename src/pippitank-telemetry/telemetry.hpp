#pragma once

#include <cmath>
#include <cstdint>
#include "pippitank-protocol.hpp"

namespace ptank{

// NOTE: wire field `adc_v_cell_2` は rev_a では実体が V_BAT (全体電圧).
// rev_b ではセル2絶対電圧を計測する設計予定なので、その時点で wire 名
// と実体が一致する。本ヘッダ側 (struct EngineTelemetry) は当面
// v_bat で公開し、rev_b 着手時に v_bat と v_cell_2 を分離する想定。

inline constexpr float VREF_INTERNAL = 1.1f;
inline constexpr float VREF_VDD      = 5.0f;
inline constexpr float ADC_FS        = 1023.0f;
inline constexpr float VDIV_BAT      = (67.0f + 10.0f) / 10.0f;
inline constexpr float VDIV_CELL_1   = (47.0f + 10.0f) / 10.0f;
inline constexpr float AV_ESC        = 4700.0f / 240.0f;
inline constexpr float AV_SYS        = 2400.0f / 240.0f;
inline constexpr float R_SHUNT_ESC   = 0.007f / 4.0f;
inline constexpr float R_SHUNT_SYS   = 0.040f / 2.0f;
inline constexpr float NTC_R_PU      = 47000.0f;
inline constexpr float NTC_R0        = 47000.0f;
inline constexpr float NTC_B         = 4050.0f;
inline constexpr float NTC_T0_K      = 298.15f;

struct EngineTelemetry {
    std::uint32_t uptime_ms;
    float v_cell_1;
    float v_bat;
    float v_q;
    float i_esc_1;
    float i_esc_2;
    float i_sys;
    float temp_c;
};

inline float to_temp(std::uint16_t adc_temp) noexcept {
    float v_ntc = adc_temp * VREF_VDD / ADC_FS;
    if (v_ntc >= VREF_VDD - 0.001f)
        return -273.15f;
    float r_ntc = NTC_R_PU * v_ntc / (VREF_VDD - v_ntc);
    float lnrat = std::log(r_ntc / NTC_R0);
    float t_k = 1.0f / (1.0f / NTC_T0_K + lnrat / NTC_B);
    return t_k - 273.15f;
}

inline EngineTelemetry to_telemetry(const FrameEngine& f) noexcept {
    EngineTelemetry t{};
    t.v_cell_1  = f.payload.adc_v_cell_1 * VREF_INTERNAL / ADC_FS * VDIV_CELL_1;
    t.v_bat     = f.payload.adc_v_cell_2 * VREF_INTERNAL / ADC_FS * VDIV_BAT;
    t.v_q       = f.payload.adc_v_q      * VREF_INTERNAL / ADC_FS * VDIV_BAT;
    t.i_esc_1   = (f.payload.adc_i_esc_1 * VREF_INTERNAL / ADC_FS / AV_ESC) / R_SHUNT_ESC;
    t.i_esc_2   = (f.payload.adc_i_esc_2 * VREF_INTERNAL / ADC_FS / AV_ESC) / R_SHUNT_ESC;
    t.i_sys     = (f.payload.adc_i_sys   * VREF_INTERNAL / ADC_FS / AV_SYS) / R_SHUNT_SYS;
    t.temp_c    = to_temp(f.payload.adc_temp);
    t.uptime_ms = f.payload.uptime_ms;
    return t;
}

}
