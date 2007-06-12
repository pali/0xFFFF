VERSION=0.2
OBJ=main.o fiasco.o hexdump.o dump.o flash.o serial.o
OBJ+=hash.o fpid.o query.o pieces.o utils.o devices.o console.o
BIN=0xFFFF
CFLAGS+=-DVERSION=\"${VERSION}\" -Wall -g -I .
USBOBJ=../libusb/error.o ../libusb/usb.o ../libusb/descriptors.o
USBOBJ+=../libusb/linux.o ## XXX not portable

all: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} -lusb

allusb: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} ${USBOBJ}

static: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} -static ${OBJ} ${USBOBJ}

clean:
	-rm -f ${OBJ} ${BIN}
