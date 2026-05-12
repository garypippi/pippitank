#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../protocol.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s HOST PORT THROTTLE_L THROTTLE_R\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    int16_t l = (int16_t)atoi(argv[3]);
    int16_t r = (int16_t)atoi(argv[4]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(host);
    if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect"); return 1;
    }

    struct pt_frame_cmd_drive frame;
    frame.header.syn = PT_SYNC;
    frame.header.src = PT_SRC_RPI;
    frame.header.typ = PT_TYP_CMD_DRIVE;
    frame.header.len = sizeof(frame.payload);
    frame.payload.throttle_l = l;
    frame.payload.throttle_r = r;
    frame.crc = pt_crc8((const uint8_t*)&frame.header.src, 3 + sizeof(frame.payload));

    if (write(fd, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("write"); return 1;
    }
    close(fd);
    printf("Sent: L=%d R=%d (%zu bytes)\n", l, r, sizeof(frame));
    return 0;
}
