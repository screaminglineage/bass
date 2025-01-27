CFLAGS := -Wall -Wextra -Wpedantic

all: bass

release: src/main.c src/parser.c src/interpreter.c
	gcc ${CFLAGS} -O3 $^ -o bass

bass: src/main.c src/parser.c src/interpreter.c
	gcc ${CFLAGS} -ggdb $^ -o bass

test.bass:
	touch $@
	
run: bass test.bass
	./bass test.bass

clean:
	rm bass
