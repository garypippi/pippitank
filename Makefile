CC_RPI      = arm-linux-gnueabihf-gcc
CC_HOST     = gcc
CFLAGS_RPI  = -static
CFLAGS_HOST = -Wall -Wextra

.PHONY: all host clean deploy arduino-compile arduino-upload

all: pippitankd pippitank-cli

host: pippitankd-host pippitank-cli-host

clean:
	rm -f pippitankd pippitank-cli pippitankd-host pippitank-cli-host

deploy: pippitankd pippitank-cli
	scp pippitankd pippitank-cli pippitank:

arduino-compile:
	arduino-cli compile src/arduino/pippitank

arduino-upload:
	arduino-cli upload src/arduino/pippitank

pippitankd: src/rpi/pippitankd/main.c
	$(CC_RPI) $(CFLAGS_RPI) -o $@ $^

pippitank-cli: src/rpi/pippitank-cli/main.c
	$(CC_RPI) $(CFLAGS_RPI) -o $@ $^

pippitankd-host: src/rpi/pippitankd/main.c
	$(CC_HOST) $(CFLAGS_HOST) -o $@ $^

pippitank-cli-host: src/rpi/pippitank-cli/main.c
	$(CC_HOST) $(CFLAGS_HOST) -o $@ $^
