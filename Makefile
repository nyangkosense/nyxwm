CC     ?= gcc
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Wold-style-declaration
CFLAGS += -Wmissing-prototypes -Wno-unused-parameter
CFLAGS += $(shell pkg-config --cflags freetype2 xft x11)
LIBS   = $(shell pkg-config --libs x11 xft freetype2)
LIBS  += -lm -pthread

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

all: nyxwm

config.h:
	cp config.def.h config.h

nyxwm: nyxwm.c nyxwmblocks.c nyxwm.h config.h blocks.h Makefile
	$(CC) -O3 $(CFLAGS) -o $@ nyxwm.c nyxwmblocks.c $(LIBS)

install: all
	install -Dm755 nyxwm $(DESTDIR)$(BINDIR)/nyxwm

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/nyxwm

clean:
	rm -f nyxwm *.o

.PHONY: all install uninstall clean
