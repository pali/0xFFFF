include ../config.mk
OBJ=main.o hexdump.o dump.o nolo.o qmode.o
OBJ+=utils.o console.o
OBJ+=squeue/squeue.o
OBJ+=image.o fiasco.o device.o usb-device.o cold-flash.o
BIN=0xFFFF
#LIBS=`${HAVE_USB} && echo -- '-lusb'`
LIBS=`if [ "${HAVE_USB}" = 1 ]; then echo '-lusb'; fi`
CFLAGS+=-DVERSION=\"${VERSION}\" -Wall -I .
CFLAGS+=-pedantic -std=c99 -DHAVE_USB=${HAVE_USB} -Wno-unused-result
USBOBJ=../libusb/error.o ../libusb/usb.o ../libusb/descriptors.o
USBOBJ+=../libusb/linux.o ## XXX not portable

CFLAGS+=-I../libusb
LDFLAGS+=-L../libusb

CFLAGS+=-D_GNU_SOURCE

all: ${BIN}

${BIN}: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} ${LIBS}

allusb: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} ${USBOBJ}

static: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} -static ${OBJ} ${USBOBJ}

clean:
	-rm -f ${OBJ} ${BIN}
