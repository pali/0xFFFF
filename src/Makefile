include ../config.mk

PREFIX ?= /usr/local
INSTALL ?= install

CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE
CFLAGS += -W -Wall -O2 -pedantic -std=c99
LIBS += -lusb

DEPENDS = Makefile ../config.mk

OBJS = main.o nolo.o printf-utils.o image.o fiasco.o device.o usb-device.o cold-flash.o operations.o local.o mkii.o disk.o cal.o
BIN = 0xFFFF
MANGEN = mangen

all: $(BIN) $(BIN).1

$(BIN): $(OBJS) $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

$(MANGEN): $(MANGEN).o $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BIN).1: $(BIN) $(MANGEN) $(DEPENDS)
	./$(MANGEN) > $@

libusb-sniff-32.so: libusb-sniff.c $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -ldl -shared -m32 -o $@ $<

libusb-sniff-64.so: libusb-sniff.c $(DEPENDS)
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -ldl -shared -m64 -o $@ $<

%.o: %.c $(DEPENDS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install: $(BIN) $(BIN).1
	$(INSTALL) -D -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(INSTALL) -D -m 644 $(BIN).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

clean:
	-$(RM) $(OBJS) $(BIN) $(MANGEN) $(MANGEN).o $(BIN).1 libusb-sniff-32.so libusb-sniff-64.so
