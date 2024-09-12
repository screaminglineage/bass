#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "utils.h"


typedef struct {
    StringView source;
    size_t start;
    size_t end;
} Lexer;

typedef enum {
    TOK_OPCODE,
    TOK_LABEL,
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


const char *OPCODES[OP_COUNT] = {
    [OP_NO]     =   "nop",
    [OP_ADD]    =   "add",
    [OP_SUB]    =   "sub",
    [OP_MUL]    =   "mul",
    [OP_DIV]    =   "div",
    [OP_MOD]    =   "mod",
    [OP_MOVE]   =   "move",
    [OP_PRINT]  =   "print",
    [OP_PUSH]   =   "push",
    [OP_POP]    =   "pop"
};

typedef struct {
    TokenType type;
    StringView string;
    union {
        OpType op;      // for opcodes
        // char *name;     // for labels
        int reg_num;    // for registers
        int value;      // for values/addresses
    };
} Token;

typedef struct {
    Token *data;
    size_t size;
    size_t capacity;
} Tokens;

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

// char current(Lexer *lexer) {
//     if (lexer->end > 0) {
//         return lexer->source.data[lexer->end - 1];
//     }
//     return '\0';
// }

#define get_string(lexer) (StringView){&(lexer)->source.data[(lexer)->start], (lexer)->end - (lexer)->start}
// #define get_slice(lexer, start, end) (StringView){&(lexer)->source.data[(start)], (end) - (start)}

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


bool lex_register(Lexer *lexer, Tokens *tokens) {
    while (isdigit(peek(lexer))) {
        next(lexer);
    }
    if (!(isspace(peek(lexer)) || peek(lexer) == '\0')) {
        fprintf(stderr, "bass: unexpected character at: %zu\n", lexer->end);
        return false;
    }
    StringView string = get_string(lexer);
    if (string.length <= 1) {
        fprintf(stderr, "bass: invalid register at: %zu\n", lexer->start);
        return false;
    }
    long num = strtol(&lexer->source.data[lexer->start + 1], NULL, 10);
    if (num >= REG_COUNT) {
        fprintf(stderr, "bass: invalid register: `%ld` at %zu. Registers can range from 0 to %d\n", num, lexer->start, REG_COUNT);
        return false;
    }
    Token token = {
        TOK_REGISTER,
        string,
        .reg_num = num
    };
    dyn_append(tokens, token);
    return true;
}

bool lex_value(Lexer *lexer, Tokens *tokens) {
    while (isdigit(peek(lexer))) {
        next(lexer);
    }
    if (!(isspace(peek(lexer)) || peek(lexer) == '\0')) {
        fprintf(stderr, "bass: unexpected character at: %zu\n", lexer->end);
        return false;
    }
    StringView string = get_string(lexer);
    if (string.length <= 1) {
        fprintf(stderr, "bass: expected value after `#` at: %zu\n", lexer->start);
        return false;
    }
    long num = strtol(&lexer->source.data[lexer->start + 1], NULL, 10);
    Token token = {
        TOK_VALUE,
        string,
        .value = num
    };
    dyn_append(tokens, token);
    return true;
}


bool lex_address(Lexer *lexer, Tokens *tokens) {
    while (isdigit(peek(lexer))) {
        next(lexer);
    }
    if (!(isspace(peek(lexer)) || peek(lexer) == '\0')) {
        fprintf(stderr, "bass: unexpected character at: %zu\n", lexer->end);
        return false;
    }
    StringView string = get_string(lexer);
    if (string.length <= 1) {
        fprintf(stderr, "bass: expected value after `@` at: %zu\n", lexer->start);
        return false;
    }
    long num = strtol(&lexer->source.data[lexer->start + 1], NULL, 10);
    Token token = {
        TOK_VALUE,
        string,
        .value = num
    };
    dyn_append(tokens, token);
    return true;
}

StringView lex_identifier(Lexer *lexer) {
    while(isalnum(peek(lexer))) {
        next(lexer);
    }
    return get_string(lexer);
}

bool get_opcode(StringView string, OpType *type) {
    for (size_t i=0; i<OP_COUNT; i++) {
        if (strncmp(string.data, OPCODES[i], string.length) == 0) {
            *type = i;
            return true;
        }
    }
    return false;
}


bool lex(Lexer *lexer, Tokens *tokens) {
    char current;
    while ((current = next(lexer))) {
        switch (current) {
            case 'r': {
                if (!lex_register(lexer, tokens)) {
                    return false;
                }
            } break;
            case '#': {
                if (!lex_value(lexer, tokens)) {
                    return false;
                }
            } break;
            case '@': {
                if (!lex_address(lexer, tokens)) {
                    return false;
                }
            } break;
            default: {
                if (isalpha(current)) {
                    StringView string = lex_identifier(lexer);
                    Token token;
                    char peeked_char = peek(lexer);
                    if (peeked_char == ':') {
                        token = (Token) {
                            TOK_LABEL,
                            string,
                            {0}
                        };
                    } else if (isspace(peeked_char)) {
                        OpType opcode;
                        if (!get_opcode(string, &opcode)) {
                            fprintf(stderr, "bass: unexpected opcode at: %zu\n", lexer->start);
                            return false;
                        }
                        token = (Token) {
                            TOK_OPCODE,
                            string,
                            .op = opcode
                        };
                    } else {
                        fprintf(stderr, "bass: unexpected character at: %zu\n", lexer->end);
                        return false;
                    }
                dyn_append(tokens, token);
                next(lexer);
                } else if (!isspace(current)) {
                    fprintf(stderr, "bass: unexpected character at: %zu\n", lexer->end);
                    return false;
                }
            }
        }
        lexer->start = lexer->end;
    }
    return true;
}

int main(int argc, char *argv[]) {
    StringView sv;
    read_to_string(argv[1], &sv);
    Lexer l;
    lexer_init(&l, sv);
    Tokens tokens = {0};
    lex(&l, &tokens)? printf("Successfully Lexed!\n"): printf("Failed to Lex!\n");
    return 0;
}
