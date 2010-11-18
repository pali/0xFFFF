include config.mk
PREFIX?=/usr/local
DESTDIR?=

all: logot frontend
	cd src && ${MAKE} all

frontend:
	-cd src/gui && ${MAKE} all

static: logot
	cd libusb && ${MAKE} all
	cd src && ${MAKE} static
	cd logotool && ${MAKE} static

allusb: logot
	cd libusb && ${MAKE} all
	cd src && ${MAKE} allusb

logot:
	cd logotool && ${MAKE} all

clean:
	cd src && ${MAKE} clean
	cd libusb && ${MAKE} clean
	cd logotool && ${MAKE} clean

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/share/applications/
	cp src/0xFFFF ${DESTDIR}${PREFIX}/bin
	-cp src/gui/goxf ${DESTDIR}${PREFIX}/bin
	cp logotool/logotool ${DESTDIR}${PREFIX}/bin
	cp 0xFFFF.desktop ${DESTDIR}${PREFIX}/share/applications/
	mkdir -p ${DESTDIR}${PREFIX}/share/man/man1
	cp man/*.1 ${DESTDIR}${PREFIX}/share/man/man1

deinstall:
	rm -f ${DESTDIR}${PREFIX}/bin/0xFFFF
	rm -f ${DESTDIR}${PREFIX}/bin/logotool
	rm -f ${DESTDIR}${PREFIX}/bin/goxf
	rm ${DESTDIR}${PREFIX}/share/applications/0xFFFF.desktop
