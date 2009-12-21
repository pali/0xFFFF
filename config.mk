VERSION=0.4.0
PREFIX=/usr


# NetBSD stuff
CFLAGS+=-I/usr/pkg/include -g -O2
LDFLAGS+=-L/usr/pkg/lib -Wl,-R/usr/pkg/lib

HAVE_USB=1
HAVE_GUI=1
