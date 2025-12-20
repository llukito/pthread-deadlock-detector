CC = gcc
CFLAGS = -Wall -fPIC -Og -g
LDFLAGS = -shared -ldl -pthread -rdynamic

LIBSRC = intercept.c tracker.c graph.c
TARGET = libdeadlock.so

TEST_SRCS = $(wildcard tests/*.c)
TEST_BINS = $(patsubst tests/%.c, %, $(TEST_SRCS))

all: $(TARGET) $(TEST_BINS)

$(TARGET): $(LIBSRC)
	$(CC) $(CFLAGS) $(LIBSRC) -o $(TARGET) $(LDFLAGS)

%: tests/%.c
	$(CC) $(CFLAGS) $< -o $@ -pthread

.PHONY: clean
clean:
	rm -f $(TARGET) $(TEST_BINS)