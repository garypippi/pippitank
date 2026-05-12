#pragma once
#include <stdint.h>
#include <math.h>

// VREF
#define VREF_INTERNAL  1.1f      // 内部 1.1V VREF
#define VREF_VDD       5.0f      // VDD VREF (温度ch のみ)
#define ADC_FS         1023.0f   // 10-bit

// 電圧分圧 (R1=67kΩ, R2=10kΩ)
#define VDIV_RATIO     ((67.0f + 10.0f) / 10.0f)   // = 7.7

// LT6106 利得 (Av = ROUT / RIN)
#define AV_ESC         (4700.0f / 240.0f)          // ≈ 19.58
#define AV_SYS         (2400.0f / 240.0f)          // ≈ 10.0

// シャント抵抗 (並列後)
#define R_SHUNT_ESC    (0.007f / 4.0f)             // 7mΩ × 4 並列 = 1.75mΩ
#define R_SHUNT_SYS    (0.040f / 2.0f)             // 40mΩ × 2 並列 = 20mΩ

static inline float adc_to_v_bat(uint16_t adc) {
    return adc * VREF_INTERNAL / ADC_FS * VDIV_RATIO;
}
static inline float adc_to_v_q(uint16_t adc) {
    return adc * VREF_INTERNAL / ADC_FS * VDIV_RATIO;
}
static inline float adc_to_v_cell1(uint16_t adc) {
    // セル1 単体は分圧比違うかも、要 schematic 確認 (R1=47kΩ 案あり)
    return adc * VREF_INTERNAL / ADC_FS * (47.0f + 10.0f) / 10.0f;
}
static inline float adc_to_i_esc(uint16_t adc) {
    float vsense = adc * VREF_INTERNAL / ADC_FS / AV_ESC;   // V across shunt
    return vsense / R_SHUNT_ESC;                            // I = V / R
}
static inline float adc_to_i_sys(uint16_t adc) {
    float vsense = adc * VREF_INTERNAL / ADC_FS / AV_SYS;
    return vsense / R_SHUNT_SYS;
}
// 温度: NTC は非線形、近似式 or 簡易テーブル
static inline float adc_to_temp_c(uint16_t adc) {
    // VDD=5V, R_pu=47kΩ, NTC 47kΩ B=4050K
    // V_ntc = adc * 5.0 / 1023
    // R_ntc = R_pu * V_ntc / (5 - V_ntc)
    // T(K) = 1 / (1/T0 + ln(R/R0)/B), T0=298.15K (25°C), R0=47k, B=4050
    float v_ntc = adc * VREF_VDD / ADC_FS;
    if (v_ntc >= VREF_VDD - 0.001f) return -273.15f;   // 安全側
    float r_ntc = 47000.0f * v_ntc / (VREF_VDD - v_ntc);
    float lnratio = std::log(r_ntc / 47000.0f);
    float t_kelvin = 1.0f / (1.0f / 298.15f + lnratio / 4050.0f);
    return t_kelvin - 273.15f;
}
