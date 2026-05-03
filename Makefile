CC     ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -static -Wall -Wextra

BIN = bin

.PHONY: all clean deploy

all: $(BIN)/pippitankd $(BIN)/pippitank-cli

clean:
	rm -rf $(BIN)

deploy: all
	scp $(BIN)/pippitankd $(BIN)/pippitank-cli pippitank:

$(BIN)/pippitankd: src/pippitankd/main.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/pippitank-cli: src/pippitank-cli/main.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN):
	mkdir -p $(BIN)
