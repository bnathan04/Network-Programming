CC := gcc
CFLAGS := -Wall

sources := client.c
targets := client

.PHONY: clean default all

default: all
all: $(targets)

client: client.o
	$(CC) -o client client.o

client.o: client.c
	$(CC) $(CFLAGS) -c -o client.o client.c