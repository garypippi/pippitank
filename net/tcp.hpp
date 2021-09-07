#pragma once

#include <arpa/inet.h>

namespace pippitank
{
    namespace net
    {
        namespace tcp
        {
            void addr(struct sockaddr_in *addr, sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr);

            class client
            {
                private:
                    int sock;
                    struct sockaddr_in addr;
                    socklen_t addr_len;

                public:
                    client(int src);
                    client(sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr);
                    ~client();
                    int connect();
                    ssize_t send(void* data, size_t size);
                    ssize_t recv(void* data, size_t size);
            };

            class server
            {
                private:
                    int sock;
                    struct sockaddr_in addr;

                public:
                    server(sa_family_t sin_family, in_port_t sin_port, in_addr_t s_addr);
                    ~server();
                    int bind();
                    int listen();
                    client accept();
            };
        }
    }
}
