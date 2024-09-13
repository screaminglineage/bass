#ifndef BASS_LEXER_H
#define BASS_LEXER_H

#include "utils.h"
#include "constants.h"

typedef struct {
    StringView source;
    size_t start;
    size_t end;
} Lexer;

typedef enum { TOK_REGISTER, TOK_VALUE, TOK_ADDRESS, TOK_LABEL } TokenType;

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
    OP_JUMP,

    OP_COUNT
} OpType;

typedef struct {
    const char *name;
    int arity; // no of arguments it takes
} OpCodeData;

static const OpCodeData OPCODES[OP_COUNT] = {[OP_NO] = {.name = "nop", .arity = 0},
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

void lexer_init(Lexer *lexer, StringView source_code);
bool lex(Lexer *lexer, OpCodes *opcodes, Labels *labels);
void patch_labels(OpCodes *opcodes, Labels labels);
void display_opcodes(OpCodes ops);
void display_labels(Labels ops);

#endif



