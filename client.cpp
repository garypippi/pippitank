#include <iostream>
#include "net/tcp.hpp"
#include "net/img.hpp"

using namespace cv;
using namespace std;
using namespace pippitank::net;

int main() {

    tcp::client client(AF_INET, htons(3333), inet_addr("127.0.0.1"));
    img::client img(&client);

    if (client.connect() < 0) {
        cerr << "err: connect()" << endl;
        return -1;
    }

    while (1) {
        img.send();
    }
}
