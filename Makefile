ifeq ($(OS), Windows_NT)
	CC := x86_64-w64-mingw32-gcc
else
	CC := cc
endif

CFLAGS := -Wall -Wextra -Wpedantic

.PHONY: all
all: bass

.PHONY: release
release: src/*.c src/*.h
	${CC} ${CFLAGS} -O3 src/*.c -o bass

bass: src/*.c src/*.h
	${CC} ${CFLAGS} -ggdb src/*.c -o bass

test.bass:
	touch $@
	
.PHONY: compile
compile: bass test.bass
	./bass -c test.bass

bass-compiled: bass test.bass
	./bass -c test.bass -o bass-compiled.s
	as -g -o bass-compiled.o bass-compiled.s && ld bass-compiled.o -o bass-compiled

.PHONY: run
run: bass test.bass
	./bass test.bass

.PHONY: examples
examples: bass examples/
	for file in examples/*.bass; do  echo $$file; ./bass $$file; echo ""; done

.PHONY: clean
clean:
	rm bass
