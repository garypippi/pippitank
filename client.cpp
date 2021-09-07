#include <iostream>
// #include <vector>
// #include <unistd.h>
// #include <opencv4/opencv2/core.hpp>
// #include <opencv4/opencv2/videoio.hpp>
// #include <opencv4/opencv2/imgcodecs.hpp>
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

    // VideoCapture capture(0);

    // char re[2];
    // char ok[2] = "k";
    // char im[2] = "i";

    while (1) {
        img.send();
        // 
        // Mat src;
        // 
        // vector<uchar> buf;
        // 
        // capture.read(src);
        // 
        // imencode(".jpg", src, buf);
        // 
        // int n = buf.size();
        // 
        // cout << n << endl;
        // 
        // client.send(&n, sizeof(int));
        // // client.send(im, 2);
        // 
        // client.recv(re, 2);
        // 
        // cout << re << endl;
        // 
        // client.send(&buf[0], buf.size());
        // 
        // // client.send(ok, 2);
        // 
        // // client.recv(re, 2);
        // 
        // cout << re << endl;
    }
}
