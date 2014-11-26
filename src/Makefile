include ../config.mk

PREFIX ?= /usr/local
INSTALL ?= install

CC = gcc
CROSS_CC = $(CROSS_COMPILE)$(CC)
HOST_CC = $(HOST_COMPILE)$(CC)

CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS += -W -Wall -O2 -pedantic -std=c99
LIBS += -lusb

DEPENDS = Makefile ../config.mk

OBJS = main.o nolo.o printf-utils.o image.o fiasco.o device.o usb-device.o cold-flash.o operations.o local.o mkii.o disk.o cal.o
BIN = 0xFFFF
MANGEN = mangen

all: $(BIN) $(BIN).1

$(BIN): $(OBJS) $(DEPENDS)
	$(CROSS_CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

$(MANGEN): $(MANGEN).c $(DEPENDS)
	$(HOST_CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

$(BIN).1: $(BIN) $(MANGEN) $(DEPENDS) ../doc/examples
	./$(MANGEN) > $@.tmp
	(printf '.SH EXAMPLES\n.\n.PP\n.B\n'; cat ../doc/examples) | sed 's/^$$/.fi\n.\n.PP\n.B/' | sed '/^\.PP$$/N;/\.B/N;/\.fi/N;s/^\.PP\n\.B\n\.fi\n//' | sed '/^\.B/N;s/\n/ /;/^\.B/s/$$/\n.nf/' | sed '/^\.nf/N;/^\.fi/N;s/^\.nf\n.fi/./' >> $@.tmp
	mv $@.tmp $@

libusb-sniff-32.so: libusb-sniff.c $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC $< -ldl -shared -m32 -o $@

libusb-sniff-64.so: libusb-sniff.c $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC $< -ldl -shared -m64 -o $@

%.o: %.c $(DEPENDS)
	$(CROSS_CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install: $(BIN) $(BIN).1
	$(INSTALL) -D -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(INSTALL) -D -m 644 $(BIN).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

clean:
	-$(RM) $(OBJS) $(BIN) $(MANGEN) $(BIN).1 $(BIN).1.tmp libusb-sniff-32.so libusb-sniff-64.so
