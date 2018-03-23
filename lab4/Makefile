CC := gcc
CFLAGS := -g -Wall

sources := client.c server.c
targets := chatclient chatserver

.PHONY: clean default all

default: all
all: $(targets)

chatclient: client.o
	$(CC) -o chatclient client.o

chatserver: server.o
	$(CC) -o chatserver server.o

client.o: client.c
	$(CC) $(CFLAGS) -c -o client.o client.c

server.o: server.c
	$(CC) $(CFLAGS) -c -o server.o server.c
