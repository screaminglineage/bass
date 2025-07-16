#include "utils.h"
#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "compiler.h"

void compile_exit(FILE *f, const char *reg) {
    fprintf(f, "\n\n");
    fprintf(f, "// exit syscall\n");
    fprintf(f, "    mov rax, 1\n");
    fprintf(f, "    mov rbx, %s\n", reg);
    fprintf(f, "    int 0x80\n");
}

void compile_value_read(FILE *f, Operand *operand) {
    switch (operand->type) {
        case TOK_REGISTER: {
            int x86_64_register = operand->as_int + 8;
            fprintf(f, "r%d", x86_64_register);
        } break;
        case TOK_LITERAL_NUM: {
            fprintf(f, "%d", operand->as_int);
        } break;
        case TOK_ADDRESS: { TODO("implement memory read"); } break;
        case TOK_ADDRESS_REG: { TODO("implement indirect register read"); } break;
        default:
            UNREACHABLE_INFO("invalid operand type");
    }
}

// TODO: see if theres any other way to report the error without taking in an opcode
bool compile_value_write(FILE *f, OpCode *opcode, Operand *operand) {
    switch (operand->type) {
        case TOK_REGISTER: {
            int x86_64_register = operand->as_int + 8;
            fprintf(f, "r%d", x86_64_register);
        } break;
        case TOK_LITERAL_NUM: {
            fprintf(stderr,
                "bass:%d:%zu: error: expected register or memory address after "
                "opcode `%s`, but got %s: `%.*s`\n"
                "help: an rvalue was expected but an lvalue was found, check if "
                "you put a `#` instead of a `r` or `@`\n",
                operand->line, operand->col, OPCODES[opcode->op].name,
                TOKEN_STRING[TOK_LITERAL_NUM], SV_FORMAT(operand->str));
            return false;
        } break;
        case TOK_ADDRESS: { TODO("implement memory write"); } break;
        case TOK_ADDRESS_REG: { TODO("implement indirect register write"); } break;
        default:
            UNREACHABLE_INFO("invalid operand type");
    }
    return true;
}


// TODO: remove opcode and only take operand
bool compile_value_set_instruction(FILE *f, const char *instruction, OpCode *opcode, size_t write_index, size_t read_index) {
    fprintf(f, "    %s ", instruction);
    if (!compile_value_write(f, opcode, &opcode->operands[write_index])) return false;
    fprintf(f, ", ");
    compile_value_read(f, &opcode->operands[read_index]);
    fprintf(f, "\n");
    return true;
}

// TODO: remove opcode and only take operand
bool compile_mov_value(FILE *f, OpCode *opcode, uint64_t value) {
    fprintf(f, "    mov ");
    if (!compile_value_write(f, opcode, &opcode->operands[0])) return false;
    fprintf(f, ", %ld", value);
    return true;
}

// TODO: remove opcode and only take operand
bool compile_mov_from_str(FILE *f, OpCode *opcode, const char *source) {
    fprintf(f, "    mov ");
    if (!compile_value_write(f, opcode, &opcode->operands[0])) return false;
    fprintf(f, ", %s\n", source);
    return true;
}

void compile_instruction_to_str(FILE *f, const char *instruction, const char *dest, Operand *source) {
    fprintf(f, "    %s %s, ", instruction, dest);
    compile_value_read(f, source);
    fprintf(f, "\n");
}

// TODO: come up with a better name for this function
bool compile_two_step_instruction(FILE *f, const char *instruction, OpCode *opcode) {
    // TODO: save and restore rcx?
    compile_instruction_to_str(f, "mov", "rcx", &opcode->operands[1]);
    compile_instruction_to_str(f, instruction, "rcx", &opcode->operands[2]);
    if (!compile_mov_from_str(f, opcode, "rcx")) return false;
    return true;
}


bool compile_opcode(FILE *f, OpCode *opcode) {
    switch (opcode->op) {
        case OP_NO: {
            fprintf(f, "    nop\n");
        } break;
        case OP_MOVE: {
            if (!compile_value_set_instruction(f, "mov", opcode, 0, 1)) return false;
        } break;
        case OP_ADD: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow
                if (!compile_mov_value(f, opcode, opcode->operands[1].as_int + opcode->operands[2].as_int)) return false;
            } else {
                if (!compile_two_step_instruction(f, "add", opcode)) return false;
            }
        } break;
        case OP_SUB: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow
                if (!compile_mov_value(f, opcode, opcode->operands[1].as_int - opcode->operands[2].as_int)) return false;
            } else {
                if (!compile_two_step_instruction(f, "sub", opcode)) return false;
            }
        } break;
        case OP_MUL: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow
                if (!compile_mov_value(f, opcode, opcode->operands[1].as_int * opcode->operands[2].as_int)) return false;
            } else {
                if (!compile_two_step_instruction(f, "imul", opcode)) return false;
            }
        } break;
        case OP_DIV: {
            // div x1 x2 x3
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and division by zero
                if (!compile_mov_value(f, opcode, opcode->operands[1].as_int / opcode->operands[2].as_int)) return false;
            } else {
                // TODO: save and restore rax and rbx
                // mov rax, x2
                compile_instruction_to_str(f, "mov", "rax", &opcode->operands[1]);

                // idiv x3
                // idiv doesnt support immediates as an operand
                if (opcode->operands[2].type == TOK_LITERAL_NUM) {
                    compile_instruction_to_str(f, "mov", "rbx", &opcode->operands[2]);
                    fprintf(f, "    idiv rbx\n");
                } else {
                    fprintf(f, "    idiv ");
                    compile_value_read(f, &opcode->operands[2]);
                    fprintf(f, "\n");
                }

                // mov x1, rax
                if (!compile_mov_from_str(f, opcode, "rax")) return false;
            }
        } break;
        case OP_MOD: {
            // div x1 x2 x3
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and division by zero
                if (!compile_mov_value(f, opcode, opcode->operands[1].as_int % opcode->operands[2].as_int)) return false;
            } else {
                // TODO: save and restore rax, rbx, rdx
                // mov rax, x2
                compile_instruction_to_str(f, "mov", "rax", &opcode->operands[1]);

                // idiv x3
                // idiv doesnt support immediates as an operand
                if (opcode->operands[2].type == TOK_LITERAL_NUM) {
                    compile_instruction_to_str(f, "mov", "rbx", &opcode->operands[2]);
                    fprintf(f, "    idiv rbx\n");
                } else {
                    fprintf(f, "    idiv ");
                    compile_value_read(f, &opcode->operands[2]);
                    fprintf(f, "\n");
                }

                // mov x1, rdx (remainder)
                if (!compile_mov_from_str(f, opcode, "rdx")) return false;
            }
        } break;
        case OP_LOAD     : { TODO("OP_LOAD: not yet implemented");     }break;
        case OP_STORE    : { TODO("OP_STORE: not yet implemented");    }break;
        case OP_PRINT    : { TODO("OP_PRINT: not yet implemented");    }break;
        case OP_PRINTLN  : { TODO("OP_PRINTLN: not yet implemented");  }break;
        case OP_PRINTB   : { TODO("OP_PRINTB: not yet implemented");   }break;
        case OP_PRINTBLN : { TODO("OP_PRINTBLN: not yet implemented"); }break;
        case OP_READ     : { TODO("OP_READ: not yet implemented");     }break;
        case OP_PUSH     : { TODO("OP_PUSH: not yet implemented");     }break;
        case OP_POP      : { TODO("OP_POP: not yet implemented");      }break;
        case OP_CMP      : { TODO("OP_CMP: not yet implemented");      }break;
        case OP_JUMP     : { TODO("OP_JUMP: not yet implemented");     }break;
        case OP_JUMPZ    : { TODO("OP_JUMPZ: not yet implemented");    }break;
        case OP_JUMPG    : { TODO("OP_JUMPG: not yet implemented");    }break;
        case OP_JUMPL    : { TODO("OP_JUMPL: not yet implemented");    }break;
        case OP_CALL     : { TODO("OP_CALL: not yet implemented");     }break;
        case OP_RETURN   : { TODO("OP_RETURN: not yet implemented");   }break;
        case OP_COUNT:
            UNREACHABLE_INFO("OP_COUNT is not a valid opcode");
    }
    return true;
}

bool compile(const char *output_path, Labels labels, OpCodes opcodes, size_t entry) {
    FILE *f = fopen(output_path, "w");
    fprintf(f, ".intel_syntax noprefix\n");
    fprintf(f, ".globl _start\n");

    size_t i = 0, j = 0;
    while (i < labels.size && j < opcodes.size) {
        while (i < labels.size && j == labels.data[i].index) {
            if (i == entry) {
                fprintf(f, "_start:\n");
            }
            fprintf(f, "%.*s: ; (opcode: %zu)\n", SV_FORMAT(labels.data[i].name), labels.data[i].index);
            i++;
        }
        compile_opcode(f, &opcodes.data[j]);
        j++;
    }
    for (; i < labels.size; i++) {
        fprintf(f, "%.*s: ; (opcode: %zu)\n", SV_FORMAT(labels.data[i].name), labels.data[i].index);
    }

    if (i == entry) {
        fprintf(f, "_start:\n");
    }
    for (; j < opcodes.size; j++) {
        compile_opcode(f, &opcodes.data[j]);
    }

    compile_exit(f, "r8");
    fclose(f);
    return true;
}


