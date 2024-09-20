#ifndef BASS_PARSER_H
#define BASS_PARSER_H

#include "constants.h"
#include "utils.h"

typedef struct {
    StringView source;
    size_t start;
    size_t end;
    size_t line_start;
    int line;
} Parser;

typedef enum {
    TOK_REGISTER,
    TOK_LITERAL_NUM,
    TOK_LITERAL_CHAR,
    TOK_LITERAL_STR,
    TOK_ADDRESS,
    TOK_ADDRESS_REG,
    TOK_LABEL
} TokenType;

static const char *const TOKEN_STRING[TOK_LABEL + 1] = {
    [TOK_REGISTER]          = "register",
    [TOK_LITERAL_NUM]       = "numeric literal",
    [TOK_LITERAL_CHAR]      = "character literal",
    [TOK_LITERAL_STR]       = "string literal",
    [TOK_ADDRESS]           = "address",
    [TOK_ADDRESS_REG]       = "address register",
    [TOK_LABEL]             = "label",
};

typedef enum {
    OP_NO,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_MOVE,
    OP_LOAD,
    OP_STORE,
    OP_PRINT,
    OP_PRINTLN,
    OP_PUSH,
    OP_POP,
    OP_CMP,
    OP_JUMP,
    OP_JUMPZ,
    OP_JUMPG,
    OP_JUMPL,

    OP_COUNT
} OpType;

typedef struct {
    const char *name;
    int arity; // no of arguments it takes
} OpCodeData;

static const OpCodeData OPCODES[OP_COUNT] = {
    [OP_NO] = {.name = "nop", .arity = 0},
    [OP_ADD] = {.name = "add", .arity = 3},
    [OP_SUB] = {.name = "sub", .arity = 3},
    [OP_MUL] = {.name = "mul", .arity = 3},
    [OP_DIV] = {.name = "div", .arity = 3},
    [OP_MOD] = {.name = "mod", .arity = 3},
    [OP_MOVE] = {.name = "move", .arity = 2},
    [OP_LOAD] = {.name = "load", .arity = 2},
    [OP_STORE] = {.name = "store", .arity = 2},
    [OP_PRINT] = {.name = "print", .arity = 1},
    [OP_PRINTLN] = {.name = "println", .arity = 1},
    [OP_PUSH] = {.name = "push", .arity = 1},
    [OP_POP] = {.name = "pop", .arity = 1},
    [OP_CMP] = {.name = "cmp", .arity = 2},
    [OP_JUMP] = {.name = "jump", .arity = 1},
    [OP_JUMPZ] = {.name = "jumpz", .arity = 1},
    [OP_JUMPG] = {.name = "jumpg", .arity = 1},
    [OP_JUMPL] = {.name = "jumpl", .arity = 1}};

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

#define get_string(parser)                                                     \
    (StringView) {                                                             \
        &(parser)->source.data[(parser)->start],                               \
            (parser)->end - (parser)->start                                    \
    }

// #define get_slice(parser, start, end)
// (StringView){&(parser)->source.data[(start)], (end) - (start)}

#define get_col(parser) ((parser)->end - (parser)->line_start)

void parser_init(Parser *parser, StringView source_code);
bool parse(Parser *parser, OpCodes *opcodes, Labels *labels);
bool patch_labels(OpCodes *opcodes, Labels labels);
void display_opcodes(OpCodes ops);
void display_labels(Labels ops);

#endif
