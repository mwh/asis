PREFIX = /usr/local

asis: asis.c
	$(CC) -o asis asis.c

install: asis
	cp asis $(PREFIX)/bin/asis

clean:
	rm -f asis
