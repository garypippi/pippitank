CC         = gcc
CC_RPI     = arm-linux-gnueabihf-gcc
CFLAGS     = -Wall -Wextra
CFLAGS_RPI = -static -Wall -Wextra

BIN = bin

.PHONY: all tui clean deploy

all: $(BIN)/pippitankd $(BIN)/pippitank-cli $(BIN)/pippitank-tui $(BIN)/pippitank-cmd

clean:
	rm -rf $(BIN)

deploy: $(BIN)/pippitankd
	scp $(BIN)/pippitankd pippitank:

$(BIN)/pippitankd: src/pippitankd/main.c | $(BIN)
	$(CC_RPI) $(CFLAGS_RPI) -o $@ $^

$(BIN)/pippitank-cli: src/pippitank-cli/main.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/pippitank-tui: src/pippitank-tui/main.c | $(BIN)
	$(CC) $(CFLAGS) -lm -lncurses -ltinfo -o $@ $^

$(BIN)/pippitank-cmd: src/pippitank-cmd/main.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN):
	mkdir -p $(BIN)
