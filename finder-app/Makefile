CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
SRC = writer.c 
OBJ = $(SRC:.c=.o)

all: writer

writer: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) writer