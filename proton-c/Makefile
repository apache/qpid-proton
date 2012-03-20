CFLAGS := -Wall -Werror -pedantic-errors -std=c99 -g -Iinclude -fPIC
PYTHON := python
PYTHONPATH := ${realpath .}
UTIL_SRC := src/util.c
CODEC_SRC := src/codec/codec.c
FRAMING_SRC := src/framing/framing.c
VALUE_SRC := src/types/value.c src/types/array.c src/types/list.c \
	src/types/map.c src/types/string.c src/types/binary.c \
	src/types/symbol.c src/types/decode.c
UTIL_HDR := include/proton/util.h
VALUE_HDR := include/proton/value.h
DISPATCHER_SRC := src/dispatcher/dispatcher.c
ENGINE_SRC := src/engine/engine.c
SASL_SRC := src/sasl/sasl.c
MESSAGE_SRC := src/message/message.c
DRIVER_SRC := src/driver.c

SRCS := ${UTIL_SRC} ${VALUE_SRC} ${FRAMING_SRC} ${CODEC_SRC} ${PROTOCOL_SRC} \
	${DISPATCHER_SRC} ${ENGINE_SRC} ${SASL_SRC} ${MESSAGE_SRC} ${DRIVER_SRC}
OBJS := ${SRCS:.c=.o}
DEPS := ${OBJS:.o=.d}
HDRS := ${UTIL_HDR} ${VALUE_HDR} \
	${FRAMING_SRC:src/framing/%.c=include/proton/%.h} \
	${CODEC_SRC:src/codec/%.c=include/proton/%.h} \
        src/protocol.h \
	include/proton/engine.h \
	include/proton/sasl.h \
	include/proton/message.h \
	src/codec/encodings.h

PROGRAMS := src/proton
LIBRARY := src/libqpidproton.so

MODULE := src/proton.py
SWIG_INT := src/proton.i

all: ${PROGRAMS} ${LIBRARY} ${MODULE}

# pull in dependency info for *existing* .o files
-include ${DEPS}

${PROGRAMS}: ${OBJS}

${LIBRARY}: ${OBJS}
	$(CC) $(CCFLAGS) -shared -o $@ $^

${MODULE}: ${SWIG_INT} ${HDRS} ${LIBRARY}
	swig -python -Iinclude ${SWIG_INT}
	$(CC) $(CCFLAGS) -fPIC -Iinclude -I/usr/include/python2.7 -Lsrc \
		-lqpidproton -shared -o src/_proton.so src/proton_wrap.c

${OBJS}: ${HDRS}
${OBJS}: %.o: %.c
	gcc -c -MMD -MP $(CFLAGS) $*.c -o $*.o

%.h: %.h.py
	PYTHONPATH=${PYTHONPATH} ${PYTHON} $< > $@

%.c: %.c.py
	PYTHONPATH=${PYTHONPATH} ${PYTHON} $< > $@

clean:
	rm -f ${PROGRAMS} ${LIBRARY} ${MODULE} ${OBJS} ${DEPS} src/protocol.c \
	src/protocol.h src/codec/encodings.h src/*.pyc \
	src/proton_wrap.c src/_proton.so
