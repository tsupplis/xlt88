PROG =	xlt88
OBJS =	xlt88.o msdos.o cpm86.o dialect.o

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} -o ${PROG} ${OBJS}

xlt88.o msdos.o cpm86.o: backend.h
xlt88.o dialect.o: dialect.h

clean:
	rm -f ${PROG} ${OBJS}

test: ${PROG}
	sh tests/mnemonic-coverage.sh
	sh tests/smoke.sh

.PHONY: all clean test
