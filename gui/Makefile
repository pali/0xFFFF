OBJ=main.o
BIN=goxf

CFLAGS=`pkg-config gladeui-1.0 libglade-2.0 --cflags`
LDFLAGS=`pkg-config gladeui-1.0 libglade-2.0 --libs`

all: ${OBJ}
	${CC} ${LDFLAGS} ${OBJ} -o ${BIN}
