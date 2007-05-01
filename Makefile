PREFIX?=/usr/local

all:
	cd src && ${MAKE} all
	cd logotool && ${MAKE} all

clean:
	cd src && ${MAKE} clean
	cd logotool && ${MAKE} clean

install:
	cp src/0xFFFF ${PREFIX}/bin
	cp logotool/logotool ${PREFIX}/bin
