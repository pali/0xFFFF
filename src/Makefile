include ../config.mk

PREFIX ?= /usr/local
INSTALL ?= install

CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE
CFLAGS += -W -Wall -O2 -pedantic -std=c99
LIBS += -lusb

OBJS = main.o nolo.o printf-utils.o image.o fiasco.o device.o usb-device.o cold-flash.o operations.o local.o mkii.o disk.o cal.o
BIN = 0xFFFF
MANGEN = mangen

all: $(BIN) $(BIN).1

$(BIN): $(OBJS) Makefile ../config.mk
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

$(MANGEN): $(MANGEN).o Makefile ../config.mk
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BIN).1: $(BIN) $(MANGEN)
	./$(MANGEN) > $@

%.o: %.c Makefile ../config.mk
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install: $(BIN) $(BIN).1
	$(INSTALL) -D -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(INSTALL) -D -m 644 $(BIN).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	$(RM) $(DESTDIR)$(PREFIX)/share/man/man1/$(BIN).1

clean:
	-$(RM) $(OBJS) $(BIN) $(MANGEN) $(MANGEN).o $(BIN).1
