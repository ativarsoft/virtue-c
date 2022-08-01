CFLAGS=-Wall -Wpedantic
LDLIBS=-lvirt -ljson-c

BIN=virtue

all: $(BIN)

clean:
	rm -f *.o $(BIN)
