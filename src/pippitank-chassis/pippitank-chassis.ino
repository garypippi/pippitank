//const uint32_t BAUDRATE = 19200;
//uint32_t hb_last_ms = 0;
//uint32_t hb_state   = false;
//
//void setup() {
//  // RPi
//  Serial1.setTX(0);
//  Serial1.setRX(1);
//  Serial1.begin(BAUDRATE);
//
//  // pippitank-engine
//  Serial2.setTX(4);
//  Serial2.setRX(5);
//  Serial2.begin(BAUDRATE);
//  //Serial1.begin(BAUDRATE); // RPi
//  //Serial2.begin(BAUDRATE); // pippitank-engine
//  pinMode(LED_BUILTIN, OUTPUT);
//}
//
//void loop() {
//  // Pass through pippitank-engine's telemetory
//  if (Serial2.available()) {
//    Serial1.write(Serial2.read());
//  }
//
//  // Heart beat
//  uint32_t now = millis();
//  if (now - hb_last_ms >= 500) {
//    hb_last_ms = now;
//    hb_state = !hb_state;
//    digitalWrite(LED_BUILTIN, hb_state);
//  }
//}


#include <Servo.h>
#include "./protocol.h"

const uint8_t PIN_ESC_1 = 6;   // 適宜
const uint8_t PIN_ESC_2 = 7;

const int PWM_MIN  = 1000;
const int PWM_MAX  = 2000;
const int PWM_IDLE = 1500;

Servo esc_l;
Servo esc_r;

const uint32_t BAUDRATE = 19200;
const uint8_t  LED_PIN  = LED_BUILTIN;

uint32_t led_last_ms  = 0;
uint32_t stat_last_ms = 0;
bool     led_state    = false;

uint32_t engine_frames_ok  = 0;
uint32_t engine_frames_err = 0;

struct pt_rx rx_engine;
struct pt_rx rx_cmd;

const uint32_t ESC_ARM_DELAY_MS = 2000;
bool esc_armed = false;

const uint32_t CHASSIS_TX_PERIOD_MS = 33;   // 30Hz
uint32_t chassis_tx_last_ms = 0;

volatile int16_t  target_throttle_l = 0;
volatile int16_t  target_throttle_r = 0;
volatile uint32_t last_cmd_ms       = 0;

const uint32_t ESC_UPDATE_MS = 20;   // 50Hz
const uint32_t CMD_TIMEOUT_MS = 500;
uint32_t esc_update_last = 0;

//void handle_cmd(const struct pt_rx *rx) {
//    const struct pt_header *hdr = (const struct pt_header *)rx->buf;
//    if (hdr->typ == PT_TYP_CMD_DRIVE
//        && hdr->len == sizeof(struct pt_payload_cmd_drive)) {
//        const struct pt_frame_cmd_drive *f = (const struct pt_frame_cmd_drive *)rx->buf;
//        target_throttle_l = f->payload.throttle_l;
//        target_throttle_r = f->payload.throttle_r;
//        last_cmd_ms = millis();
//          Serial.printf("CMD_DRIVE L=%d R=%d (count=%u)\n",
//                last_throttle_l, last_throttle_r, cmd_count);
//    }
//}
uint32_t cmd_count = 0;
//int16_t  last_throttle_l = 0;
//int16_t  last_throttle_r = 0;
void handle_cmd(const struct pt_rx *rx) {
  const struct pt_header *hdr = (const struct pt_header *)rx->buf;

  if (hdr->typ == PT_TYP_CMD_DRIVE
      && hdr->len == sizeof(struct pt_payload_cmd_drive))
  {
      const struct pt_frame_cmd_drive *f = (const struct pt_frame_cmd_drive *)rx->buf;
      target_throttle_l = f->payload.throttle_l;
      target_throttle_r = f->payload.throttle_r;
      last_cmd_ms = millis();
      cmd_count++;
      Serial.printf("CMD_DRIVE L=%d R=%d (count=%u)\n",
                    target_throttle_l, target_throttle_r, cmd_count);
  }
}

void send_chassis_telemetry() {
     struct pt_frame_chassis frame;
     frame.header.syn = PT_SYNC;
     frame.header.src = PT_SRC_CHASSIS;
     frame.header.typ = PT_TYP_TELEMETORY;
     frame.header.len = sizeof(frame.payload);

     frame.payload.uptime_ms = millis();
     frame.payload.engine_frames_ok = engine_frames_ok;
     frame.payload.engine_frames_dropped = 0;   // 実装は後で
     frame.payload.flags = 0;

     frame.crc = pt_crc8(&frame.header.src, 3 + sizeof(frame.payload));

     Serial1.write((const uint8_t*)&frame, sizeof(frame));
}

int throttle_to_pwm(int16_t t) {
      if (t > 1000)  t = 1000;
      if (t < -1000) t = -1000;
      return PWM_IDLE + t / 2;   // -1000 → 1000us, 0 → 1500us, +1000 → 2000us
}

void setup() {
    Serial.begin(115200);

    Serial1.setTX(0);
    Serial1.setRX(1);
    Serial1.begin(BAUDRATE);

    Serial2.setTX(4);
    Serial2.setRX(5);
    Serial2.begin(BAUDRATE);

    pinMode(LED_PIN, OUTPUT);

    esc_l.attach(PIN_ESC_1, PWM_MIN, PWM_MAX);
    esc_r.attach(PIN_ESC_2, PWM_MIN, PWM_MAX);
    esc_l.writeMicroseconds(PWM_IDLE);
    esc_r.writeMicroseconds(PWM_IDLE);

    pt_rx_init(&rx_engine);
}

void loop() {
    // engine からの受信 → state machine へ
    while (Serial2.available()) {
        uint8_t b = Serial2.read();
        pt_rx_feed(&rx_engine, b);

        if (rx_engine.done == 1) {
            rx_engine.done = 0;
            // フレーム完成 → そのまま RPi へ
            // 全長 = sizeof(pt_header) + payload_len + 1(CRC)
            size_t frame_len = sizeof(struct pt_header) + rx_engine.buf[3] + 1;
            Serial1.write(rx_engine.buf, frame_len);
            engine_frames_ok++;
        }
        // CRC エラーは pt_rx_feed 内で done=0 のまま捨てられる
        // → ここでは err カウントできない、要なら state machine 側に追加
    }

    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        pt_rx_feed(&rx_cmd, b);
        if (rx_cmd.done == 1) {
            rx_cmd.done = 0;
            handle_cmd(&rx_cmd);
        }
    }

    uint32_t now = millis();

    // chassis 自前テレメトリ
    if (now - chassis_tx_last_ms >= CHASSIS_TX_PERIOD_MS) {
        chassis_tx_last_ms = now;
        send_chassis_telemetry();
    }

    if (!esc_armed && now >= ESC_ARM_DELAY_MS) {
        esc_armed = true;
    }

    if (now - last_cmd_ms > CMD_TIMEOUT_MS) {
      target_throttle_l = 0;
      target_throttle_r = 0;
    }

    if (now - esc_update_last >= ESC_UPDATE_MS) {
         esc_update_last = now;
         if (esc_armed) {
             Serial.printf("ESC %d %d \n", throttle_to_pwm(target_throttle_l), throttle_to_pwm(target_throttle_r));
             esc_l.writeMicroseconds(throttle_to_pwm(target_throttle_l));
             esc_r.writeMicroseconds(throttle_to_pwm(target_throttle_r));
         }
    }

    // ハートビート LED
    if (now - led_last_ms >= 500) {
        led_last_ms = now;
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state);
    }
    // 統計
    if (now - stat_last_ms >= 1000) {
        stat_last_ms = now;
        Serial.printf("engine_ok=%u\n", engine_frames_ok);
    }
}
