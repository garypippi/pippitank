#include <iostream>
#include <vector>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include "net/tcp.hpp"
#include "net/img.hpp"

using namespace cv;
using namespace std;
using namespace pippitank::net;

int main(int argc, char* argv[]) {
    //
    tcp::server server(AF_INET, htons(3333), inet_addr("127.0.0.1"));

    if (server.bind() < 0) {
        cerr << "err: bind()" << endl;
        return -1;
    }

    if (server.listen() < 0) {
        cerr << "err: listen()" << endl;
        return -1;
    }

    tcp::client client = server.accept();
    img::server im(&client);

    // char ok[2] = "k";
    // const int chunk_size = 4096 * 2;
    // vector<uchar> chunk;

    // cout << "Listening..." << endl;

    while (1) {
        im.show();
        // vector<uchar> c = vector<uchar>(sizeof(int));
        // 
        // client.recv(&c[0], sizeof(int));
        // 
        // int n = 0;
        // 
        // for (int i = sizeof(int); i > 0; i--) {
        //     n = n | (c[i - 1] << (8 * (i - 1)));
        // }
        // 
        // cout << n << endl;
        // 
        // client.send(ok, 2);
        // 
        // 
        // const int m = 4096;
        // 
        // 
        // vector<uchar> img = vector<uchar>(0);
        // 
        // while (1) {
        //     vector<uchar> chk = vector<uchar>(m);
        //     int k = client.recv(&chk[0], m);
        //     img.insert(img.end(), chk.begin(), chk.begin() + k);
        //     if ((int) img.size() >= n) {
        //         break;
        //     }
        // }
        // // // 
        // // client.recv(&img[0], (size_t) n);
        // 
        // Mat src = imdecode(img, CV_8S);
        // Size siz = src.size();
        // 
        // if (siz.width + siz.height > 0) {
        // //     imshow("Live", src);
        // //     waitKey(10);
        //     imshow("Live", src);
        //     waitKey(10);
        // }
        // 
        // client.send(ok, 2);
        // // client.recv(&buf[0], n);
        // // 
        // // Mat src = imdecode(buf, CV_8S);
        // // 
        // // Size siz = src.size();
        // // 
        // // if (siz.width + siz.height > 0) {
        // //     imshow("Live", src);
        // //     waitKey(10);
        // //     client.send(ok, 2);
        // // }
    }
}
