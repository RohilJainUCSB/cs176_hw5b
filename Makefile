CC = gcc
CFLAGS = -Wall

all: hangman_client hangman_server

hangman_client: hangman_client.c
	$(CC) $(CFLAGS) -o hangman_client hangman_client.c

hangman_server: hangman_server.c
	$(CC) $(CFLAGS) -o hangman_server hangman_server.c

clean:
	rm -f hangman_client hangman_server