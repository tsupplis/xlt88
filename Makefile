PROG =	xlt88
OBJS = mexpr.o macro.o m80.o fcb.o xlt88.o dialect.o symbols.o state.o expr.o layout.o norm8080.o \
	osrecog.o trans.o emitter.o target.o nasm.o asm86.o asmfmt.o masm.o drasm86.o msdos.o cpm86.o
PYTHON = python3

all: ${PROG}

${PROG}: ${OBJS}
	${CC} ${LDFLAGS} -o ${PROG} ${OBJS}

xlt88.o trans.o msdos.o cpm86.o: backend.h

xlt88.o trans.o: trans.h

xlt88.o trans.o emitter.o target.o nasm.o asm86.o asmfmt.o masm.o drasm86.o msdos.o cpm86.o: emitter.h
xlt88.o trans.o dialect.o norm8080.o osrecog.o: dialect.h
trans.o osrecog.o: osrecog.h

clean:
	rm -f ${PROG} ${OBJS} tests/iface tests/expr tests/readers

tests/iface: tests/iface.c ${OBJS}
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/iface.c $(filter-out xlt88.o,${OBJS})

tests/expr: tests/expr.c expr.o expr.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/expr.c expr.o

tests/readers: tests/readers.c dialect.o mexpr.o macro.o m80.o symbols.o layout.o expr.o dialect.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/readers.c dialect.o mexpr.o macro.o m80.o symbols.o layout.o expr.o

test: ${PROG} tests/iface tests/expr tests/readers
	./tests/iface
	./tests/expr
	./tests/readers
	${PYTHON} tests/readers.py
	${PYTHON} tests/preproc.py
	${PYTHON} tests/macros.py
	${PYTHON} tests/repeat.py
	${PYTHON} tests/macops.py
	${PYTHON} tests/setexpr.py
	${PYTHON} tests/radix.py
	sh tests/coverage.sh
	sh tests/smoke.sh
	${PYTHON} tests/emitters.py
	XLT_TARGET=msdos ${PYTHON} tests/emitters.py
	XLT_EMIT=masm3 ${PYTHON} tests/emitters.py
	XLT_TARGET=msdos XLT_EMIT=masm3 ${PYTHON} tests/emitters.py
	XLT_EMIT=drasm86 ${PYTHON} tests/emitters.py
	XLT_TARGET=msdos XLT_EMIT=drasm86 ${PYTHON} tests/emitters.py
	${PYTHON} tests/arith.py
	${PYTHON} tests/state.py
	${PYTHON} tests/layout.py
	${PYTHON} tests/fileops.py
	${PYTHON} tests/randio.py
	${PYTHON} tests/dosfcb.py

.PHONY: all clean test

xlt88.o target.o: target.h

xlt88.o trans.o symbols.o state.o norm8080.o: symbols.h
symbols.o state.o: dialect.h

expr.o target.o nasm.o trans.o xlt88.o emitter.o msdos.o cpm86.o: expr.h

layout.o: symbols.h dialect.h expr.h
symbols.o state.o norm8080.o: expr.h

fcb.o trans.o: fcb.h
fcb.o: symbols.h dialect.h expr.h

m80.o: dialect.h symbols.h expr.h

macro.o m80.o: macro.h
macro.o: dialect.h symbols.h

mexpr.o macro.o m80.o: mexpr.h

asm86.o: target.h emitter.h expr.h

xlt88.o asm86.o tests/iface: asm86.h

asm86.o asmfmt.o masm.o drasm86.o: asmfmt.h target.h emitter.h expr.h
xlt88.o masm.o tests/iface: masm.h

xlt88.o drasm86.o tests/iface: drasm86.h
