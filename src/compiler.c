#include "utils.h"
#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "compiler.h"

void compile_exit(FILE *f) {
    fprintf(f, "\n");
    fprintf(f, "    mov eax, 1\n");
    fprintf(f, "    mov ebx, 25\n");
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
                opcode->line, opcode->col, OPCODES[opcode->op].name,
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


bool compile_value_set_instruction(FILE *f, const char *instruction, OpCode *opcode, size_t write_index, size_t read_index) {
    fprintf(f, "    %s ", instruction);
    if (!compile_value_write(f, opcode, &opcode->operands[write_index])) return false;
    fprintf(f, ", ");
    compile_value_read(f, &opcode->operands[read_index]);
    fprintf(f, "\n");
    return true;
}

// TODO: come up with a better name for this function
bool compile_two_step_instruction(FILE *f, const char *instruction, OpCode *opcode) {
    int write_index = 1;
    int read_index = 2;

    // Cant write to these operands
    if (opcode->operands[1].type == TOK_LITERAL_NUM
        || opcode->operands[1].type == TOK_LITERAL_CHAR
        || opcode->operands[1].type == TOK_LITERAL_STR
        || opcode->operands[1].type == TOK_IDENTIFIER) {
        write_index = 2;
        read_index = 1;
    }
    if (!compile_value_set_instruction(f, instruction, opcode, write_index, read_index)) return false;
    if (!compile_value_set_instruction(f, "mov", opcode, 0, write_index)) return false;
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
            const char *instruction = "add";
            // TODO: check for other invalid arguments (string, char, identifiers)
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                fprintf(f, "    %s ", instruction);
                if (!compile_value_write(f, opcode, &opcode->operands[0])) return false;
                // TODO: check for overflow
                fprintf(f, ", %d", opcode->operands[1].as_int + opcode->operands[2].as_int);
            } else {
                if (!compile_two_step_instruction(f, instruction, opcode)) return false;
            }
        } break;
        case OP_SUB: {
            const char *instruction = "sub";
            // TODO: check for other invalid arguments (string, char, identifiers)
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                fprintf(f, "    %s ", instruction);
                if (!compile_value_write(f, opcode, &opcode->operands[0])) return false;
                // TODO: check for overflow
                fprintf(f, ", %d", opcode->operands[1].as_int - opcode->operands[2].as_int);
            } else {
                if (!compile_two_step_instruction(f, instruction, opcode)) return false;
            }
        } break;
        case OP_MUL: {
            const char *instruction = "imul";
            // TODO: check for other invalid arguments (string, char, identifiers)
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                fprintf(f, "    %s ", instruction);
                if (!compile_value_write(f, opcode, &opcode->operands[0])) return false;
                // TODO: check for overflow
                fprintf(f, ", %d", opcode->operands[1].as_int * opcode->operands[2].as_int);
            } else {
                if (!compile_two_step_instruction(f, instruction, opcode)) return false;
            }
        } break;
        case OP_DIV      : { TODO("OP_DIV: not yet implemented");      }break;
        case OP_MOD      : { TODO("OP_MOD: not yet implemented");      }break;
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

bool compile(Labels labels, OpCodes opcodes, size_t entry) {
    FILE *f = fopen("bass-compiled.s", "w");
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

    compile_exit(f);
    fclose(f);
    return true;
}


