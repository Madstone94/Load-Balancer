EXECBIN = loadbalancer
GCC = gcc -std=c99 -g -pthread -Wextra -Wpedantic -O2 -Wshadow -D_GNU_SOURCE
OBJS = main.o Parameters.o threader.o

all: ${EXECBIN}
${EXECBIN}: ${OBJS}
	${GCC} -o $@ $^

%.o: %.c
	${GCC} -c $<

clean:
	rm -f ${EXECBIN} ${OBJS}
