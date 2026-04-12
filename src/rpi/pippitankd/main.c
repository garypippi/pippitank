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

#define MAX_CLIENTS   1
#define BASE_POLL_FDS 2
#define BUFFER_SIZE   128
#define BAUDRATE      B9600
#define IDLE_PULSE    1500

int main(int argc, char *argv[])
{
    int no_serial = 0;
    char *serial_port_path = NULL;

    int opt;
    struct option options[] = {
        {"port",      required_argument, NULL, 'p'},
        {"no-serial", no_argument,       NULL, 'n'},
        {0,           0,                 0,     0 },
    };

    while ((opt = getopt_long(argc, argv, ":p:n", options, NULL)) != -1)
    {
        switch (opt)
        {
            case 'p':
                serial_port_path = optarg;
                break;
            case 'n':
                no_serial = 1;
                break;
            case ':':
                fprintf(stderr, "Option -%c requires argument.\n", optopt);
                return -1;
            case '?':
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return -1;
        }
    }

    // Set to -1 so poll() ignores this slot when --no-serial is given.
    int serial_port = -1;

    if (no_serial != 1)
    {
        if (serial_port_path == NULL)
        {
            fprintf(stderr, "[ERROR] Please specify the serial port path.\n");
            return -1;
        }

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

    // UNIX domain socket for accepting clients.
    int server_fd;
    struct sockaddr_un sock_addr;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("[ERROR] socket()");
        return -1;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, PIPPITANKD_SOCK_PATH);

    // Remove any stale socket file.
    unlink(PIPPITANKD_SOCK_PATH);

    if (bind(server_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_un)) == -1) {
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
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd < 0)
            {
                perror("[ERROR] accept()");
                return -1;
            }

            if (poll_num_fds < BASE_POLL_FDS + MAX_CLIENTS)
            {
                printf("[INFO] Client connected: %d\n", client_fd);
                poll_fds[poll_num_fds].fd = client_fd;
                poll_fds[poll_num_fds].events =  POLLIN;
                poll_num_fds++;
            }
            else
            {
                close(client_fd);
            }

        }

        // Handle responses from Arduino.
        if (poll_fds[1].revents & POLLIN)
        {
            char buffer[4];
            int n = read(serial_port, buffer, sizeof(buffer) - 1);

            if (n > 0)
            {
                buffer[n] = '\0';
                printf("[INFO] Response from Arduino (%d bytes): %s\n", n, buffer);
            }
        }

        // Handle requests from clients.
        for (int i = BASE_POLL_FDS; i < poll_num_fds; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                char buffer[BUFFER_SIZE];
                int n = read(poll_fds[i].fd, buffer, sizeof(buffer) - 1);

                if (n <= 0)
                {
                    printf("[INFO] Client disconnected: %d\n", poll_fds[i].fd);
                    close(poll_fds[i].fd);
                    poll_fds[i] = poll_fds[poll_num_fds - 1];
                    poll_num_fds--;
                    i--; // Re-check this slot — it now holds the swapped-in last entry.
                }
                else
                {
                    int cmd_value, ret;
                    char cmd_type;

                    buffer[n] = '\0';

                    // Only one command is currently supported: "F [SPEED]".
                    ret = sscanf(buffer, "%c %d", &cmd_type, &cmd_value);

                    // Move forward at the given speed.
                    if (ret == 2 && cmd_type == 'F')
                    {
                        char cmd_str[BUFFER_SIZE];
                        sprintf(cmd_str, "L%d", IDLE_PULSE + cmd_value);

                        if (no_serial != 1)
                        {
                            write(serial_port, cmd_str, strlen(cmd_str));
                            write(serial_port, "\n", 1);
                            printf("[INFO] Command sent: %s\n", cmd_str);
                        }
                        else
                        {
                            printf("[INFO] Command skipped: %s\n", cmd_str);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);

    unlink(PIPPITANKD_SOCK_PATH);

    return 0;
}
