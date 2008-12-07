OBJ=logotool.o compress.o uncompress.o rgb2yuv.o
CFLAGS=-Wall -g
BIN=logotool

all: ${BIN}

${BIN}: ${OBJ}
	${CC} ${OBJ} -o logotool

clean:
	rm -f ${OBJ} logotool
