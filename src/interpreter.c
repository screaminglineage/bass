#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "interpreter.h"
#include "parser.h"
#include "utils.h"

int eval_rval(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_VALUE:
        return operand.value;
    case TOK_ADDRESS:
        assert(false && "Unimplemented");
    case TOK_REGISTER:
        return state->registers[operand.value];
    default:
        assert(false && "Unreachable");
    }
}

bool set_lval(State *state, Operand lval, int rval) {
    switch (lval.type) {
    case TOK_REGISTER: {
        state->registers[lval.value] = rval;
        return true;
    }
    case TOK_ADDRESS: {
        assert(false && "Unimplemented");
    } break;
    default: {
        char *str = string_view_to_cstring(lval.string);
        fprintf(stderr,
                "bass: lvalue: `%s`, is neither register nor memory"
                "address ",
                str);
        free(str);
        return false;
    }
    }
}

// TODO: wtf is this shit
// needs this function to make compiler happy
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
    int first = eval_rval(state, opcode.operands[1]);
    int second = eval_rval(state, opcode.operands[2]);

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
        int first = eval_rval(state, opcode.operands[1]);
        if (!set_lval(state, opcode.operands[0], first)) {
            return false;
        }
    } break;
    case OP_JUMP: {
        state->reg_pc = opcode.operands[0].value;
    } break;
    case OP_PUSH: {
        int first = eval_rval(state, opcode.operands[0]);
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
        printf("%d", eval_rval(state, opcode.operands[0]));
    } break;
    case OP_NO:
    default:
        break;
    }
    return true;
}

bool interpret(State *state, OpCodes opcodes) {
    state->reg_pc = 0;
    while (state->reg_pc < opcodes.size) {
        OpCode op = opcodes.data[state->reg_pc++];
        if (!execute_opcode(state, op)) {
            return false;
        }
    }
    return true;
}
