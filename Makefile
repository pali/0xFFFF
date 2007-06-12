PREFIX?=/usr/local

all: logot
	cd src && ${MAKE} all

static: logot
	cd libusb && ${MAKE} all
	cd src && ${MAKE} static

allusb: logot
	cd libusb && ${MAKE} all
	cd src && ${MAKE} allusb

logot:
	cd logotool && ${MAKE} all

clean:
	cd src && ${MAKE} clean
	cd logotool && ${MAKE} clean

install:
	cp src/0xFFFF ${PREFIX}/bin
	cp logotool/logotool ${PREFIX}/bin

deinstall:
	rm -f ${PREFIX}/bin/0xFFFF
	rm -f ${PREFIX}/bin/logotool
