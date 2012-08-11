include ../config.mk

CPPFLAGS += -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE -I.
CFLAGS += -W -Wall -Wno-unused-parameter -Wno-unused-result -O2 -pedantic -std=c99
ifdef WITH_USB
CPPFLAGS += -DWITH_USB
LIBS += -lusb
endif

ifdef WITH_DEVICE
CPPFLAGS += -DWITH_DEVICE
endif

ifdef WITH_SQUEUES
CPPFLAGS += -DWITH_SQUEUES
endif

OBJS = main.o nolo.o printf-utils.o image.o fiasco.o device.o usb-device.o cold-flash.o dump.o qmode.o console.o squeue/squeue.o
BIN = 0xFFFF

all: $(BIN)

$(BIN): $(OBJS) Makefile ../config.mk
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

%.o: %.c Makefile ../config.mk
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	-rm -f $(OBJS) $(BIN)
