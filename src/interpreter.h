#ifndef BASS_INTERPRETER_H
#define BASS_INTERPRETER_H

#include "constants.h"
#include "parser.h"

typedef struct {
    int registers[REG_COUNT];
    int stack[STACK_MAX];
    int reg_sp;    // stack pointer register
    size_t reg_pc; // program counter register (stores next op index)
} State;

bool interpret(State *state, OpCodes opcodes);
#endif
