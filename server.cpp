#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sys/un.h>
#include <string.h>
#include <error.h>
#include "net/tcp.hpp"
#include "net/img.hpp"

using namespace cv;
using namespace std;
using namespace pippitank::net;

int main(int argc, char* argv[]) {
    //
    // tcp::server server(AF_INET, htons(3333), inet_addr("127.0.0.1"));
    // 
    // if (server.bind() < 0) {
    //     cerr << "err: bind()" << endl;
    //     return -1;
    // }
    // 
    // if (server.listen() < 0) {
    //     cerr << "err: listen()" << endl;
    //     return -1;
    // }
    // 
    // tcp::client client = server.accept();
    // img::server im(&client);
    // 
    // while (1) {
    //     im.show();
    // }
    
    int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strncpy(un.sun_path, "/home/garypippi/Documents/pippitank/c2p", 107);

    connect(s, (struct sockaddr*)&un, sizeof(un));

    // if (c < 0) {
    //     cerr << "err: connect()" << endl;
    // }

    // bind(s, (struct sockaddr*)&un, sizeof(un));

    int u = ::send(s, (char*) "hoge", 4, 0);

    if (u < 0) {
        cerr << "err: send() " << u << endl;
        cerr << strerror(errno) << endl;
    }
    // *un.sun_path = (char*) 
}
