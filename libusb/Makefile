#include ../config.mk
OBJ=linux.o usb.o descriptors.o error.o

all: ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} -fPIC -I . ${OBJ} -shared -o libusb.so

clean:
	-rm -f libusb.so ${OBJ}
