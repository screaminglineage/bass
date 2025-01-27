CFLAGS := -Wall -Wextra -Wpedantic

all: bass

release: src/main.c src/parser.c src/interpreter.c src/constants.h src/utils.h src/parser.h src/interpreter.h
	gcc ${CFLAGS} -O3 src/*.c -o bass

bass: src/main.c src/parser.c src/interpreter.c src/constants.h src/utils.h src/parser.h src/interpreter.h
	gcc ${CFLAGS} -ggdb src/*.c -o bass

test.bass:
	touch $@
	
run: bass test.bass
	./bass test.bass

clean:
	rm bass
