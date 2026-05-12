#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include "../protocol.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define MAX_CLIENTS   1
#define BASE_POLL_FDS 2
#define BUFFER_SIZE   128
#define BAUDRATE      B19200
#define PWM_MIN       1000
#define PWM_MAX       2000
#define PWM_IDLE      1500

struct pt_client {
    int fd;
    char rx_buf[BUFFER_SIZE];
    size_t rx_len;
};

//enum pt_rx_state {
//    WAIT_SYNC,
//    READ_HEADER,
//    READ_PAYLOAD,
//    READ_CRC
//};

struct pt_rx_engine {
    enum pt_rx_state state;
    uint8_t buf[sizeof(struct pt_frame_engine) + 1];
    uint16_t pos;
    uint16_t len;
    int done;
};

//void pt_rx_engine_feed(struct pt_rx_engine* engine, uint8_t data)
//{
//    switch (engine->state)
//    {
//        case PT_RX_WAIT_SYNC:
//            if (data == PT_SYNC)
//            {
//                engine->state = PT_RX_READ_HEADER;
//                engine->buf[engine->pos++] = data;
//                engine->len = sizeof(struct pt_header) - 1;
//            }
//            break;
//        case PT_RX_READ_HEADER:
//            engine->buf[engine->pos++] = data;
//            engine->len--;
//
//            if (engine->len == 0)
//            {
//                engine->state = PT_RX_READ_PAYLOAD;
//                engine->len = sizeof(struct pt_payload_engine);
//                if (engine->buf[3] != sizeof(struct pt_payload_engine))
//                {
//                    fprintf(stderr, "[ERROR] pt_header len mismatch.\n");
//                    engine->state = PT_RX_WAIT_SYNC;
//                    engine->pos = 0;
//                }
//            }
//            break;
//        case READ_PAYLOAD:
//            engine->buf[engine->pos++] = data;
//            engine->len--;
//
//            if (engine->len == 0)
//            {
//                engine->state = READ_CRC;
//            }
//            break;
//        case READ_CRC:
//            if (pt_crc8(engine->buf + 1, 3 + sizeof(struct pt_payload_engine)) != data)
//            {
//                fprintf(stderr, "[ERROR] CRC error\n");
//            }
//            else
//            {
//                engine->done = 1;
//            }
//            engine->buf[engine->pos] = data;
//            engine->state = WAIT_SYNC;
//            engine->pos = 0;
//            break;
//    }
//}

int main(int argc, char *argv[])
{
    int listen_port = 0;
    char listen_addr[64]  = "";
    char *serial_port_path = NULL;

    int opt;
    struct option options[] = {
        {"serial",    required_argument, NULL, 's'},
        {"listen",    required_argument, NULL, 'L'},
        {0,           0,                 0,     0 },
    };

    while ((opt = getopt_long(argc, argv, ":s:L:", options, NULL)) != -1)
    {
        switch (opt)
        {
            case 's':
                serial_port_path = optarg;
                break;
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

    if (listen_addr[0] == '\0')
    {
        fprintf(stderr, "[ERROR] No listen address provided.");
        return -1;
    }

    if (listen_port == 0) {
        fprintf(stderr, "[ERROR] No listen port provided.");
        return -1;
    }

    printf("[INFO] serial_port_path: %s\n", serial_port_path);
    printf("[INFO] listen_addr: %s\n", listen_addr);
    printf("[INFO] listen_port: %d\n", listen_port);

    // Set to -1 so poll() ignores this slot when no serial port path is given.
    int serial_port = -1;

    if (serial_port_path != NULL)
    {
        struct termios termios_options;

        if ((serial_port = open(serial_port_path, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
        {
            fprintf(stderr, "[ERROR] Failed to open serial port: %s\n", serial_port_path);
            return -1;
        }

        tcgetattr(serial_port, &termios_options);

        cfsetispeed(&termios_options, BAUDRATE);
        cfsetospeed(&termios_options, BAUDRATE);

        // Needed because without raw mode, read() blocks until a newline,
        // and Arduino's ACK has no terminator.
        cfmakeraw(&termios_options);

        tcsetattr(serial_port, TCSADRAIN, &termios_options);
    }

    // TCP socket for accepting clients.
    int server_fd;
    struct sockaddr_in sock_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[ERROR] socket()");
        return -1;
    }

    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(listen_addr);
    sock_addr.sin_port        = htons(listen_port);

    int sockopt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1)
    {
        perror("[ERROR] setsockopt(SO_REUSEADDR)");
        return -1;
    }

    if (bind(server_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("[ERROR] bind()");
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("[ERROR] listen()");
        return -1;
    }

    int poll_num_fds = BASE_POLL_FDS;
    struct pollfd poll_fds[BASE_POLL_FDS + MAX_CLIENTS];

    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = serial_port;
    poll_fds[1].events = POLLIN;

    struct pt_client client;
    struct pt_rx pt_engine;
    pt_rx_init(&pt_engine);
    //pt_engine.state = WAIT_SYNC;
    //pt_engine.pos = 0;
    //pt_engine.done = 0;

    while (1)
    {
        if (poll(poll_fds, poll_num_fds, -1) < 0)
        {
            perror("[ERROR] poll()");
            return -1;
        }

        // Accept incoming clients.
        if (poll_fds[0].revents & POLLIN)
        {
            client.fd = accept(server_fd, NULL, NULL);
            client.rx_len = 0;

            if (client.fd < 0)
            {
                perror("[ERROR] accept()");
                return -1;
            }

            if (setsockopt(client.fd, IPPROTO_TCP, TCP_NODELAY, &sockopt, sizeof(sockopt)) == -1)
            {
                perror("[ERROR] setsockopt(TCP_NODELAY)");
            }

            if (setsockopt(client.fd, SOL_SOCKET, SO_KEEPALIVE, &sockopt, sizeof(sockopt)) == -1)
            {
                perror("[ERROR] setsockopt(SO_KEEPALIVE)");
            }

            if (poll_num_fds < BASE_POLL_FDS + MAX_CLIENTS)
            {
                printf("[INFO] Client connected: %d\n", client.fd);
                poll_fds[poll_num_fds].fd = client.fd;
                poll_fds[poll_num_fds].events =  POLLIN;
                poll_num_fds++;
            }
            else
            {
                close(client.fd);
                client.fd = -1;
                client.rx_len = 0;
            }

        }

        // Handle responses from Arduino.
        if (poll_fds[1].revents & POLLIN)
        {
            char buffer[64];
            int n = read(serial_port, buffer, sizeof(buffer));

              //printf("[INFO] RX %d bytes:", n);
              //for (int i = 0; i < n; i++) {
              //    printf(" %02X", (unsigned char)buffer[i]);
              //}
              //printf("\n");

            if (n > 0)
            {
                //buffer[n] = '\0';
                //printf("[INFO] Response from Arduino (%d bytes)\n", n);

                for (int i = 0; i < n; i++)
                {
                    pt_rx_feed(&pt_engine, buffer[i]);
                    //pt_rx_engine_feed(&pt_engine, buffer[i]);
                    if (pt_engine.done == 1)
                    {
                        pt_engine.done = 0;
                        struct pt_frame_engine *frame = (struct pt_frame_engine*) pt_engine.buf;

                        //printf("[FRAME] @%u\n", frame->payload.uptime_ms);
                        //printf("[FRAME] ADC_I_ESC_1 %d\n", frame->payload.adc_i_esc_1);
                        //printf("[FRAME] ADC_I_ESC_2 %d\n", frame->payload.adc_i_esc_2);
                        //printf("[FRAME] ADC_I_SYS %d\n", frame->payload.adc_i_sys);
                        //printf("[FRAME] ADC_V_CELL_1 %d\n", frame->payload.adc_v_cell_1);
                        //printf("[FRAME] ADC_V_CELL_2 %d\n", frame->payload.adc_v_cell_2);
                        //printf("[FRAME] ADC_V_Q %d\n", frame->payload.adc_v_q);
                        //printf("[FRAME] ADC_TEMP %d\n", frame->payload.adc_temp);

                        if (poll_num_fds > BASE_POLL_FDS)
                        {
                            for (int j = BASE_POLL_FDS; j < poll_num_fds; j++)
                            {
                                write(poll_fds[j].fd, frame, sizeof(*frame));
                            }
                        }
                    }
                }
            }
        }

        // Handle requests from clients.
        for (int i = BASE_POLL_FDS; i < poll_num_fds; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                 char buf[64];
                 int n = read(poll_fds[i].fd, buf, sizeof(buf));

                 if (n <= 0) {
                     printf("[INFO] Client disconnected: %d\n", poll_fds[i].fd);
                     close(poll_fds[i].fd);
                     poll_fds[i] = poll_fds[poll_num_fds - 1];
                     poll_num_fds--;
                     i--;
                 } else {
                     if (serial_port != -1) {
                         ssize_t w = write(serial_port, buf, n);
                         if (w < 0) perror("[ERROR] write to serial");
                     }
                     printf("[INFO] Forwarded %d bytes to UART\n", n);   // デバッグ、後で削除可
                 }

                ////char buffer[BUFFER_SIZE];
                //int n = read(poll_fds[i].fd, client.rx_buf + client.rx_len, sizeof(client.rx_buf) - client.rx_len - 1);

                //if (n <= 0)
                //{
                //    printf("[INFO] Client disconnected: %d\n", poll_fds[i].fd);
                //    close(poll_fds[i].fd);
                //    poll_fds[i] = poll_fds[poll_num_fds - 1];
                //    poll_num_fds--;
                //    i--; // Re-check this slot — it now holds the swapped-in last entry.
                //    client.fd = -1;
                //    client.rx_len = 0;
                //}
                //else
                //{
                //    char *pos;
                //    client.rx_len += n;
                //    printf("[INFO] %d bytes read from client.\n", n);


                //    if ((pos = memchr(client.rx_buf, '\n', client.rx_len)) != NULL)
                //    {
                //        printf("[INFO] new line detected.\n");
                //        *pos = '\0';
                //        size_t consumed = pos - client.rx_buf + 1;
                //        memmove(client.rx_buf, client.rx_buf + consumed, client.rx_len - consumed);
                //        client.rx_len -= consumed;

                //        int cmd_value, pwm_value, ret;
                //        char cmd_type;
                //        char cmd_str[BUFFER_SIZE] = "";

                //        //client.rx_buf[n] = '\0';
                //        ret = sscanf(client.rx_buf, "%c %d", &cmd_type, &cmd_value);

                //        if (ret != 2)
                //        {
                //            printf("[INFO] ret is not 2\n");
                //            continue;
                //        }

                //        switch (cmd_type)
                //        {
                //            case 'F':
                //                if (cmd_value >= 0 && cmd_value <= 100)
                //                {
                //                    pwm_value = PWM_IDLE + (PWM_MAX - PWM_IDLE) * cmd_value / 100;
                //                    sprintf(cmd_str, "L%d",  pwm_value);
                //                }
                //                break;
                //            case 'B':
                //                if (cmd_value >= 0 && cmd_value <= 100)
                //                {
                //                    pwm_value = PWM_IDLE - (PWM_IDLE - PWM_MIN) * cmd_value / 100;
                //                    sprintf(cmd_str, "L%d",  pwm_value);
                //                }
                //                break;
                //        }

                //        if (cmd_str[0] == '\0')
                //        {
                //            printf("[INFO] empty cmd_str\n");
                //            continue;
                //        }

                //        if (serial_port != -1)
                //        {
                //            write(serial_port, cmd_str, strlen(cmd_str));
                //            write(serial_port, "\n", 1);
                //            printf("[INFO] Command sent: %s\n", cmd_str);
                //        }
                //        else
                //        {
                //            printf("[INFO] Command skipped: %s\n", cmd_str);
                //        }
                //    }
                //}
            }
        }
    }

    close(server_fd);

    //unlink(PIPPITANKD_SOCK_PATH);

    return 0;
}
