CC = gcc
CFLAGS = -std=c99


all: fuckdevmem

fuckdevmem: fuckdevmem.h num_util.o proc_util.o fuckdevmem.o
	$(CC) $(CFLAGS) -o fuckdevmem num_util.o proc_util.o fuckdevmem.o

fuckdevmem.o: fuckdevmem.c
	$(CC) $(CFLAGS) -c fuckdevmem.c

num_util.o: num_util.c
	$(CC) $(CFLAGS) -c num_util.c

proc_util.o: proc_util.c
	$(CC) $(CFLAGS) -c proc_util.c

debug: CFLAGS += -g
debug: fuckdevmem

clean:
	rm -f *.o fuckdevmem
