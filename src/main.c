#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "compiler.h"
#include "constants.h"
#include "interpreter.h"
#include "parser.h"
#include "utils.h"

// TODO: rename this function as it also compiles
bool parse_and_interpret(const char *source_file, bool debug, bool compile_file, const char *output_path) {
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
    // TODO: move parse, compile, and interpret into separate functions
    if (compile_file) {
        if (output_path == NULL) output_path = DEFAULT_COMPILER_OUTPUT;
        return compile(output_path, labels, opcodes, (entry_label == -1)? 0: (size_t)entry_label);
    }

    State state;
    if (!state_init(&state)) {
        printf("bass: failed to allocate enough memory, exiting\n");
        free((void *)sv.data);
        return false;
    }
    state.reg_pc = (entry_label == -1)? 0: labels.data[entry_label].index;

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
    const char *output_path = NULL;
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
    for (size_t i = 0; i < source_files.size; i++) {
        if (!parse_and_interpret(source_files.data[i], debug, compile, output_path)) {
            fprintf(stderr, "bass: failed to run `%s`\n", source_files.data[i]);
            return 1;
        }
    }

    return 0;
}
