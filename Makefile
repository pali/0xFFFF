include config.mk
PREFIX?=/usr/local
DESTDIR?=

all: frontend
	cd src && ${MAKE} all

frontend:
	-cd src/gui && ${MAKE} all

static:
	cd src && ${MAKE} static

allusb:
	cd src && ${MAKE} allusb

clean:
	cd src && ${MAKE} clean

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/share/applications/
	cp src/0xFFFF ${DESTDIR}${PREFIX}/bin
	-cp src/gui/goxf ${DESTDIR}${PREFIX}/bin
	cp 0xFFFF.desktop ${DESTDIR}${PREFIX}/share/applications/

deinstall:
	rm -f ${DESTDIR}${PREFIX}/bin/0xFFFF
	rm -f ${DESTDIR}${PREFIX}/bin/goxf
	rm ${DESTDIR}${PREFIX}/share/applications/0xFFFF.desktop
