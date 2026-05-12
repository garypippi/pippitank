#pragma once
#include <stdint.h>
#define PT_SYNC 0xA5
#define PT_PAYLOAD_MAX 64

enum pt_src {
    PT_SRC_ENGINE     = 0x01,
    PT_SRC_CHASSIS    = 0x02,
    PT_SRC_TURRET     = 0x03,
    PT_SRC_RPI        = 0x10,
};

enum pt_typ {
    PT_TYP_TELEMETORY = 0x01,
    PT_TYP_EVENT      = 0x02,
    PT_TYP_COMMAND    = 0x03,
    PT_TYP_ACK        = 0x04,
    PT_TYP_CMD_DRIVE  = 0x10,
};


struct __attribute__((__packed__)) pt_header {
    uint8_t syn;
    uint8_t src;
    uint8_t typ;
    uint8_t len;
};

struct __attribute__((__packed__)) pt_payload_engine {
    uint32_t uptime_ms;
    uint16_t adc_i_esc_1;
    uint16_t adc_i_esc_2;
    uint16_t adc_i_sys;
    uint16_t adc_v_cell_1;
    uint16_t adc_v_cell_2;
    uint16_t adc_v_q;
    uint16_t adc_temp;
};

struct __attribute__((__packed__)) pt_frame_engine {
    struct pt_header         header;
    struct pt_payload_engine payload;
    uint8_t                      crc;
};


struct __attribute__((__packed__)) pt_payload_chassis {
    uint32_t uptime_ms;
    uint32_t engine_frames_ok;     // engine から受信成功した累積フレーム数
    uint32_t engine_frames_dropped; // CRC NG / overflow 等で捨てた累積数
    uint8_t  flags;                // 予約 (PWR_EN 状態反映等、後で)
};   // 13B

struct __attribute__((__packed__)) pt_frame_chassis {
    struct pt_header          header;
    struct pt_payload_chassis payload;
    uint8_t                   crc;
};   // 4 + 13 + 1 = 18B

struct __attribute__((__packed__)) pt_payload_cmd_drive {
    int16_t throttle_l;
    int16_t throttle_r;
};

struct __attribute__((__packed__)) pt_frame_cmd_drive {
    struct pt_header             header;
    struct pt_payload_cmd_drive  payload;
    uint8_t                      crc;
};

static inline uint8_t pt_crc8(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < len; i++) {
      crc ^= data[i];
      for (uint8_t b = 0; b < 8; b++) {
          crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
      }
  }
  return crc;
}

// 受信ステートマシン用
  enum pt_rx_state {
      PT_RX_WAIT_SYNC,
      PT_RX_READ_HEADER,
      PT_RX_READ_PAYLOAD,
      PT_RX_READ_CRC
  };

  struct pt_rx {
      enum pt_rx_state state;
      uint8_t  buf[sizeof(struct pt_header) + PT_PAYLOAD_MAX + 1];
      uint16_t pos;
      uint16_t len;
      int      done;
  };

  static inline void pt_rx_init(struct pt_rx *rx)
  {
      rx->state = PT_RX_WAIT_SYNC;
      rx->pos   = 0;
      rx->len   = 0;
      rx->done  = 0;
  }

  static inline void pt_rx_feed(struct pt_rx *rx, uint8_t data)
  {
      switch (rx->state)
      {
          case PT_RX_WAIT_SYNC:
              if (data == PT_SYNC) {
                  rx->state = PT_RX_READ_HEADER;
                  rx->buf[rx->pos++] = data;
                  rx->len = sizeof(struct pt_header) - 1;
              }
              break;

          case PT_RX_READ_HEADER:
              rx->buf[rx->pos++] = data;
              rx->len--;
              if (rx->len == 0) {
                  uint8_t payload_len = rx->buf[3];
                  if (payload_len > PT_PAYLOAD_MAX) {
                      rx->state = PT_RX_WAIT_SYNC;
                      rx->pos   = 0;
                  } else {
                      rx->state = PT_RX_READ_PAYLOAD;
                      rx->len   = payload_len;
                      if (payload_len == 0) {
                          rx->state = PT_RX_READ_CRC;
                      }
                  }
              }
              break;

          case PT_RX_READ_PAYLOAD:
              rx->buf[rx->pos++] = data;
              rx->len--;
              if (rx->len == 0) {
                  rx->state = PT_RX_READ_CRC;
              }
              break;

          case PT_RX_READ_CRC: {
              uint8_t expected = pt_crc8(rx->buf + 1, 3 + rx->buf[3]);
              if (expected == data) {
                  rx->buf[rx->pos] = data;
                  rx->done = 1;
              }
              rx->state = PT_RX_WAIT_SYNC;
              rx->pos   = 0;
              break;
          }
      }
  }
