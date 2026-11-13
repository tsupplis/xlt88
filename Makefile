PROG =	xlt88
OBJS = layout.o expr.o symbols.o state.o xlt88.o dialect.o norm8080.o osrecog.o trans.o emitter.o target.o nasm.o msdos.o cpm86.o
PYTHON = python3

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} -o ${PROG} ${OBJS}

xlt88.o trans.o msdos.o cpm86.o: backend.h

xlt88.o trans.o: trans.h

xlt88.o trans.o emitter.o target.o nasm.o msdos.o cpm86.o: emitter.h
xlt88.o trans.o dialect.o norm8080.o osrecog.o: dialect.h
trans.o osrecog.o: osrecog.h

clean:
	rm -f ${PROG} ${OBJS} tests/iface tests/expr

tests/iface: tests/iface.c ${OBJS}
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/iface.c $(filter-out xlt88.o,${OBJS})

tests/expr: tests/expr.c expr.o expr.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/expr.c expr.o

test: ${PROG} tests/iface tests/expr
	./tests/iface
	./tests/expr
	sh tests/coverage.sh
	sh tests/smoke.sh
	${PYTHON} tests/arith.py
	${PYTHON} tests/state.py
	${PYTHON} tests/layout.py

.PHONY: all clean test

xlt88.o target.o: target.h

xlt88.o trans.o symbols.o state.o norm8080.o: symbols.h
symbols.o state.o: dialect.h

expr.o target.o nasm.o trans.o xlt88.o emitter.o msdos.o cpm86.o: expr.h

layout.o: symbols.h dialect.h expr.h
symbols.o state.o norm8080.o: expr.h
