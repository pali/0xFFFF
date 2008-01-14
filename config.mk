VERSION=0.3.2
PREFIX=/usr

# NetBSD stuff
CFLAGS+=-I/usr/pkg/include
LDFLAGS+=-L/usr/pkg/lib -Wl,-R/usr/pkg/lib

HAVE_USB=1
