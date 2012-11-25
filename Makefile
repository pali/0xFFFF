include config.mk
PREFIX?=/usr/local
DESTDIR?=

all:
	cd src && ${MAKE} all

static:
	cd src && ${MAKE} static

allusb:
	cd src && ${MAKE} allusb

clean:
	cd src && ${MAKE} clean

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp src/0xFFFF ${DESTDIR}${PREFIX}/bin

deinstall:
	rm -f ${DESTDIR}${PREFIX}/bin/0xFFFF
