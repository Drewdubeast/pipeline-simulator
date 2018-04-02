# Simple makefile that builds the pipeline simulator
CC=gcc
CFLAGS=-std=c99 -pipe -w

all: pipeline
	$(CC) $(CFLAGS) pipeline.o -o pipeline

pipeline: pipeline.c
	$(CC) $(CFLAGS) -c pipeline.c

clean:
	rm *.o pipeline
