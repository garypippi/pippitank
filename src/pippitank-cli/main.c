#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../protocol.h"

#define BUFFER_SIZE 128

// A simple pippitankd client.
// Sends whatever you type, except "q" which quits the program.
int main(int argc, char *argv[])
{
    int listen_port = 0;
    char listen_addr[64]  = "";

    int opt;
    struct option options[] = {
        {"listen",    required_argument, NULL, 'L'},
        {0,           0,                 0,     0 },
    };

    while ((opt = getopt_long(argc, argv, ":s:L:", options, NULL)) != -1)
    {
        switch (opt)
        {
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

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("ERROR: socket()");
        return -1;
    }

    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(listen_addr);
    sock_addr.sin_port        = htons(listen_port);

    if (connect(server_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("ERROR: connect()");
        return -1;
    }

    char buffer[BUFFER_SIZE];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0)
        {
            continue;
        }

        if (strcmp(buffer, "q") == 0)
        {
            break;
        }

        write(server_fd, buffer, strlen(buffer));
        write(server_fd, "\n", 1);
    }

    printf("Bye...");

    close(server_fd);

    return 0;
}
