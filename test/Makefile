CC = gcc
CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-variadic-macros -I. -I../src/c -g -O0
TARGET = test_eye_timer
SRC = test_eye_timer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
