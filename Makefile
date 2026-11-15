PROG =	xlt88
OBJS = comread.o rmac.o conbuf.o storage.o reset.o fcbdef.o fcbaddr.o flags.o mexpr.o macro.o m80.o mac.o fcb.o xlt88.o dialect.o symbols.o state.o expr.o layout.o regions.o norm8080.o \
	osrecog.o trans.o emitter.o target.o shrink.o nasm.o asm86.o asmfmt.o masm.o drasm86.o msdos.o cpm86.o
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
	rm -f ${PROG} ${OBJS} tests/iface tests/expr tests/readers tests/shrink tests/osrecog tests/flags tests/fcbaddr

tests/iface: tests/iface.c ${OBJS}
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/iface.c $(filter-out xlt88.o,${OBJS})

tests/expr: tests/expr.c expr.o expr.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/expr.c expr.o

tests/readers: tests/readers.c dialect.o mexpr.o macro.o m80.o mac.o rmac.o symbols.o layout.o expr.o dialect.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/readers.c dialect.o mexpr.o macro.o m80.o mac.o rmac.o symbols.o layout.o expr.o

test: ${PROG} tests/iface tests/expr tests/readers tests/shrink tests/osrecog tests/flags tests/fcbaddr
	./tests/iface
	./tests/expr
	./tests/readers
	./tests/shrink
	./tests/osrecog
	./tests/flags
	./tests/fcbaddr
	${PYTHON} tests/shrink.py
	${PYTHON} tests/textfmt.py
	${PYTHON} tests/flagflow.py
	${PYTHON} tests/andlive.py
	${PYTHON} tests/login.py
	${PYTHON} tests/attrs.py
	${PYTHON} tests/users.py
	${PYTHON} tests/protect.py
	${PYTHON} tests/reset.py
	${PYTHON} tests/iobyte.py
	${PYTHON} tests/farptr.py
	${PYTHON} tests/bdosall.py
	${PYTHON} tests/allreset.py
	${PYTHON} tests/readers.py
	${PYTHON} tests/mac.py
	${PYTHON} tests/rmac.py
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
	${PYTHON} tests/flow.py
	${PYTHON} tests/ossets.py
	${PYTHON} tests/memsvc.py
	${PYTHON} tests/native.py
	${PYTHON} tests/devices.py
	${PYTHON} tests/regions.py
	${PYTHON} tests/console.py
	${PYTHON} tests/conbuf.py
	${PYTHON} tests/forth.py
	${PYTHON} tests/arith.py
	${PYTHON} tests/cpufinal.py
	${PYTHON} tests/operands.py
	${PYTHON} tests/comread.py
	${PYTHON} tests/state.py
	${PYTHON} tests/layout.py
	${PYTHON} tests/fileops.py
	${PYTHON} tests/randio.py
	${PYTHON} tests/dosfcb.py
	${PYTHON} tests/storage.py
	${PYTHON} tests/fcbflow.py
	${PYTHON} tests/fcbdef.py
	${PYTHON} tests/dosrand.py
	${PYTHON} tests/dosmore.py
	${PYTHON} tests/dosnames.py
	${PYTHON} tests/drives.py
	${PYTHON} tests/vtlbench.py
	${PYTHON} tests/vtlscr.py
	${PYTHON} tests/wash.py
	${PYTHON} tests/washfile.py
	${PYTHON} tests/washport.py
	${PYTHON} tests/diskref.py

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

corpus: ${PROG}
	${PYTHON} tests/corpus.py

.PHONY: corpus

dialect.o: expr.h

osrecog.o: symbols.h

trans.o regions.o: regions.h
regions.o: symbols.h dialect.h

vtlbench: ${PROG}
	${PYTHON} tests/vtlbench.py

.PHONY: vtlbench

shrink.o: target.h emitter.h expr.h

tests/shrink: tests/shrink.c target.o shrink.o emitter.o expr.o
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/shrink.c target.o shrink.o emitter.o expr.o

mac.o: dialect.h expr.h

tests/osrecog: tests/osrecog.c ${OBJS} osrecog.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/osrecog.c $(filter-out xlt88.o,${OBJS})

flags.o trans.o: flags.h
flags.o: dialect.h

tests/flags: tests/flags.c flags.o flags.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/flags.c flags.o

fcbaddr.o fcb.o: fcbaddr.h
fcbaddr.o: dialect.h

tests/fcbaddr: tests/fcbaddr.c fcbaddr.o fcbaddr.h
	${CC} ${CFLAGS} -I. ${LDFLAGS} -o $@ tests/fcbaddr.c fcbaddr.o

fcbdef.o fcb.o: fcbdef.h
fcbdef.o: fcbaddr.h symbols.h dialect.h

fcb.o fcbdef.o msdos.o: fcbapi.h
storage.o fcb.o: storage.h

reset.o cpm86.o msdos.o: reset.h
reset.o: emitter.h

conbuf.o msdos.o: conbuf.h emitter.h

rmac.o mac.o m80.o dialect.o: dialect.h
comread.o xlt88.o: comread.h
comread.o: dialect.h
