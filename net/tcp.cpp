#include "tcp.hpp"
#include <string.h>
#include <unistd.h>

using namespace pippitank::net;

void tcp::addr(struct sockaddr_in *addr, sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = sin_family;
    addr->sin_port = sin_port;
    addr->sin_addr.s_addr = s_addr;
}

tcp::server::server(sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr) {
    sock = socket(sin_family, SOCK_STREAM, 0);
    tcp::addr(&addr, sin_family, sin_port, s_addr);
}

tcp::server::~server() {
    close(sock);
}

int tcp::server::bind() {
    return ::bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr));
}

int tcp::server::listen() {
    return ::listen(sock, 1);
}

tcp::client tcp::server::accept() {
    return tcp::client(sock);
}

tcp::client::client(int src) {
    sock = ::accept(src, (struct sockaddr*)&addr, &addr_len);
}

tcp::client::client(sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr) {
    sock = socket(sin_family, SOCK_STREAM, 0);
    tcp::addr(&addr, sin_family, sin_port, s_addr);
}

tcp::client::~client() {
    close(sock);
}

int tcp::client::connect() {
    return ::connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
}

ssize_t tcp::client::send(void *data, size_t size) {
    return ::send(sock, data, size, 0);
}

ssize_t tcp::client::recv(void *data, size_t size) {
    return ::recv(sock, data, size, 0);
}
