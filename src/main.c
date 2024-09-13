#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "interpreter.h"
#include "parser.h"
#include "utils.h"

/* int main(int argc, char **argv) { */
/*     (void)argc; */
/**/
/*     StringView source; */
/*     if (!read_to_string(argv[1], &source)) { */
/*         return 1; */
/*     } */
/*     Tokens tokens = {0}; */
/*     OpCodes opcodes = {0}; */
/*     if (!parse_source(source.data, &tokens, &opcodes)) { */
/*         return 1; */
/*     } */
/*     tokens_print(tokens); */
/*     opcodes_print(tokens, opcodes); */
/**/
/*     State state = {0}; */
/*     execute(&state, tokens, opcodes); */
/*     return 0; */
/* } */

int main(int argc, char *argv[]) {
    (void)argc;

    StringView sv;
    read_to_string(argv[1], &sv);
    Parser p;
    parser_init(&p, sv);
    OpCodes opcodes = {0};
    Labels labels = {0};
    parse(&p, &opcodes, &labels) ? printf("Successfully parsed!\n")
                                 : printf("Failed to parse!\n");
    patch_labels(&opcodes, labels);

    display_opcodes(opcodes);
    display_labels(labels);
    State state = {0};
    interpret(&state, opcodes);
    return 0;
}
