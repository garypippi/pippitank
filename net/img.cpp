#include "img.hpp"
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/videoio.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <vector>

using namespace pippitank::net;

img::client::client(tcp::client *tcp)
    : cap(cv::VideoCapture(0)), tcp(tcp) {};

void img::client::send() {
    cv::Mat src;
    cap.read(src);
    std::vector<uchar> buf;
    cv::imencode(".jpg", src, buf);
    char ok[2];
    int n = buf.size();
    tcp->send(&n, sizeof(int));
    tcp->recv(ok, 2);
    tcp->send(&buf[0], n);
    tcp->recv(ok, 2);
}

img::server::server(tcp::client *tcp)
    : tcp(tcp) {};

void img::server::show() {
    std::vector<uchar> size = std::vector<uchar>(sizeof(int));
    tcp->recv(&size[0], sizeof(int));

    int n = 0;

    for (int i = sizeof(int); i > 0; i--) {
        n = n | (size[i - 1] << (8 * (i - 1)));
    }

    char ok[2] = "k";

    tcp->send(ok, 2);

    const int m = 4096;

    std::vector<uchar> img = std::vector<uchar>(0);

    while (1) {
        std::vector<uchar> chk = std::vector<uchar>(m);
        int k = tcp->recv(&chk[0], m);
        img.insert(img.end(), chk.begin(), chk.begin() + k);
        if ((int) img.size() >= n) {
            break;
        }
    }

    cv::Mat src = cv::imdecode(img, CV_8S);
    cv::Size siz = src.size();

    if (siz.width + siz.height > 0) {
        cv::imshow("Live", src);
        cv::waitKey(10);
    }

    tcp->send(ok, 2);
}
