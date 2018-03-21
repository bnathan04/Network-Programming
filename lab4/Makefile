CC := gcc
CFLAGS := -g -Wall

sources := client.c server.c
targets := client server

.PHONY: clean default all

default: all
all: $(targets)

client: client.o
	$(CC) -o client client.o

server: server.o
	$(CC) -o server server.o

client.o: client.c
	$(CC) $(CFLAGS) -c -o client.o client.c

server.o: server.c
	$(CC) $(CFLAGS) -c -o server.o server.c
