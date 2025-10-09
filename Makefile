CC = gcc
CFLAGS = -Wall -Wextra -g
BUILD = build

default: build

build: main.o
	
%.o: %.c
	$(CC) 


clean: rm -r build