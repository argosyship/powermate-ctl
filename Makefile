CC ?= gcc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -Wall -Wextra -std=c11
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libevdev)
LDFLAGS += $(shell $(PKG_CONFIG) --libs libevdev)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all test clean install

all: powermate-scroll

powermate-scroll: src/main.c src/mapping.c src/config.c src/mapping.h src/config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ src/main.c src/mapping.c src/config.c $(LDFLAGS)

tests/test_mapping: tests/test_mapping.c src/mapping.c src/config.c src/mapping.h src/config.h
	$(CC) $(CFLAGS) -o $@ tests/test_mapping.c src/mapping.c src/config.c

test: tests/test_mapping
	./tests/test_mapping

clean:
	rm -f powermate-scroll tests/test_mapping

install: powermate-scroll
	install -Dm755 powermate-scroll "$(DESTDIR)$(BINDIR)/powermate-scroll"
