#ifndef BASS_INTERPRETER_H
#define BASS_INTERPRETER_H

#include "constants.h"
#include "parser.h"

typedef struct {
    int registers[REG_COUNT];
    int stack[STACK_MAX];
    int reg_sp;    // stack pointer register
    size_t reg_pc; // program counter register (stores next op index)
    int flag_cmp;  // -1, 0, 1 depending on last cmp operation
    unsigned char *memory;
} State;

static inline bool state_init(State *state) {
    memset(state, 0, sizeof(*state));
    state->memory = calloc(MEMORY_SIZE, sizeof(state->memory[0]));
    if (!state->memory) {
        return false;
    }
    return true;
}

bool interpret(State *state, OpCodes opcodes);
#endif
