#pragma once

#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/videoio.hpp>
#include "tcp.hpp"

namespace pippitank
{
    namespace net
    {
        namespace img
        {
            class client
            {
                private:
                    cv::VideoCapture cap;
                    tcp::client *tcp;

                public:
                    client(tcp::client *tcp);
                    void send();
            };

            class server
            {
                private:
                    tcp::client *tcp;

                public:
                    server(tcp::client *tcp);
                    void show();
            };
        }
    }
}
