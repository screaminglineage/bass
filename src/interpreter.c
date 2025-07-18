#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "interpreter.h"
#include "constants.h"
#include "parser.h"
#include "utils.h"

// evaluates values that are treated as integers
static inline int eval_int(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_NUM:
        return operand.as_int;
    case TOK_REGISTER:
        return state->registers[operand.as_int];
    case TOK_ADDRESS:
        return *(int *)(&state->memory[operand.as_int]);
    case TOK_ADDRESS_REG:
        return *(int *)(&state->memory[state->registers[operand.as_int]]);
    default:
        UNREACHABLE_INFO("Passed in value was not an integer!");
    }
}

// evaluates values that are treated as chars
static inline int eval_char(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_NUM:
        return operand.as_int;
    case TOK_REGISTER:
        return state->registers[operand.as_int];
    case TOK_ADDRESS:
        return *(char *)(&state->memory[operand.as_int]);
    case TOK_ADDRESS_REG:
        return *(char *)(&state->memory[state->registers[operand.as_int]]);
    default:
        UNREACHABLE_INFO("Passed in value was not a character!");
    }
}

static inline void set_lval(State *state, OpCode *op, int rval) {
    // first operand is always the lvalue to be set
    Operand lval = op->operands[0];

    switch (lval.type) {
    case TOK_REGISTER:
        state->registers[lval.as_int] = rval;
        break;
    case TOK_ADDRESS:
        *(int *)(&state->memory[lval.as_int]) = rval;
        break;
    case TOK_ADDRESS_REG:
        *(int *)(&state->memory[state->registers[lval.as_int]]) = rval;
        break;
    default:
        UNREACHABLE_INFO("passed in value was not an lvalue");
    }
}

static inline void execute_print(State *state, Operand operand) {
    switch (operand.type) {
    case TOK_LITERAL_CHAR:
        printf("%c", operand.as_int);
        break;
    case TOK_LITERAL_STR:
        printf("%.*s", SV_FORMAT(operand.str));
        break;
    default:
        printf("%d", eval_int(state, operand));
    }
}


static inline bool execute_print_bytes(State *state, OpCode *opcode) {
    Operand operand = opcode->operands[0];
    char *start = 0;
    switch (operand.type) {
    case TOK_LITERAL_NUM: start = (char *)&operand.as_int; break;
    case TOK_REGISTER: start = (char *)&state->registers[operand.as_int]; break;
    case TOK_ADDRESS: start = (char *)(&state->memory[operand.as_int]); break;
    case TOK_ADDRESS_REG: start = (char *)(&state->memory[state->registers[operand.as_int]]); break;
    default:
        UNREACHABLE_INFO("Passed in value was not a character");
    }

    int count = eval_int(state, opcode->operands[1]);
    if (count >= MEMORY_SIZE) {
        fprintf(stderr, "bass:%d:%zu: error: string access will go out of bounds at opcode `%s`, "
                "memory size is %d bytes, but string length is %d\n",
                opcode->line, opcode->col, OPCODES[opcode->op].name, MEMORY_SIZE, count);
        return false;
    }
    for (int i = 0; i < count; i++) {
        putchar(start[i]);
    }
    return true;
}

static inline int eval_jump(State *state, OpCode *opcode) {
    return (opcode->operands[0].type == TOK_IDENTIFIER)
               ? opcode->operands[0].as_int
               : eval_int(state, opcode->operands[0]);
}

// TODO: set flags after operations other than cmp
bool execute_opcode(State *state, OpCode *opcode) {
    switch (opcode->op) {
    case OP_NO: break;
    case OP_ADD: {
        set_lval(state, opcode,
                eval_int(state, opcode->operands[1])
                + eval_int(state, opcode->operands[2]));
    } break;
    case OP_SUB: {
        set_lval(state, opcode, 
                eval_int(state, opcode->operands[1])
                - eval_int(state, opcode->operands[2]));
    } break;
    case OP_MUL: {
        set_lval(state, opcode, 
                eval_int(state, opcode->operands[1])
                * eval_int(state, opcode->operands[2]));
    } break;
    case OP_DIV: {
        int n = eval_int(state, opcode->operands[2]);
        if (n == 0) {
            fprintf(stderr, "bass:%d:%zu: error: division by 0 at opcode `%s`\n",
                    opcode->line, opcode->col, OPCODES[opcode->op].name);
            return false;
        }
        set_lval(state, opcode, eval_int(state, opcode->operands[1]) / n);
    } break;
    case OP_MOD: {
        int n = eval_int(state, opcode->operands[2]);
        if (n == 0) {
            fprintf(stderr, "bass:%d:%zu: error: division by 0 at opcode `%s`\n",
                    opcode->line, opcode->col, OPCODES[opcode->op].name);
            return false;
        }
        set_lval(state, opcode, eval_int(state, opcode->operands[1]) % n);
    } break;
    case OP_MOVE: {
        int value = eval_int(state, opcode->operands[1]);
        set_lval(state, opcode, value);
    } break;
    case OP_LOAD: {
        int index = eval_int(state, opcode->operands[1]);
        int value = *(int *)(&state->memory[index]);
        set_lval(state, opcode, value);
    } break;
    case OP_STORE: {
        int index = eval_int(state, opcode->operands[0]);
        Operand operand = opcode->operands[1];
        switch (operand.type) {
            case TOK_LITERAL_STR: {
                memcpy(&state->memory[index], operand.str.data, operand.str.length);
            } break;
            case TOK_LITERAL_CHAR: {
                *(char *)(&state->memory[index]) = operand.as_int;
            } break;
            default: {
                *(int *)(&state->memory[index]) = eval_int(state, operand);
            }
        }
    } break;
    case OP_CMP: {
        int first = eval_int(state, opcode->operands[0]);
        int second = eval_int(state, opcode->operands[1]);
        state->flag_cmp = (first < second) ? -1 : (first > second) ? +1 : 0;
    } break;
    case OP_JUMP: {
        state->reg_pc = eval_jump(state, opcode);
    } break;
    case OP_JUMPZ: {
        if (state->flag_cmp == 0) {
            state->reg_pc = eval_jump(state, opcode);
        }
    } break;
    case OP_JUMPG: {
        if (state->flag_cmp == 1) {
            state->reg_pc = eval_jump(state, opcode);
        }
    } break;
    case OP_JUMPL: {
        if (state->flag_cmp == -1) {
            state->reg_pc = eval_jump(state, opcode);
        }
    } break;
    case OP_CALL: {
        state->stack[state->reg_sp] = state->reg_pc;
        state->reg_sp = (state->reg_sp + 1) % STACK_MAX;
        state->reg_pc = eval_jump(state, opcode);
    } break;
    case OP_RETURN: {
        state->reg_sp = MODULO(state->reg_sp - 1, STACK_MAX);
        state->reg_pc = state->stack[state->reg_sp];
    } break;
    case OP_PUSH: {
        int first = eval_int(state, opcode->operands[0]);
        state->stack[state->reg_sp] = first;
        state->reg_sp = (state->reg_sp + 1) % STACK_MAX;
    } break;
    case OP_POP: {
        state->reg_sp = MODULO(state->reg_sp - 1, STACK_MAX);
        int value = state->stack[state->reg_sp];
        set_lval(state, opcode, value);
    } break;
    case OP_PRINT: {
        execute_print(state, opcode->operands[0]);
    } break;
    case OP_PRINTLN: {
        execute_print(state, opcode->operands[0]);
        putchar('\n');
    } break;
    case OP_PRINTB: {
        if (!execute_print_bytes(state, opcode)) return false;
    } break;
    case OP_PRINTBLN: {
        if (!execute_print_bytes(state, opcode)) return false;
        putchar('\n');
    } break;
    case OP_READ: {
        int index = eval_int(state, opcode->operands[0]);
        int count = eval_int(state, opcode->operands[1]);
        if (fgets((void*)&state->memory[index], count, stdin) == NULL) {
            fprintf(stderr, "bass:%d:%zu: error: failed to read from stdin at opcode `%s`\n",
                    opcode->line, opcode->col, OPCODES[opcode->op].name);
            return false;
        }
        state->memory[strcspn((void*)&state->memory[index], "\n")] = 0;
    } break;
    default:
        UNREACHABLE();
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
