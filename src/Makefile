VERSION=0.2
OBJ=main.o fiasco.o hexdump.o dump.o flash.o
OBJ+=hash.o fpid.o query.o pieces.o utils.o devices.o
BIN=0xFFFF
CFLAGS+=-DVERSION=\"${VERSION}\" -Wall -g

all: ${OBJ}
	${CC} ${LDFLAGS} -o ${BIN} ${OBJ} -lusb

clean:
	-rm -f ${OBJ} ${BIN}
