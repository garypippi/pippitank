#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../protocol.h"

#define BUFFER_SIZE 128

// A simple pippitankd client.
// Sends whatever you type, except "q" which quits the program.
int main(int argc, char *argv[])
{
    int server_fd;
    struct sockaddr_un sock_addr;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("ERROR: socket()");
        return -1;
    }

    sock_addr.sun_family = AF_UNIX;
    strcpy(sock_addr.sun_path, PIPPITANKD_SOCK_PATH);

    if (connect(server_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_un)) == -1)
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
    }

    printf("Bye...");

    close(server_fd);

    return 0;
}
