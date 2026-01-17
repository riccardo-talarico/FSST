# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c11


TARGET = fsst

OBJ = fsst.o heap.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

fsst.o: fsst.c heap.h
	$(CC) $(CFLAGS) -c fsst.c

heap.o: heap.c heap.h
	$(CC) $(CFLAGS) -c heap.c

clean:
	rm -f $(OBJ) $(TARGET)
