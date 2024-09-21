#include "interpreter.h"
#include "constants.h"
#include "parser.h"
#include "utils.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// evaluates values that are treated as integers
static inline int eval_int(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_NUM:
        return operand.value;
    case TOK_REGISTER:
        return state->registers[operand.value];
    case TOK_ADDRESS:
        return *(int *)(&state->memory[operand.value]);
    case TOK_ADDRESS_REG:
        return *(int *)(&state->memory[state->registers[operand.value]]);
    default:
        assert(false && "Passed in value was not an integer!");
    }
}

static inline bool set_lval(State *state, OpCode *op, int rval) {
    // first operand is always the lvalue to be set
    Operand lval = op->operands[0];

    switch (lval.type) {
    case TOK_REGISTER:
        state->registers[lval.value] = rval;
        return true;
    case TOK_ADDRESS:
        *(int *)(&state->memory[lval.value]) = rval;
        return true;
    case TOK_ADDRESS_REG:
        *(int *)(&state->memory[state->registers[lval.value]]) = rval;
        return true;
    default: {
        fprintf(
            stderr,
            "bass: expected register or memory address after opcode `%s`, but "
            "got %s: `%.*s` at: %d:%zu\n"
            "help: an rvalue was expected but an lvalue was found, check if "
            "you put a `#` instead of a `r` or `@`\n",
            OPCODES[op->op].name, TOKEN_STRING[lval.type],
            SV_FORMAT(lval.string), op->line, op->col);
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

bool calculate_and_set(State *state, OpCode *opcode) {
    int first = eval_int(state, opcode->operands[1]);
    int second = eval_int(state, opcode->operands[2]);
    OpType op = opcode->op;

    if ((op == OP_DIV || op == OP_MOD) && second == 0) {
        fprintf(stderr, "bass: division by 0 at opcode `%s` at: %d:%zu\n",
                OPCODES[op].name, opcode->line, opcode->col);
        return false;
    }
    if (!set_lval(state, opcode, CALCULATE(op, first, second))) {
        return false;
    }
    return true;
}

static inline void execute_print(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_CHAR:
        printf("%c", operand.value);
        break;
    case TOK_LITERAL_STR:
        printf("%.*s", SV_FORMAT(operand.string));
        break;
    default:
        printf("%d", eval_int(state, operand));
    }
}

bool execute_opcode(State *state, OpCode *opcode) {
    switch (opcode->op) {
    case OP_ADD:
    case OP_SUB:
    case OP_DIV:
    case OP_MUL:
    case OP_MOD: {
        if (!(calculate_and_set(state, opcode))) {
            return false;
        }
    } break;
    case OP_MOVE: {
        int first = eval_int(state, opcode->operands[1]);
        if (!set_lval(state, opcode, first)) {
            return false;
        }
    } break;
    case OP_LOAD: {
        int index = eval_int(state, opcode->operands[1]);
        int first = *(int *)(&state->memory[index]);
        if (!set_lval(state, opcode, first)) {
            return false;
        }
    } break;
    case OP_STORE: {
        int index = eval_int(state, opcode->operands[0]);
        *(int *)(&state->memory[index]) = opcode->operands[1].value;
    } break;
    case OP_CMP: {
        int first = eval_int(state, opcode->operands[0]);
        int second = eval_int(state, opcode->operands[1]);
        state->flag_cmp = (first < second) ? -1 : (first > second) ? +1 : 0;
    } break;
    case OP_JUMP: {
        state->reg_pc = opcode->operands[0].value;
    } break;
    case OP_JUMPZ: {
        if (state->flag_cmp == 0) {
            state->reg_pc = opcode->operands[0].value;
        }
    } break;
    case OP_JUMPG: {
        if (state->flag_cmp == 1) {
            state->reg_pc = opcode->operands[0].value;
        }
    } break;
    case OP_JUMPL: {
        if (state->flag_cmp == -1) {
            state->reg_pc = opcode->operands[0].value;
        }
    } break;
    case OP_PUSH: {
        int first = eval_int(state, opcode->operands[0]);
        state->stack[state->reg_sp] = first;
        state->reg_sp = (state->reg_sp + 1) % STACK_MAX;
    } break;
    case OP_POP: {
        state->reg_sp = MODULO(state->reg_sp - 1, STACK_MAX);
        int value = state->stack[state->reg_sp];
        if (!set_lval(state, opcode, value)) {
            return false;
        }
    } break;
    case OP_PRINT: {
        execute_print(state, opcode->operands[0]);
    } break;
    case OP_PRINTLN: {
        execute_print(state, opcode->operands[0]);
        putchar('\n');
    } break;
    case OP_NO:
        break;
    default:
        assert(false && "Unreachable");
    }
    return true;
}

bool interpret(State *state, OpCodes opcodes) {
    while (state->reg_pc < opcodes.size) {
        OpCode op = opcodes.data[state->reg_pc++];
        if (!execute_opcode(state, &op)) {
            return false;
        }
    }
    return true;
}
