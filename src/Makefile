include ../config.mk

PREFIX ?= /usr/local
INSTALL ?= install

CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE
CFLAGS += -W -Wall -O2 -pedantic -std=c99
LIBS += -lm -lusb

OBJS = main.o nolo.o printf-utils.o image.o fiasco.o device.o usb-device.o cold-flash.o operations.o local.o mkii.o disk.o cal.o
BIN = 0xFFFF

all: $(BIN)

$(BIN): $(OBJS) Makefile ../config.mk
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

%.o: %.c Makefile ../config.mk
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install: $(BIN)
	$(INSTALL) -D -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	-$(RM) $(OBJS) $(BIN)
