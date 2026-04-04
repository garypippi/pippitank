CC  = arm-linux-gnueabihf-gcc
CFLAGS  = -static

.PHONY: all deploy clean arduino-compile arduino-upload

all: pippitankd pippitank-cli

clean:
	rm -f pippitankd pippitank-cli

deploy: pippitankd pippitank-cli
	scp pippitankd pippitank-cli pippitank:

arduino-compile:
	arduino-cli compile src/arduino/pippitank

arduino-upload:
	arduino-cli upload src/arduino/pippitank

pippitankd: src/rpi/pippitankd/main.c
	$(CC) $(CFLAGS) -o $@ $^

pippitank-cli: src/rpi/pippitank-cli/main.c
	$(CC) $(CFLAGS) -o $@ $^
