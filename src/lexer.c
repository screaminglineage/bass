#include "constants.h"
#include "utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"

typedef struct {
    const char *name;
    int arity; // no of arguments it takes
} OpCodeData;

OpCodeData OPCODES[OP_COUNT] = {[OP_NO] = {.name = "nop", .arity = 0},
                                [OP_ADD] = {.name = "add", .arity = 3},
                                [OP_SUB] = {.name = "sub", .arity = 3},
                                [OP_MUL] = {.name = "mul", .arity = 3},
                                [OP_DIV] = {.name = "div", .arity = 3},
                                [OP_MOD] = {.name = "mod", .arity = 3},
                                [OP_MOVE] = {.name = "move", .arity = 2},
                                [OP_PRINT] = {.name = "print", .arity = 1},
                                [OP_PUSH] = {.name = "push", .arity = 1},
                                [OP_POP] = {.name = "pop", .arity = 1},
                                [OP_JUMP] = {.name = "jump", .arity = 1}};

void lexer_init(Lexer *lexer, StringView source_code) {
    lexer->source = source_code;
    lexer->start = 0;
    lexer->end = 0;
}

char next(Lexer *lexer) {
    if (lexer->end < lexer->source.length) {
        char next = lexer->source.data[lexer->end];
        lexer->end++;
        return next;
    }
    return '\0';
}

char peek(Lexer *lexer) {
    if (lexer->end < lexer->source.length) {
        return lexer->source.data[lexer->end];
    }
    return '\0';
}

char current(Lexer *lexer) {
    if (lexer->end > 0) {
        return lexer->source.data[lexer->end - 1];
    }
    return '\0';
}

#define get_string(lexer)                                                      \
    (StringView) {                                                             \
        &(lexer)->source.data[(lexer)->start], (lexer)->end - (lexer)->start   \
    }

// #define get_slice(lexer, start, end)
// (StringView){&(lexer)->source.data[(start)], (end) - (start)}

bool parse_num(Lexer *lexer, long *num, StringView *string) {
    while (isalnum(peek(lexer))) {
        next(lexer);
    }
    if (!(isspace(peek(lexer)) || peek(lexer) == '\0')) {
        fprintf(stderr, "bass: unexpected character: `%c` at: %zu\n",
                peek(lexer), lexer->end);
        return false;
    }
    *string = get_string(lexer);
    if (string->length <= 1) {
        fprintf(stderr, "bass: expected value at: %zu\n", lexer->start);
        return false;
    }
    *num = strtol(&lexer->source.data[lexer->start + 1], NULL, 0);
    return true;
}

StringView parse_identifier(Lexer *lexer) {
    while (isalnum(peek(lexer))) {
        next(lexer);
    }
    return get_string(lexer);
}

bool get_opcode(char *string, OpType *type) {
    for (size_t i = 0; i < OP_COUNT; i++) {
        if (strcmp(string, OPCODES[i].name) == 0) {
            *type = i;
            return true;
        }
    }
    return false;
}

bool parse_operands(Lexer *lexer, OpType type, Operand operands[MAX_OPERANDS]) {
    int i = 0;
    while (i < OPCODES[type].arity) {
        char current = next(lexer);
        long num;
        StringView string;
        switch (current) {
        case 'r': {
            if (!parse_num(lexer, &num, &string)) {
                return false;
            }
            if (num >= REG_COUNT) {
                fprintf(
                    stderr,
                    "bass: invalid register: `%ld` at %zu. Registers can range "
                    "from 0 to %d\n",
                    num, lexer->start, REG_COUNT);
                return false;
            }
            operands[i++] = (Operand){TOK_REGISTER, string, num};
        } break;
        case '#': {
            if (!parse_num(lexer, &num, &string)) {
                return false;
            }
            operands[i++] = (Operand){TOK_VALUE, string, num};
        } break;
        case '@': {
            if (!parse_num(lexer, &num, &string)) {
                return false;
            }
            operands[i++] = (Operand){TOK_ADDRESS, string, num};
        } break;
        default: {
            if (!isspace(current)) {
                fprintf(stderr,
                        "bass: not enough operands for opcode `%s`, expected "
                        "%d but got %d\n",
                        OPCODES[type].name, OPCODES[type].arity, i);
                return false;
            }
        }
        }
        lexer->start = lexer->end;
    }
    return true;
}

bool parse_label(Lexer *lexer, Operand *operand) {
    if (isalpha(next(lexer))) {
        StringView string = parse_identifier(lexer);
        lexer->start = lexer->end;
        *operand = (Operand){TOK_LABEL, string, -1};
        return true;
    }
    return false;
}

bool lex(Lexer *lexer, OpCodes *opcodes, Labels *labels) {
    char current;
    size_t op_index = 0;
    while ((current = next(lexer))) {
        if (isalpha(current)) {
            StringView string = parse_identifier(lexer);
            lexer->start = lexer->end;
            char next_char = next(lexer);
            if (next_char == ':') {
                Label label = {string, op_index};
                dyn_append(labels, label);
            } else if ((isspace(next_char) || next_char == '\0')) {
                OpType op_type;
                char *str = string_view_to_cstring(string);
                if (!get_opcode(str, &op_type)) {
                    fprintf(stderr, "bass: invalid opcode `%s` at: %zu\n", str,
                            lexer->start);
                    free(str);
                    return false;
                }
                free(str);
                lexer->start = lexer->end;
                Operand operands[MAX_OPERANDS] = {0};
                if (op_type == OP_JUMP) {
                    if (!parse_label(lexer, &operands[0])) {
                        return false;
                    }
                } else {
                    if (!parse_operands(lexer, op_type, operands)) {
                        return false;
                    }
                }
                OpCode opcode;
                opcode.op = op_type;
                memcpy(&opcode.operands, operands,
                       sizeof(Operand) * MAX_OPERANDS);
                dyn_append(opcodes, opcode);
            } else {
                fprintf(stderr, "bass: unexpected character `%c` at: %zu\n",
                        next_char, lexer->end);
                return false;
            }
        } else if (!(isspace(current) || current == '\0')) {
            fprintf(stderr,
                    "bass: expected opcode or label, got `%c` at: %zu\n",
                    current, lexer->end);
            return false;
        }
        lexer->start = lexer->end;
    }
    return true;
}

void display_opcodes(OpCodes ops) {
    for (size_t i = 0; i < ops.size; i++) {
        OpCode t = ops.data[i];
        printf("OpCode: %s\n", OPCODES[t.op].name);
        for (int i = 0; i < OPCODES[t.op].arity; i++) {
            int val = t.operands[i].value;
            switch (t.operands[i].type) {
            case TOK_REGISTER:
                printf("\tREGISTER: %d\n", val);
                break;
            case TOK_VALUE:
                printf("\tVALUE: %d\n", val);
                break;
            case TOK_ADDRESS:
                printf("\tADDRESS: %d\n", val);
                break;
            case TOK_LABEL: {
                char *str = string_view_to_cstring(t.operands[i].string);
                printf("\tLABEL: %s (to opcode: %d)\n", str, val);
                free(str);
            } break;
            }
        }
    }
}

void display_labels(Labels lbls) {
    for (size_t i = 0; i < lbls.size; i++) {
        Label t = lbls.data[i];
        char *str = string_view_to_cstring(t.name);
        printf("Label: %s\n", str);
        free(str);
    }
}
