#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "constants.h"
#include "interpreter.h"
#include "parser.h"
#include "utils.h"

bool parse_file(const char *source_file, bool debug, StringView *sv, OpCodes *opcodes, Labels *labels) {
    if (!read_to_string(source_file, sv)) {
        return false;
    }

    Parser p;
    parser_init(&p, *sv);
    if (!parse(&p, opcodes, labels)) {
        free((void *)sv->data);
        return false;
    }

    if (debug) {
        printf("Opcodes:\n");
        for (size_t i = 0; i < opcodes->size; i++) {
            display_opcode(opcodes->data[i]);
        }
        printf("\nLabels:\n");
        display_labels(*labels);
    }
    return true;
}

bool compile_program(Labels labels, OpCodes opcodes, const char *output_file) {
    int entry_label = find_label(&labels, SV("_"));
    if (entry_label == -1) {
        entry_label = 0;
    } else {
        entry_label = (size_t)entry_label;
    }
    if (!compile(output_file, labels, opcodes, entry_label)) return false;
    return true;
}


bool interpret_program(Labels labels, OpCodes opcodes) {
    State state;
    if (!state_init(&state)) {
        printf("bass: failed to allocate enough memory, exiting\n");
        return false;
    }
    int entry_label = find_label(&labels, SV("_"));
    state.reg_pc = (entry_label == -1)? 0: labels.data[entry_label].index;

    if (!interpret(&state, opcodes)) {
        free(state.memory);
        return false;
    }
    free(state.memory);
    return true;
}

void print_help() {
    fprintf(stderr, "usage: bass [--help|-h] [--debug|-d] [--compile|-c] [FILES ...]\n\n"
            "a simple interpreted language that mimics the look and "
            "feel of assembly\n\n"
            "options:\n"
            "  -h, --help            show this help message and exit\n"
            "  -c, --compile         compile bass into an executable file\n"
            "  -o, --output <path>   path to compiled output\n"
            "  -d, --debug           show some debug info before running file\n");
}

typedef struct {
    const char **data;
    size_t size;
    size_t capacity;
} FileNames;

int main(int argc, char *argv[]) {
    bool debug = false;
    bool compile = false;
    const char *output_path = DEFAULT_COMPILER_OUTPUT;
    FileNames source_files = {0};

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--debug") == 0) || (strcmp(argv[i], "-d") == 0)) {
            if (!debug) {
                printf("bass: enabling debug mode\n");
            }
            debug = true;
        } else if ((strcmp(argv[i], "--compile") == 0) || (strcmp(argv[i], "-c") == 0)) {
            compile = true;
        } else if ((strcmp(argv[i], "--output") == 0) || (strcmp(argv[i], "-o") == 0)) {
            if (i + 1 > argc) {
                printf("bass: option `%s` requires a valid path", argv[i]);
                return 1;
            }
            output_path = argv[i + 1];
            i += 1;
        } else if ((strcmp(argv[i], "--help") == 0) ||
                   (strcmp(argv[i], "-h") == 0)) {
            print_help();
            return 0;
        } else {
            dyn_append(&source_files, argv[i]);
        }
    }

    if (source_files.size == 0) {
        fprintf(stderr, "bass: no input files provided\n");
        return 1;
    }

    OpCodes opcodes = {0};
    Labels labels = {0};

    for (size_t i = 0; i < source_files.size; i++) {
        StringView sv = {0};
        if (!parse_file(source_files.data[i], debug, &sv, &opcodes, &labels)) return 1;

        if (compile) {
            // TODO: currently overwrites previous files when compiling multiple files
            if (!compile_program(labels, opcodes, output_path)) {
                fprintf(stderr, "bass: failed to compile `%s`\n", source_files.data[i]);
                return 1;
            }
            fprintf(stderr, "bass: compiled to `%s`\n", output_path);
        } else {
            if (!interpret_program(labels, opcodes)) {
                fprintf(stderr, "bass: failed to run `%s`\n", source_files.data[i]);
                return 1;
            }
        }
        // TODO: use a string builder instead to not have to free each iteration
        free((void*)sv.data);
        opcodes.size = 0;
        labels.size = 0;
    }

    free(opcodes.data);
    free(labels.data);
    return 0;
}
