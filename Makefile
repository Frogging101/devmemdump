all: fuckdevmem

fuckdevmem: fuckdevmem.h num_util.o proc_util.o fuckdevmem.o
	gcc -std=c99 -o fuckdevmem num_util.o proc_util.o fuckdevmem.o

fuckdevmem.o: fuckdevmem.c
	gcc -std=c99 -c fuckdevmem.c

num_util.o: num_util.c
	gcc -std=c99 -c num_util.c

proc_util.o: proc_util.c
	gcc -std=c99 -c proc_util.c

clean:
	rm -f *.o fuckdevmem
