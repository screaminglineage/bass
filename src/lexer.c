#include "constants.h"
#include "utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    StringView source;
    size_t start;
    size_t end;
} Lexer;

typedef enum {
    TOK_REGISTER,
    TOK_VALUE,
    TOK_ADDRESS,
} TokenType;

typedef enum {
    OP_NO,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_MOVE,
    OP_PRINT,
    OP_PUSH,
    OP_POP,

    OP_COUNT
} OpType;

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
                                [OP_POP] = {.name = "pop", .arity = 1}};

typedef struct {
    TokenType type;
    StringView string;
    int value;
} Operand;

typedef struct {
    OpType op;
    Operand operands[MAX_OPERANDS];
} OpCode;

typedef struct {
    OpCode *data;
    size_t size;
    size_t capacity;
} OpCodes;

typedef struct {
    StringView name;
    size_t index; // index of next opcode
} Label;

typedef struct {
    Label *data;
    size_t size;
    size_t capacity;
} Labels;

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

// bool string_view_to_int(StringView string, int *result) {
//     *result = 0;
//     for (size_t i=0; i<string.length; i++) {
//         if ('0' <= string.data[i] && string.data[i] <= '9') {
//             *result = *result * 10 + (string.data[i] - '0');
//         } else {
//             return false;
//         }
//     }
//     return true;
// }

bool parse_num(Lexer *lexer, long *num, StringView *string) {
    while (isalnum(peek(lexer))) {
        next(lexer);
    }
    if (!(isspace(peek(lexer)) || peek(lexer) == '\0')) {
        fprintf(stderr, "bass: unexpected character: `%c` at: %zu\n", peek(lexer), lexer->end);
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

StringView lex_identifier(Lexer *lexer) {
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
    char current;
    int i = 0;
    while (i < OPCODES[type].arity && (current = next(lexer))) {
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
                if (current == '\0') {
                    fprintf(stderr, "bass: not enough operands for opcode `%s`\n",
                        OPCODES[type].name);
                    return false;
                } else if (!isspace(current)) {
                    fprintf(stderr, "bass: unexpected character `%c` at: %zu\n",
                        current, lexer->end);
                    return false;
                }
            }
        }
        lexer->start = lexer->end;
    }
    return true;
}

bool lex(Lexer *lexer, OpCodes *opcodes, Labels *labels) {
    char current;
    size_t op_index = 0;
    while ((current = next(lexer))) {
        if (isalpha(current)) {
            StringView string = lex_identifier(lexer);
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
                if (!parse_operands(lexer, op_type, operands)) {
                    return false;
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
            fprintf(stderr, "bass: unexpected character `%c` at: %zu\n",
                    current, lexer->end);
            return false;
        }
        lexer->start = lexer->end;
    }
    return true;
}

// void tokens_print(Operand tokens) {
//     for (size_t i=0; i<tokens.size; i++) {
//         Operand t = tokens.data[i];
//         switch (t.type) {
//             case TOK_LABEL: {
//                 printf("LABEL: ");
//                 string_view_print(t.string);
//                 printf("\n");
//             } break;
//             case TOK_OPCODE: {
//                 printf("OPCODE:");
//                 string_view_print(t.string);
//                 printf("\n");
//             } break;
//             case TOK_REGISTER:
//                 printf("REGISTER: %d\n", t.reg_num);
//                 break;

//             case TOK_VALUE:
//                 printf("VALUE: %d\n", t.value);
//                 break;
//             case TOK_ADDRESS:
//                 printf("ADDRESS: %d\n", t.value);
//                 break;
//         }

//     }
// }

int main(int argc, char *argv[]) {
    StringView sv;
    read_to_string(argv[1], &sv);
    Lexer l;
    lexer_init(&l, sv);
    OpCodes tokens = {0};
    Labels labels = {0};
    lex(&l, &tokens, &labels) ? printf("Successfully Lexed!\n")
                              : printf("Failed to Lex!\n");
    // tokens_print(tokens);
    return 0;
}
