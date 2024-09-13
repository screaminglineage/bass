#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "parser.h"
#include "utils.h"

typedef struct {
    int registers[REG_COUNT];
    int reg_sp;
    int stack[STACK_MAX];
} State;

#if 0

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
        printf("bass: lvalue: `%s`, is neither register nor a memory address",
               lval.value);
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

    case OP_MOD: {
        if (arg3_val == 0) {
            printf("bass: attempt to divide by 0\n");
            return false;
        }
        if (!set_lval(state, arg1, arg2_val % arg3_val)) {
            return false;
        }
        return true;
    } break;

    default: {
        printf("bass: opcode: `%s` does not take 3 arguments\n",
               OPERATIONS[op]);
        return false;
    }
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
    default: {
        printf("bass: opcode: `%s` does not take 2 arguments\n",
               OPERATIONS[op]);
        return false;
    }
    }
}

bool eval_1(State *state, Operation op, Token arg) {
    switch (op) {
    case OP_PRINT: {
        int arg_val;
        if (!eval_rval(state, arg, &arg_val)) {
            return false;
        }
        printf("%d", arg_val);
        return true;
    } break;
    case OP_PUSH: {
        int arg_val;
        if (!eval_rval(state, arg, &arg_val)) {
            return false;
        }
        state->stack[state->reg_sp] = arg_val;
        state->reg_sp = (state->reg_sp + 1) % STACK_MAX;
        return true;
    } break;
    case OP_POP: {
        state->reg_sp = MODULO(state->reg_sp - 1, STACK_MAX);
        int value = state->stack[state->reg_sp];
        if (!set_lval(state, arg, value)) {
            return false;
        }
        return true;
    } break;
    default: {
        printf("bass: opcode: `%s` does not take 2 arguments\n",
               OPERATIONS[op]);
        return false;
    }
    }
}

bool execute_opcode(State *state, Tokens tokens, OpCode op) {
    switch (op.end - op.start - 1) {
    case 3: {
        return eval_3(state, tokens.data[op.start].op,
                      tokens.data[op.start + 1], tokens.data[op.start + 2],
                      tokens.data[op.start + 3]);
    } break;
    case 2: {
        return eval_2(state, tokens.data[op.start].op,
                      tokens.data[op.start + 1], tokens.data[op.start + 2]);
    } break;
    case 1: {
        return eval_1(state, tokens.data[op.start].op,
                      tokens.data[op.start + 1]);
    } break;

    case 0: {
        if (tokens.data[op.start].op == OP_NO) {
            // nop does nothing
            return true;
        }
        printf("bass: opcode: `%s` does not take 0 arguments\n",
               OPERATIONS[tokens.data[op.start].op]);
        return false;
    } break;

    default:
        assert(false && "Unreachable!");
    }
}

#endif

/* int main(int argc, char **argv) { */
/*     (void)argc; */
/**/
/*     StringView source; */
/*     if (!read_to_string(argv[1], &source)) { */
/*         return 1; */
/*     } */
/*     Tokens tokens = {0}; */
/*     OpCodes opcodes = {0}; */
/*     if (!parse_source(source.data, &tokens, &opcodes)) { */
/*         return 1; */
/*     } */
/*     tokens_print(tokens); */
/*     opcodes_print(tokens, opcodes); */
/**/
/*     State state = {0}; */
/*     execute(&state, tokens, opcodes); */
/*     return 0; */
/* } */

int main(int argc, char *argv[]) {
    (void)argc;

    StringView sv;
    read_to_string(argv[1], &sv);
    Parser p;
    parser_init(&p, sv);
    OpCodes opcodes = {0};
    Labels labels = {0};
    parse(&p, &opcodes, &labels) ? printf("Successfully parsed!\n")
                                 : printf("Failed to parse!\n");
    patch_labels(&opcodes, labels);

    display_opcodes(opcodes);
    display_labels(labels);
    return 0;
}
