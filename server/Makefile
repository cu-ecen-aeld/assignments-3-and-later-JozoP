#CC = $(CROSS_COMPILE)gcc
#CC = gcc
CFLAGS = -Wall -Werror -g -pthread -lrt



aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -f aesdsocket

all: clean aesdsocket