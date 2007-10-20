OBJ=main.o
BIN=goxf

CFLAGS=`pkg-config libgladeui-1.0 libglade-2.0 --cflags`
LDFLAGS=`pkg-config libgladeui-1.0 libglade-2.0 --libs`

all: ${OBJ}
	${CC} ${LDFLAGS} ${OBJ} -o ${BIN}
