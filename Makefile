# Compiler and Flags
CC = gcc
CFLAGS = -Wall -W -O2


TARGET = test_fsst

OBJ = test_fsst.o hashFsst.o heap.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

test.o: test_fsst.c hashFsst.h 
	$(CC) $(CFLAGS) -c test_fsst.c

hashFsst.o: hashFsst.c hashFsst.h heap.h
	$(CC) $(CFLAGS) -c hashFsst.c

heap.o: heap.c heap.h
	$(CC) $(CFLAGS) -c heap.c

clean:
	rm -f $(OBJ) $(TARGET)
