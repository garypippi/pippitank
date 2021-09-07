options = -std=c++17 -Wall --pedantic-error

all: server client

server: server.cpp net/tcp.o net/img.o
	g++ $(options) net/tcp.o net/img.o server.cpp -o server -lopencv_core -lopencv_videoio -lopencv_highgui -lopencv_imgcodecs -I/usr/include/opencv4

client: client.cpp net/tcp.o net/img.o
	g++ $(options) net/tcp.o net/img.o client.cpp -o client -lopencv_core -lopencv_videoio -lopencv_highgui -lopencv_imgcodecs -I/usr/include/opencv4

net/tcp.o: net/tcp.cpp net/tcp.hpp
	g++ -c net/tcp.cpp -o net/tcp.o

net/img.o: net/img.cpp net/img.hpp
	g++ -c net/img.cpp -o net/img.o -lopencv_core -lopencv_videoio -lopencv_imgcodecs -lopencv_highgui -I/usr/include/opencv4

clean:
	rm net/tcp.o net/img.o ./server ./client
