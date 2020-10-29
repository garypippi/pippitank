#include <iostream>
#include <string>
#include <serial/serial.h>
#include <bitset>
#include <unistd.h>

using namespace std;

int main(int argc, char** argv)
{
    if(argc < 3) {
        cerr << "Usage: test_serial <serial port address> ";
        cerr << "<baudrate>" << endl;
        return 0;
    }

    string port(argv[1]);
    string baud(argv[2]);

    serial::Serial s(port, stoi(baud), serial::Timeout::simpleTimeout(0));

    if (! s.isOpen()) {
        cerr << "Error opening port " << port << endl;
        return 0;
    }


    int w;
    uint8_t o[2];

    sleep(3);

    while (1) {
        cout << "ms: ";
        cin >> w;
        o[0] = w & 0xff;
        o[1] = w >> 8;
        s.write(o, 2);
    }
}
