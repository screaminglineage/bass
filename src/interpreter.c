#include "interpreter.h"
#include "constants.h"
#include "parser.h"
#include "utils.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// evaluates values that are treated as integers
int eval_int(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_NUM:
        return operand.value;
    case TOK_ADDRESS:
        return *(int *)(&state->memory[operand.value]);
    case TOK_REGISTER:
        return state->registers[operand.value];
    default:
        assert(false && "Passed in value was not an integer!");
    }
}

bool set_lval(State *state, Operand lval, int rval) {
    switch (lval.type) {
    case TOK_REGISTER:
        state->registers[lval.value] = rval;
        return true;
    case TOK_ADDRESS:
        *(int *)(&state->memory[lval.value]) = rval;
        return true;
    default: {
        fprintf(stderr,
                "bass: expected register or memory address, got %s: `%.*s`\n",
                TOKEN_STRING[lval.type], (int)lval.string.length,
                lval.string.data);
        return false;
    }
    }
}

// TODO: wtf is this shit
// need this function to make compiler happy
static inline int unreachable() {
    assert(false && "Unreachable");
    return 0;
}

#define CALCULATE(op, a, b)                                                    \
    ((op) == OP_ADD)   ? a + b                                                 \
    : ((op) == OP_SUB) ? a - b                                                 \
    : ((op) == OP_MUL) ? a *b                                                  \
    : ((op) == OP_DIV) ? a / b                                                 \
    : ((op) == OP_MUL) ? a % b                                                 \
                       : unreachable()

bool calculate_and_set(State *state, OpCode opcode, OpType op) {
    int first = eval_int(state, opcode.operands[1]);
    int second = eval_int(state, opcode.operands[2]);

    if ((op == OP_DIV || op == OP_MOD) && second == 0) {
        fprintf(stderr, "bass: division by 0");
        return false;
    }
    if (!set_lval(state, opcode.operands[0], CALCULATE(op, first, second))) {
        return false;
    }
    return true;
}

bool execute_opcode(State *state, OpCode opcode) {
    switch (opcode.op) {
    case OP_ADD:
    case OP_SUB:
    case OP_DIV:
    case OP_MUL:
    case OP_MOD: {
        if (!(calculate_and_set(state, opcode, opcode.op))) {
            return false;
        }
    } break;
    case OP_MOVE: {
        int first = eval_int(state, opcode.operands[1]);
        if (!set_lval(state, opcode.operands[0], first)) {
            return false;
        }
    } break;
    case OP_CMP: {
        int first = eval_int(state, opcode.operands[0]);
        int second = eval_int(state, opcode.operands[1]);
        state->flag_cmp = (first < second) ? -1 : (first > second) ? +1 : 0;
    } break;
    case OP_JUMP: {
        state->reg_pc = opcode.operands[0].value;
    } break;
    case OP_JUMPZ: {
        if (state->flag_cmp == 0) {
            state->reg_pc = opcode.operands[0].value;
        }
    } break;
    case OP_JUMPG: {
        if (state->flag_cmp == 1) {
            state->reg_pc = opcode.operands[0].value;
        }
    } break;
    case OP_JUMPL: {
        if (state->flag_cmp == -1) {
            state->reg_pc = opcode.operands[0].value;
        }
    } break;
    case OP_PUSH: {
        int first = eval_int(state, opcode.operands[0]);
        state->stack[state->reg_sp] = first;
        state->reg_sp = (state->reg_sp + 1) % STACK_MAX;
    } break;
    case OP_POP: {
        state->reg_sp = MODULO(state->reg_sp - 1, STACK_MAX);
        int value = state->stack[state->reg_sp];
        if (!set_lval(state, opcode.operands[0], value)) {
            return false;
        }
    } break;
    case OP_PRINT: {
        Operand operand = opcode.operands[0];
        switch (operand.type) {
        case TOK_LITERAL_CHAR:
            printf("%c", operand.value);
            break;
        case TOK_LITERAL_STR:
            printf("%.*s", SV_FORMAT(operand.string));
            break;
        default:
            printf("%d", eval_int(state, opcode.operands[0]));
        }

    } break;
    case OP_NO:
    default:
        break;
    }
    return true;
}

bool state_init(State *state) {
    memset(state, 0, sizeof(*state));
    state->memory = calloc(MEMORY_SIZE, sizeof(state->memory[0]));
    if (!state->memory) {
        return false;
    }
    return true;
}

bool interpret(State *state, OpCodes opcodes) {
    while (state->reg_pc < opcodes.size) {
        OpCode op = opcodes.data[state->reg_pc++];
        if (!execute_opcode(state, op)) {
            return false;
        }
    }
    return true;
}
