
dimet: dime.c dime.h util.c util.h
	gcc -g -Wall -o dimet dime.c util.c

clean:
	rm -f dimet
