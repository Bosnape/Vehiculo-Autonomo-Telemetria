CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGET = server
SOURCES = server.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: clean
