#include "../protocol.h"
#include "./conv.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/input.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFER_SIZE 128

// enum pt_rx_state {
//     WAIT_SYNC,
//     READ_HEADER,
//     READ_PAYLOAD,
//     READ_CRC
// };
//
// struct pt_rx_engine {
//     enum pt_rx_state state;
//     uint8_t buf[sizeof(struct pt_frame_engine) + 1];
//     uint16_t pos;
//     uint16_t len;
//     int done;
// };

void cleanup_ncurses(void) { endwin(); }

void on_sigint(int sig) {
  (void)sig;
  exit(0);
}

// y, x: 描画開始位置、width: バー幅(文字数)、value: 値、max: 最大値
void draw_bar(int y, int x, int width, float value, float max) {
  if (value < 0)
    value = 0;
  if (value > max)
    value = max;
  int filled = (int)(value * width / max);

  mvaddch(y, x, '[');
  for (int i = 0; i < width; i++) {
    mvaddch(y, x + 1 + i, i < filled ? '#' : '.');
  }
  mvaddch(y, x + 1 + width, ']');
}

// void pt_rx_engine_feed(struct pt_rx_engine* engine, uint8_t data)
//{
//     switch (engine->state)
//     {
//         case WAIT_SYNC:
//             if (data == PT_SYNC)
//             {
//                 engine->state = READ_HEADER;
//                 engine->buf[engine->pos++] = data;
//                 engine->len = sizeof(struct pt_header) - 1;
//             }
//             break;
//         case READ_HEADER:
//             engine->buf[engine->pos++] = data;
//             engine->len--;
//
//             if (engine->len == 0)
//             {
//                 engine->state = READ_PAYLOAD;
//                 engine->len = sizeof(struct pt_payload_engine);
//                 if (engine->buf[3] != sizeof(struct pt_payload_engine))
//                 {
//                     fprintf(stderr, "[ERROR] pt_header len mismatch.\n");
//                     engine->state = WAIT_SYNC;
//                     engine->pos = 0;
//                 }
//             }
//             break;
//         case READ_PAYLOAD:
//             engine->buf[engine->pos++] = data;
//             engine->len--;
//
//             if (engine->len == 0)
//             {
//                 engine->state = READ_CRC;
//             }
//             break;
//         case READ_CRC:
//             if (pt_crc8(engine->buf + 1, 3 + sizeof(struct
//             pt_payload_engine)) != data)
//             {
//                 fprintf(stderr, "[ERROR] CRC error\n");
//             }
//             else
//             {
//                 engine->done = 1;
//             }
//             engine->buf[engine->pos] = data;
//             engine->state = WAIT_SYNC;
//             engine->pos = 0;
//             break;
//     }
// }

struct app_state {
  struct pt_payload_engine engine_latest;
  bool engine_valid;
  struct pt_payload_chassis chassis_latest;
  bool chassis_valid;
  uint32_t engine_frame_count;
  uint32_t chassis_frame_count;
};

void render(const struct app_state *s) {
  erase();

  if (s->engine_valid) {
    const struct pt_payload_engine *e = &s->engine_latest;
    mvprintw(0, 0, "ENGINE  uptime %.1f s frames %u", e->uptime_ms / 1000.0f,
             s->engine_frame_count);

    mvprintw(2, 0, "V_BAT     %5.2f V", adc_to_v_bat(e->adc_v_cell_2));
    draw_bar(2, 24, 30, adc_to_v_bat(e->adc_v_cell_2), 8.4f);

    mvprintw(3, 0, "V_CELL1   %5.2f V", adc_to_v_cell1(e->adc_v_cell_1));
    draw_bar(3, 24, 30, adc_to_v_cell1(e->adc_v_cell_1), 4.2f);

    mvprintw(4, 0, "V_Q       %5.2f V", adc_to_v_q(e->adc_v_q));
    draw_bar(4, 24, 30, adc_to_v_q(e->adc_v_q), 8.4f);

    mvprintw(6, 0, "I_ESC1    %6.2f A", adc_to_i_esc(e->adc_i_esc_1));
    draw_bar(6, 24, 30, adc_to_i_esc(e->adc_i_esc_1), 30.0f);

    mvprintw(7, 0, "I_ESC2    %6.2f A", adc_to_i_esc(e->adc_i_esc_2));
    draw_bar(7, 24, 30, adc_to_i_esc(e->adc_i_esc_2), 30.0f);

    mvprintw(8, 0, "I_SYS     %6.2f A", adc_to_i_sys(e->adc_i_sys));
    draw_bar(8, 24, 30, adc_to_i_sys(e->adc_i_sys), 5.0f);

    mvprintw(10, 0, "TEMP     %5.1f C", adc_to_temp_c(e->adc_temp));
    draw_bar(10, 24, 30, adc_to_temp_c(e->adc_temp), 100.0f);
    // mvprintw(0, 0, "ENGINE  uptime %.1f s   frames %u",
    //          e->uptime_ms / 1000.0f, s->engine_frame_count);
    // mvprintw(2, 0, "V_BAT     %5.2f V", adc_to_v_bat(e->adc_v_cell_2));
    // draw_bar(2, 24, 30, adc_to_v_bat(e->adc_v_cell_2), 8.4f);
    //  ... 既存表示 ...
  } else {
    mvprintw(0, 0, "ENGINE  waiting...");
  }

  if (s->chassis_valid) {
    const struct pt_payload_chassis *c = &s->chassis_latest;
    mvprintw(13, 0, "CHASSIS uptime %.1f s   frames %u", c->uptime_ms / 1000.0f,
             s->chassis_frame_count);
    mvprintw(14, 0, "  engine_ok=%u  drop=%u  flags=0x%02X",
             c->engine_frames_ok, c->engine_frames_dropped, c->flags);
  } else {
    mvprintw(13, 0, "CHASSIS waiting...");
  }

  refresh();
}

struct keys {
  bool w, a, s, d;
};

float current_throttle_fwd = 0.0f;  // 前後 (-1000..+1000)
float current_throttle_turn = 0.0f; // 左右

const float RAMP_PER_TICK_FWD = 2.5f; // 50Hz × 50 = 2500/s → 0.4s で full
const float RAMP_PER_TICK_TURN = 2.5f;
const float DECAY_PER_TICK = 2.5f; // 解放時の戻り

void compute_target_and_ramp(const struct keys *k, int16_t *out_l,
                             int16_t *out_r) {
  // 目標値
  float target_fwd = 0.0f;
  if (k->w)
    target_fwd += 1000.0f;
  if (k->s)
    target_fwd -= 1000.0f;
  float target_turn = 0.0f;
  if (k->d)
    target_turn += 1000.0f;
  if (k->a)
    target_turn -= 1000.0f;

  // 前後ramp
  float diff_fwd = target_fwd - current_throttle_fwd;
  float step_fwd = (target_fwd != 0.0f) ? RAMP_PER_TICK_FWD : DECAY_PER_TICK;
  if (diff_fwd > step_fwd)
    current_throttle_fwd += step_fwd;
  else if (diff_fwd < -step_fwd)
    current_throttle_fwd -= step_fwd;
  else
    current_throttle_fwd = target_fwd;

  // 旋回 ramp
  float diff_turn = target_turn - current_throttle_turn;
  float step_turn = (target_turn != 0.0f) ? RAMP_PER_TICK_TURN : DECAY_PER_TICK;
  if (diff_turn > step_turn)
    current_throttle_turn += step_turn;
  else if (diff_turn < -step_turn)
    current_throttle_turn -= step_turn;
  else
    current_throttle_turn = target_turn;

  // 戦車差動: 左右 = fwd ± turn
  float l = current_throttle_fwd + current_throttle_turn;
  float r = current_throttle_fwd - current_throttle_turn;

  // クリップ
  if (l > 500.0f)
    l = 500.0f;
  if (l < -500.0f)
    l = -500.0f;
  if (r > 500.0f)
    r = 500.0f;
  if (r < -500.0f)
    r = -500.0f;

  *out_l = (int16_t)l;
  *out_r = (int16_t)r;
}

// A simple pippitankd client.
// Sends whatever you type, except "q" which quits the program.
int main(int argc, char *argv[]) {
  int listen_port = 0;
  char listen_addr[64] = "";

  int opt;
  struct option options[] = {
      {"listen", required_argument, NULL, 'L'},
      {0, 0, 0, 0},
  };

  while ((opt = getopt_long(argc, argv, ":s:L:", options, NULL)) != -1) {
    switch (opt) {
    case 'L':
      sscanf(optarg, "%63[^:]:%d", listen_addr, &listen_port);
      break;
    case ':':
      fprintf(stderr, "Option -%c requires argument.\n", optopt);
      return -1;
    case '?':
      fprintf(stderr, "Unknown option: -%c\n", optopt);
      return -1;
    }
  }

  int server_fd;
  struct sockaddr_in sock_addr;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("ERROR: socket()");
    return -1;
  }

  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = inet_addr(listen_addr);
  sock_addr.sin_port = htons(listen_port);

  if (connect(server_fd, (struct sockaddr *)&sock_addr,
              sizeof(struct sockaddr_in)) == -1) {
    perror("ERROR: connect()");
    return -1;
  }

  int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  struct itimerspec ts = {
      .it_interval = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000},
      .it_value = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000},
  };
  timerfd_settime(timer_fd, 0, &ts, NULL);

  int timer_tx_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  struct itimerspec ts_tx = {
      .it_interval = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000}, // 20ms = 50Hz
      .it_value = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000},
  };
  timerfd_settime(timer_tx_fd, 0, &ts_tx, NULL);

  int evdev_fd = open("/dev/input/event8", O_RDONLY); // X はキーボード
  if (evdev_fd < 0) {
    perror("[ERROR] evdev");
    return -1;
  }
  struct keys key = {0};

  struct pollfd poll_fds[4];

  poll_fds[0].fd = server_fd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = timer_fd;
  poll_fds[1].events = POLLIN;
  poll_fds[2].fd = evdev_fd;
  poll_fds[2].events = POLLIN;
  poll_fds[3].fd = timer_tx_fd;
  poll_fds[3].events = POLLIN;

  struct pt_rx pt_engine;
  pt_rx_init(&pt_engine);
  // pt_engine.state = PT_WAIT_SYNC;
  // pt_engine.pos = 0;
  // pt_engine.done = 0;

  // =================
  // ncurses
  // =================
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  atexit(cleanup_ncurses);
  signal(SIGINT, on_sigint);

  struct app_state state;

  while (1) {
    if (poll(poll_fds, 4, -1) < 0) {
      perror("[ERROR] poll()");
      return -1;
    }

    if (poll_fds[0].revents & POLLIN) {
      char buffer[64];
      int n = read(poll_fds[0].fd, buffer, sizeof(buffer));

      if (n > 0) {
        for (int i = 0; i < n; i++) {
          // pt_rx_engine_feed(&pt_engine, buffer[i]);
          pt_rx_feed(&pt_engine, buffer[i]);
          if (pt_engine.done == 1) {
            pt_engine.done = 0;
            const struct pt_header *header =
                (const struct pt_header *)pt_engine.buf;

            // erase();

            if (header->src == PT_SRC_ENGINE &&
                header->typ == PT_TYP_TELEMETORY) {
              if (header->len == sizeof(struct pt_payload_engine)) {
                struct pt_frame_engine *frame =
                    (struct pt_frame_engine *)pt_engine.buf;
                state.engine_latest = frame->payload;
                state.engine_valid = true;
                state.engine_frame_count++;
              }
            } else if (header->src == PT_SRC_CHASSIS &&
                       header->typ == PT_TYP_TELEMETORY) {
              struct pt_frame_chassis *frame =
                  (struct pt_frame_chassis *)pt_engine.buf;
              state.chassis_latest = frame->payload;
              state.chassis_valid = true;
              state.chassis_frame_count++;
              // mvprintw(12, 0, "CHASSIS  uptime %.1f s",
              // frame->payload.uptime_ms / 1000.0f);
            }

            // printf("[FRAME] @%u\n", frame->payload.uptime_ms);
            // printf("[FRAME] ADC_I_ESC_1 %d\n", frame->payload.adc_i_esc_1);
            // printf("[FRAME] ADC_I_ESC_2 %d\n", frame->payload.adc_i_esc_2);
            // printf("[FRAME] ADC_I_SYS %d\n", frame->payload.adc_i_sys);
            // printf("[FRAME] ADC_V_CELL_1 %d\n", frame->payload.adc_v_cell_1);
            // printf("[FRAME] ADC_V_CELL_2 %d\n", frame->payload.adc_v_cell_2);
            // printf("[FRAME] ADC_V_Q %d\n", frame->payload.adc_v_q);
            // printf("[FRAME] ADC_TEMP %d\n", frame->payload.adc_temp);
            // refresh();
          }
        }
      } else {
        printf("Disconnect...\n");
        close(poll_fds[0].fd);
        break;
      }
    }

    // rendering timer
    if (poll_fds[1].revents & POLLIN) {
      uint64_t expirations;
      read(timer_fd, &expirations,
           sizeof(expirations)); // 必須、読まないと再発火
      render(&state);
    }

    // evdev
    if (poll_fds[2].revents & POLLIN) {
      struct input_event ev;
      read(evdev_fd, &ev, sizeof(ev));
      if (ev.type == EV_KEY) {
        bool pressed = (ev.value != 0); // press or autorepeat → pressed
        switch (ev.code) {
        case KEY_W:
          key.w = pressed;
          break;
        case KEY_A:
          key.a = pressed;
          break;
        case KEY_S:
          key.s = pressed;
          break;
        case KEY_D:
          key.d = pressed;
          break;
        }
      }
    }
    if (poll_fds[3].revents & POLLIN) {
      uint64_t exp;
      read(timer_tx_fd, &exp, sizeof(exp));
      int16_t l, r;
      compute_target_and_ramp(&key, &l, &r);

      struct pt_frame_cmd_drive frame;
      frame.header.syn = PT_SYNC;
      frame.header.src = PT_SRC_RPI;
      frame.header.typ = PT_TYP_CMD_DRIVE;
      frame.header.len = sizeof(frame.payload);
      frame.payload.throttle_l = l;
      frame.payload.throttle_r = r;
      frame.crc = pt_crc8(&frame.header.src, 3 + sizeof(frame.payload));

      write(server_fd, &frame, sizeof(frame));

      // ついでに app_state に最新 throttle を入れて render で表示
      //state.cmd_l = l;
      //state.cmd_r = r;
    }
  }

  return 0;
}
