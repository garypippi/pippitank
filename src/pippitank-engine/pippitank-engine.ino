#include "./protocol.h"

const uint32_t BAUDRATE     = 19200;
const uint32_t TS_PERIOD_MS = 33;
uint32_t tx_last_ms         = 0;

const uint8_t PWR_EN_PIN       = PIN_PB0;
const uint8_t ADC_PIN_I_ESC_1  = A1;
const uint8_t ADC_PIN_I_ESC_2  = A6;
const uint8_t ADC_PIN_I_SYS    = A4;
const uint8_t ADC_PIN_V_CELL_1 = A7;
const uint8_t ADC_PIN_V_CELL_2 = A3;
const uint8_t ADC_PIN_V_Q      = A2;
const uint8_t ADC_PIN_TEMP     = A5;

const uint32_t POWER_ON_DELAY_MS = 5000;


void send_telemtory() {
  // Buidl pt_frame
  struct pt_frame_engine frame;
  // Build pt_header
  frame.header.syn = PT_SYNC;
  frame.header.src = PT_SRC_ENGINE;
  frame.header.typ = PT_TYP_TELEMETORY;
  frame.header.len = sizeof(frame.payload);
  // Build payloads
  frame.payload.uptime_ms = millis();
  // Read all adc's
  analogReference(VDD);
  delayMicroseconds(50);
  frame.payload.adc_temp = analogRead(ADC_PIN_TEMP);
  analogReference(INTERNAL1V1);
  delayMicroseconds(50);
  frame.payload.adc_i_esc_1  = analogRead(ADC_PIN_I_ESC_1);
  frame.payload.adc_i_esc_2  = analogRead(ADC_PIN_I_ESC_2);
  frame.payload.adc_i_sys    = analogRead(ADC_PIN_I_SYS);
  frame.payload.adc_v_cell_1 = analogRead(ADC_PIN_V_CELL_1);
  frame.payload.adc_v_cell_2 = analogRead(ADC_PIN_V_CELL_2);
  frame.payload.adc_v_q      = analogRead(ADC_PIN_V_Q);
  // CRC
  // 3 = src,typ,len
  frame.crc = pt_crc8(&frame.header.src, 3 + sizeof(frame.payload));

  Serial0.write((const uint8_t*)&frame, sizeof(frame));
}

void setup() {
  Serial0.begin(BAUDRATE);
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, LOW);
}

void loop() {
  uint32_t now = millis();
  static bool pwr_on = false;
  if (!pwr_on && now > POWER_ON_DELAY_MS) {
    pwr_on = true;
    digitalWrite(PWR_EN_PIN, HIGH);
  }
  if (now - tx_last_ms >= TS_PERIOD_MS) {
    tx_last_ms = now;
    send_telemtory();
  }
}
