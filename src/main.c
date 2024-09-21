#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "interpreter.h"
#include "parser.h"
#include "utils.h"

bool parse_and_interpret(const char *source_file, bool debug) {
    StringView sv;
    if (!read_to_string(source_file, &sv)) {
        return false;
    }

    Parser p;
    parser_init(&p, sv);
    OpCodes opcodes = {0};
    Labels labels = {0};
    if (!parse(&p, &opcodes, &labels)) {
        return false;
    }
    if (!patch_labels(&opcodes, labels)) {
        return false;
    }

    if (debug) {
        printf("Opcodes:\n");
        display_opcodes(opcodes);
        printf("\nLabels:\n");
        display_labels(labels);
    }

    State state;
    if (!state_init(&state)) {
        printf("bass: failed to allocate enough memory, exiting\n");
        return false;
    }
    if (!interpret(&state, opcodes)) {
        return false;
    }
    return true;
}

void print_help() {
    fprintf(stderr, "usage: bass [--help|-h] [--debug|-d] [FILES ...]\n\n"
                    "a simple interpreted language that mimics the look and "
                    "feel of assembly\n\n"
                    "options:\n"
                    "  -h, --help  show this help message and exit\n"
                    "  -d, --debug show some debug info before running file\n");
}

int main(int argc, char *argv[]) {
    bool debug = false;
    int files_count = 0;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--debug") == 0) || (strcmp(argv[i], "-d") == 0)) {
            if (!debug) {
                printf("bass: enabling debug mode\n");
            }
            debug = true;
        } else if ((strcmp(argv[i], "--help") == 0) ||
                   (strcmp(argv[i], "-h") == 0)) {
            print_help();
            return 0;
        } else {
            files_count++;
            if (!parse_and_interpret(argv[i], debug)) {
                fprintf(stderr, "bass: failed to run `%s`\n", argv[i]);
                return 1;
            }
        }
    }

    if (files_count == 0) {
        fprintf(stderr, "bass: no input files provided\n");
        return 1;
    }

    return 0;
}
