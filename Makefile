CFLAGS=-Wall -std=c99
BINDIR=/usr/local/bin
MANDIR=/usr/local/share/man/man1

LINK_LIBS=-lcurl

morsefeed : main.c morsefeed.h morsefeed.c text.h text.c vector.h vector.c
	gcc $(CFLAGS) -o morsefeed main.c morsefeed.c text.c vector.c $(LINK_LIBS)

install : morsefeed
	cp morsefeed $(BINDIR)/
	mkdir -p $(MANDIR)
	cp morsefeed.1 $(MANDIR)/

clean :
	rm -f morsefeed *.o

distclean :
	rm -f morsefeed *.o $(BINDIR)/morsefeed $(MANDIR)/morsefeed.1
