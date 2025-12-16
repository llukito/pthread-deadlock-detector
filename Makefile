CC = gcc
CFLAGS = -Wall -fPIC -O2 -g
LDFLAGS = -shared -ldl -pthread

LIBSRC = intercept.c tracker.c graph.c
TARGET = libdeadlock.so

all: $(TARGET) deadlock1 test

$(TARGET): $(LIBSRC)
	$(CC) $(CFLAGS) $(LIBSRC) -o $(TARGET) $(LDFLAGS)

deadlock1: deadlock1.c
	$(CC) deadlock1.c -o deadlock1 -pthread

test: test.c
	$(CC) test.c -o test -pthread

.PHONY: clean
clean:
	rm -f $(TARGET) deadlock1
