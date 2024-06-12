CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror -g



aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -f aesdsocket

all: clean aesdsocket