#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "interpreter.h"
#include "parser.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    (void)argc;

    StringView sv;
    read_to_string(argv[1], &sv);
    Parser p;
    parser_init(&p, sv);
    OpCodes opcodes = {0};
    Labels labels = {0};
    if (!parse(&p, &opcodes, &labels)) {
        printf("Failed to parse!\n");
        return 1;
    }
    printf("Successfully parsed!\n");
    patch_labels(&opcodes, labels);

    display_opcodes(opcodes);
    display_labels(labels);
    State state = {0};
    interpret(&state, opcodes);
    return 0;
}
