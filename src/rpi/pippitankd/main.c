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

#define MAX_CLIENTS 1
#define BUFFER_SIZE 128
#define BAUDRATE B9600
#define IDLE_PULSE 1500

int main(int argc, char *argv[])
{
    // Parse options
    int opt;
    int no_serial = 0;
    char *serial_port_path = NULL;
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

    // Open serial port
    int serial_port = -1;

    if (no_serial != 1)
    {
        struct termios termios_options;

        if ((serial_port = open(serial_port_path, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
        {
            fprintf(stderr, "ERROR: Failed to open serial port: %s\n", serial_port_path);
            return -1;
        }

        tcgetattr(serial_port, &termios_options);
        cfsetispeed(&termios_options, BAUDRATE);
        cfsetospeed(&termios_options, BAUDRATE);
        cfmakeraw(&termios_options); // Don't wait for new line.
        tcsetattr(serial_port, TCSADRAIN, &termios_options);
    }

    // Create unix domain socket
    int server_fd;
    struct sockaddr_un sock_addr;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("ERROR: socket()");
        return -1;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, PIPPITANKD_SOCK_PATH);

    // Make sure unix socket is absent
    unlink(PIPPITANKD_SOCK_PATH);

    if (bind(server_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_un)) == -1) {
        perror("ERROR: bind()");
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("ERROR: listen()");
        return -1;
    }

    struct pollfd poll_fds[2 + MAX_CLIENTS];

    // Server
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    // Serial port
    poll_fds[1].fd = serial_port;
    poll_fds[1].events = POLLIN;

    int poll_num_fds = 2;

    while (1)
    {
        if (poll(poll_fds, poll_num_fds, -1) < 0)
        {
            perror("ERROR: poll()");
            return -1;
        }

        // Accept client
        if (poll_fds[0].revents & POLLIN)
        {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd < 0)
            {
                perror("ERROR: accept()");
                return -1;
            }

            if (poll_num_fds < MAX_CLIENTS + 2)
            {
                printf("Accept(%d)\n", client_fd);
                poll_fds[poll_num_fds].fd = client_fd;
                poll_fds[poll_num_fds].events =  POLLIN;
                poll_num_fds++;
            }
            else
            {
                close(client_fd);
            }

        }

        // Response from arduino
        if (poll_fds[1].revents & POLLIN)
        {
            char buffer[4];
            int n = read(serial_port, buffer, sizeof(buffer) - 1);

            if (n > 0)
            {
                buffer[n] = '\0';
                printf("Response(%d): %s\n", n, buffer);
            }
        }

        // Handle client requests
        for (int i = 2; i < poll_num_fds; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                char buffer[BUFFER_SIZE];
                int n = read(poll_fds[i].fd, buffer, sizeof(buffer) - 1);

                if (n <= 0)
                {
                    printf("Disconnected(%d)\n", poll_fds[i].fd);
                    close(poll_fds[i].fd);
                    poll_fds[i] = poll_fds[poll_num_fds - 1];
                    poll_num_fds--;
                    i--;
                }
                else
                {
                    int cmd_value, parse_ret;
                    char cmd_type, parse_cmd[BUFFER_SIZE];

                    buffer[n] = '\0';

                    if ((parse_ret = sscanf(buffer, "%c %d", &cmd_type, &cmd_value)) == 2)
                    {
                        sprintf(parse_cmd, "L%d\n", IDLE_PULSE + cmd_value);
                    }

                    if (parse_ret == 2)
                    {
                        write(serial_port, parse_cmd, strlen(parse_cmd));
                        printf("Command: %s\n", parse_cmd);
                    }
                }
            }
        }
    }

    close(server_fd);

    unlink(PIPPITANKD_SOCK_PATH);

    return 0;
}
