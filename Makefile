dimet: dime.c dime.h util.c util.h
	gcc -g -Wall -o dimet dime.c util.c -lpthread

test: dimet
	./dimet

clean:
	rm -f dimet
