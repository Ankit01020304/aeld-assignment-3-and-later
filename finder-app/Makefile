CC=$(CROSS_COMPILE)gcc

TARGET=writer
SRC=writer.c
OBJ=writer.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

$(OBJ): $(SRC)
	$(CC) -Wall -Werror -c $(SRC)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
