#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define REG_COUNT 8

#define dyn_append(da, item)                                                      \
do {                                                                         \
    if ((da)->size == (da)->capacity) {                                        \
        (da)->capacity = ((da)->capacity <= 0) ? 1 : (da)->capacity * 2;         \
        (da)->data = realloc((da)->data, (da)->capacity * sizeof(*(da)->data));  \
        assert((da)->data && "Catastrophic Failure: Allocation failed!");        \
    }                                                                          \
    (da)->data[(da)->size++] = item;                                           \
} while (0)

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_MOVE,
    OP_PRINT,
    OP_COUNT
} Operation;

const char *OPERATIONS[OP_COUNT] = {
    [OP_ADD]    =   "add",
    [OP_SUB]    =   "sub",
    [OP_MUL]    =   "mul",
    [OP_DIV]    =   "div",
    [OP_MOD]    =   "mod",
    [OP_MOVE]   =   "move",
    [OP_PRINT]  =   "print",
};

typedef enum {
    TOK_OPCODE,
    TOK_REGISTER,
    TOK_VALUE,
    TOK_ADDRESS,
} TokenType;

typedef struct {
    char *value;
    TokenType type;
    union {
        Operation op; // only valid if it is an opcode
        int register_num;
    };
} Token;

typedef struct {
    Token *data;
    size_t size;
    size_t capacity;
} Tokens;

typedef struct {
    size_t start;
    size_t end; // points to the beginning of next opcode
} OpCode;

typedef struct {
    OpCode *data;
    size_t size;
    size_t capacity;
} OpCodes;

char *read_to_string(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *data = malloc(size + 1);
    fread(data, size, 1, file);
    fclose(file);

    data[size] = 0;
    return data;
}

bool is_opcode(const char *token, Operation *op) {
    for (size_t i=0; i<OP_COUNT; i++) {
        if (strcmp(token, OPERATIONS[i]) == 0) {
            *op = i;
            return true;
        }
    }
    return false;
}

bool is_register(char *token, int *register_num) {
    if (!(token[0] && token[1] && !token[2] && token[0] == 'r')) {
        printf("bass: invalid register: `%s`\n", token);
        return false;
    }
    int num = token[1] - 48;
    if (num < 0 || num >= REG_COUNT) {
        printf("bass: invalid register: `%s`\n", token);
        return false;
    }
    *register_num = num;
    return true;
}


bool parse_source(char *source, Tokens *tokens, OpCodes *opcodes) {
    const char *delims = " \n";
    char *token = strtok(source, delims);

    size_t op_start = 0;
    size_t op_end = 0;
    size_t line = 1;
    Operation op;
    int register_num;

    while (token != NULL) {
        token[strcspn(token, "\n")] = 0;
        if (token[0] == '#') {
            dyn_append(tokens, ((Token){token + 1, TOK_VALUE, {0}}));
        } else if (token[0] == '@') {
            dyn_append(tokens, ((Token){token + 1, TOK_ADDRESS, {0}}));
        } else if (token[0] == 'r') {
            if (!is_register(token, &register_num)) {
                return false;
            }
            dyn_append(tokens, ((Token){token, TOK_REGISTER, {.register_num = register_num} }));
        } else if (is_opcode(token, &op)) {
            dyn_append(tokens, ((Token){token, TOK_OPCODE, {.op = op} }));
            if (op_start != op_end) {
                dyn_append(opcodes, ((OpCode){op_start, op_end}));
                op_start = op_end;
                line++;
            }
        } else {
            printf("Error parsing token: `%s` at line: %zu\n", token, line);
            return false;
        }
        token = strtok(NULL, delims);
        op_end++;
    }
    dyn_append(opcodes, ((OpCode){op_start, op_end}));
    return true;
}

void tokens_print(Tokens tokens) {
    for (size_t i=0; i<tokens.size; i++) {
        printf("%s", tokens.data[i].value);
        switch (tokens.data[i].type) {
            case TOK_VALUE:
                printf(" - value\n");
                break;
            case TOK_ADDRESS:
                printf(" - address\n");
                break;
            case TOK_REGISTER:
                printf(" - register\n");
                break;
            case TOK_OPCODE:
                printf(" - opcode\n");
                break;
            default: 
                assert(false && "Unhandled case!");
        }
    }
    putchar('\n');
}

void opcodes_print(Tokens tokens, OpCodes opcodes) {
    for (size_t i=0; i<opcodes.size; i++) {
        Token token = tokens.data[opcodes.data[i].start];
        printf("%s\n", OPERATIONS[token.op]); 
    }
    putchar('\n');
}

typedef struct {
    int registers[REG_COUNT];
} State;

bool eval_rval(State *state, Token token, int *value) {
    switch (token.type) {
        case TOK_VALUE: {
            char *end;
            *value = (int)strtol(token.value, &end, 0);
            if (*end) {
                printf("bass: incorrect integer literal");
                return false;
            }
            return true;
        } break;
        case TOK_ADDRESS:
            assert(false && "Unimplemented");
        break;
        case TOK_REGISTER: {
            *value = state->registers[token.register_num];
            return true;
        } break;
        case TOK_OPCODE:
            printf("bass: opcode: `%s` is not an rvalue!", token.value);
            return false;
        break;
    }
    return true;
}

bool set_lval(State *state, Token lval, int rval) {
    switch (lval.type) {
        case TOK_REGISTER: {
            state->registers[lval.register_num] = rval;
            return true;
        } break;
        case TOK_ADDRESS: {
            assert(false && "Unimplemented");
        } break;
        default:
            printf("bass: lvalue: `%s`, is neither register nor a memory address", lval.value);
            return false;
        break;

    }
}

bool eval_3(State *state, Operation op, Token arg1, Token arg2, Token arg3) {
    int arg2_val, arg3_val;
    if (!eval_rval(state, arg2, &arg2_val)) {
        return false;
    }
    if (!eval_rval(state, arg3, &arg3_val)) {
        return false;
    }

    switch (op) {
        case OP_ADD: {
            if (!set_lval(state, arg1, arg2_val + arg3_val)) {
                return false;
            }
            return true;
        } break;

        case OP_SUB: {
            if (!set_lval(state, arg1, arg2_val - arg3_val)) {
                return false;
            }
            return true;
        } break;

        case OP_MUL: {
            if (!set_lval(state, arg1, arg2_val * arg3_val)) {
                return false;
            }
            return true;
        } break;

        case OP_DIV: {
            if (arg3_val == 0) {
                printf("bass: attempt to divide by 0\n");
                return false;
            }
            if (!set_lval(state, arg1, arg2_val / arg3_val)) {
                return false;
            }
            return true;
        } break;

        default:
            assert(false && "Unimplemented");
        break;
    }
}

bool eval_2(State *state, Operation op, Token arg1, Token arg2) {
    int arg2_val;
    if (!eval_rval(state, arg2, &arg2_val)) {
        return false;
    }

    switch (op) {
        case OP_MOVE: {
            return set_lval(state, arg1, arg2_val);
        } break;
        default:
            assert(false && "Unreachable!");
    }
}


bool execute_opcode(State *state, Tokens tokens, OpCode op) {
    switch (op.end - op.start - 1) {
        case 3: {
            return eval_3(state, tokens.data[op.start].op, 
                          tokens.data[op.start + 1], 
                          tokens.data[op.start + 2], 
                          tokens.data[op.start + 3]);
        } break;
        case 2: {
            return eval_2(state, tokens.data[op.start].op, 
                          tokens.data[op.start + 1], 
                          tokens.data[op.start + 2]);
        } break;
        case 1: {
            Token arg1 = tokens.data[op.start + 1];
            int arg1_val;
            if (!eval_rval(state, arg1, &arg1_val)) {
                return false;
            }
            if (tokens.data[op.start].op == OP_PRINT) {
                printf("%d", arg1_val);
                return true;
            }
            return false;
        } break;
        default:
            assert(false && "Unreachable!");
    }
}


bool execute(State *state, Tokens tokens, OpCodes opcodes) {
    for (size_t i = 0; i < opcodes.size; i++) {
        OpCode op = opcodes.data[i];
        if (!execute_opcode(state, tokens, op)) {
            return false;
        }
    }
    return true;
}


int main(int argc, char **argv) {
    (void)argc;
    char *source = read_to_string(argv[1]);
    Tokens tokens = {0};
    OpCodes opcodes = {0};
    if (!parse_source(source, &tokens, &opcodes)) {
        return 1;
    }
    tokens_print(tokens);
    opcodes_print(tokens, opcodes);

    State state = {0};
    execute(&state, tokens, opcodes);
    return 0;
}
