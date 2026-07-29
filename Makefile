CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude
LDFLAGS = -lpthread -lm

TARGET = arachne
SRCS = src/arachne_core.c \
       src/arachne_predict.c \
       src/arachne_learn.c \
       src/arachne_query.c \
       src/arachne_time.c \
       src/arachne_utils.c \
       src/arachne_cli.c

OBJS = $(SRCS:.c=.o)

TEST_TARGET = test_arachne
TEST_SRCS = $(SRCS) tests/test_core.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET) *.arachne

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	cp include/arachne.h /usr/local/include/

run: $(TARGET)
	./$(TARGET)
