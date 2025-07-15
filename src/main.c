#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "compiler.h"
#include "interpreter.h"
#include "parser.h"
#include "utils.h"

bool parse_and_interpret(const char *source_file, bool debug, bool compile_file) {
    StringView sv;
    if (!read_to_string(source_file, &sv)) {
        return false;
    }

    Parser p;
    parser_init(&p, sv);
    OpCodes opcodes = {0};
    Labels labels = {0};
    if (!parse(&p, &opcodes, &labels)) {
        free((void *)sv.data);
        return false;
    }

    if (debug) {
        printf("Opcodes:\n");
        for (size_t i = 0; i < opcodes.size; i++) {
            display_opcode(opcodes.data[i]);
        }
        printf("\nLabels:\n");
        display_labels(labels);
    }

    int entry_label = find_label(&labels, SV("_"));
    if (entry_label == -1) {
        entry_label = 0;
    }
    if (compile_file) {
        return compile(labels, opcodes, (size_t)entry_label);
    }

    State state;
    if (!state_init(&state)) {
        printf("bass: failed to allocate enough memory, exiting\n");
        free((void *)sv.data);
        return false;
    }
    state.reg_pc = labels.data[entry_label].index;

    if (!interpret(&state, opcodes)) {
        free((void *)sv.data);
        free(state.memory);
        return false;
    }
    free((void *)sv.data);
    free(state.memory);
    free(opcodes.data);
    return true;
}

void print_help() {
    fprintf(stderr, "usage: bass [--help|-h] [--debug|-d] [--compile|-c] [FILES ...]\n\n"
            "a simple interpreted language that mimics the look and "
            "feel of assembly\n\n"
            "options:\n"
            "  -h, --help       show this help message and exit\n"
            "  -c, --compile    compile bass into an executable file\n"
            "  -d, --debug      show some debug info before running file\n");
}

int main(int argc, char *argv[]) {
    bool debug = false;
    bool compile = false;
    int files_count = 0;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--debug") == 0) || (strcmp(argv[i], "-d") == 0)) {
            if (!debug) {
                printf("bass: enabling debug mode\n");
            }
            debug = true;
        } else if ((strcmp(argv[i], "--compile") == 0) || (strcmp(argv[i], "-c") == 0)) {
            compile = true;
        } else if ((strcmp(argv[i], "--help") == 0) ||
                   (strcmp(argv[i], "-h") == 0)) {
            print_help();
            return 0;
        } else {
            files_count++;
            // TODO: consider flags provided after the file in the command line
            if (!parse_and_interpret(argv[i], debug, compile)) {
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
