include ../config.mk
OBJ=main.o fiasco.o hexdump.o dump.o flash.o serial.o
OBJ+=hash.o fpid.o query.o pieces.o utils.o devices.o console.o
BIN=0xFFFF
#LIBS=`${HAVE_USB} && echo -- '-lusb'`
LIBS=`if [ "${HAVE_USB}" = 1 ]; then echo '-lusb'; fi`
CFLAGS+=-DVERSION=\"${VERSION}\" -Wall -g -I .
CFLAGS+=-pedantic -std=c99 -DHAVE_USB=${HAVE_USB}
USBOBJ=../libusb/error.o ../libusb/usb.o ../libusb/descriptors.o
USBOBJ+=../libusb/linux.o ## XXX not portable

all: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} ${LIBS}

allusb: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} ${USBOBJ}

static: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} -static ${OBJ} ${USBOBJ}

clean:
	-rm -f ${OBJ} ${BIN}
