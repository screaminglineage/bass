#include "utils.h"
#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "compiler.h"

void compile_exit(FILE *f) {
    fprintf(f, "\n\n; exit syscall\n");
    fprintf(f, "    mov rax, 60\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    syscall\n");
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
            fprintf(stderr, "invalid operand for reads: %s\n", TOKEN_STRING[operand->type]);
            UNREACHABLE();
    }
}

void compile_value_write(FILE *f, Operand *operand) {
    switch (operand->type) {
        case TOK_REGISTER: {
            int x86_64_register = operand->as_int + 8;
            fprintf(f, "r%d", x86_64_register);
        } break;
        case TOK_ADDRESS: { TODO("implement memory write"); } break;
        case TOK_ADDRESS_REG: { TODO("implement indirect register write"); } break;
        case TOK_LITERAL_NUM:
        default:
            UNREACHABLE_INFO("invalid operand type for writes");
    }
}


void compile_value_set_instruction(FILE *f, const char *instruction, Operand *dest, Operand *source) {
    fprintf(f, "    %s ", instruction);
    compile_value_write(f, dest);
    fprintf(f, ", ");
    compile_value_read(f, source);
    fprintf(f, "\n");
}

void compile_mov_to_operand(FILE *f, Operand *operand, uint64_t value) {
    fprintf(f, "    mov ");
    compile_value_write(f, operand);
    fprintf(f, ", %ld", value);
}

void compile_mov_from_str(FILE *f, Operand *operand, const char *source) {
    fprintf(f, "    mov ");
    compile_value_write(f, operand);
    fprintf(f, ", %s\n", source);
}

void compile_instruction_to_str(FILE *f, const char *instruction, const char *dest, Operand *source) {
    fprintf(f, "    %s %s, ", instruction, dest);
    compile_value_read(f, source);
    fprintf(f, "\n");
}

// TODO: come up with a better name for this function
void compile_two_step_instruction(FILE *f, const char *instruction, OpCode *opcode) {
    // TODO: save and restore rcx?
    compile_instruction_to_str(f, "mov", "rcx", &opcode->operands[1]);
    compile_instruction_to_str(f, instruction, "rcx", &opcode->operands[2]);
    compile_mov_from_str(f, &opcode->operands[0], "rcx");
}

void compile_print_int(FILE *f, Operand *operand, bool add_newline) {
    // TODO: make print a function and fix this temporary label hack
    fprintf(f, "\n; print opcode\n");
    static int print_count = 0;
    fprintf(f, "print_num_%d:\n", print_count++);

    // TODO: check if this can be done better
    fprintf(f, "    mov rbp, rsp\n");
    compile_instruction_to_str(f, "mov", "rsi", operand);
    fprintf(f, "    mov rbx, 10\n");

    if (add_newline) {
        fprintf(f, "    mov rcx, 1\n");
        fprintf(f, "    dec rsp\n");
        fprintf(f, "    mov byte [rsp], 0xA\n");
    }

    // check if number is negative
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    test rsi, rsi\n");
    fprintf(f, "    jnl .non_negative\n");
    fprintf(f, "    mov rdi, 1\n");
    fprintf(f, "    neg rsi\n");

    fprintf(f, ".non_negative:\n");
    fprintf(f, "    mov rax, rsi\n");
    fprintf(f, ".num_to_string:\n");
    fprintf(f, "    xor rdx, rdx\n");
    fprintf(f, "    div rbx\n");
    fprintf(f, "    add rdx, '0'\n");
    fprintf(f, "    inc rcx\n");
    fprintf(f, "    dec rsp\n");
    fprintf(f, "    mov [rsp], dl\n");
    fprintf(f, "    test rax, rax\n");
    fprintf(f, "    jnz .num_to_string\n");

    // add '-' if number was negative
    fprintf(f, "    cmp rdi, 1\n");
    fprintf(f, "    jnz .write\n");
    fprintf(f, "    inc rcx\n");
    fprintf(f, "    dec rsp\n");
    fprintf(f, "    mov byte [rsp], '-'\n");

    fprintf(f, ".write:\n");
    fprintf(f, "    mov rax, 1\n");
    fprintf(f, "    mov rdi, 1\n");
    fprintf(f, "    mov rsi, rsp\n");
    fprintf(f, "    mov rdx, rcx\n");
    fprintf(f, "    syscall\n");

    fprintf(f, "    mov rsp, rbp\n\n");
}

bool compile_opcode(FILE *f, OpCode *opcode) {
    switch (opcode->op) {
        case OP_NO: {
            fprintf(f, "    nop\n");
        } break;
        case OP_MOVE: {
            compile_value_set_instruction(f, "mov", &opcode->operands[0], &opcode->operands[1]);
        } break;
        case OP_ADD: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and return error
                compile_mov_to_operand(f, &opcode->operands[0], opcode->operands[1].as_int + opcode->operands[2].as_int);
            } else {
                compile_two_step_instruction(f, "add", opcode);
            }
        } break;
        case OP_SUB: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and return error
                compile_mov_to_operand(f, &opcode->operands[0], opcode->operands[1].as_int - opcode->operands[2].as_int);
            } else {
                compile_two_step_instruction(f, "sub", opcode);
            }
        } break;
        case OP_MUL: {
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and return error
                compile_mov_to_operand(f, &opcode->operands[0], opcode->operands[1].as_int * opcode->operands[2].as_int);
            } else {
                compile_two_step_instruction(f, "imul", opcode);
            }
        } break;
        case OP_DIV: {
            // div x1 x2 x3
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and division by zero
                compile_mov_to_operand(f, &opcode->operands[0], opcode->operands[1].as_int / opcode->operands[2].as_int);
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
                compile_mov_from_str(f, &opcode->operands[0], "rax");
            }
        } break;
        case OP_MOD: {
            // div x1 x2 x3
            if ((opcode->operands[1].type == TOK_LITERAL_NUM) && (opcode->operands[2].type == TOK_LITERAL_NUM)) {
                // TODO: check for overflow and division by zero
                compile_mov_to_operand(f, &opcode->operands[0], opcode->operands[1].as_int % opcode->operands[2].as_int);
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
                compile_mov_from_str(f, &opcode->operands[0], "rdx");
            }
        } break;
        case OP_LOAD     : { TODO("OP_LOAD: not yet implemented");     }break;
        case OP_STORE    : { TODO("OP_STORE: not yet implemented");    }break;

        case OP_PRINT: {
            if (opcode->operands[0].type == TOK_LITERAL_STR) {
                TODO("printing strings: not yet implemented");
            } else if (opcode->operands[0].type == TOK_LITERAL_CHAR) {
                TODO("printing characters: not yet implemented");
            } else {
                compile_print_int(f, &opcode->operands[0], false);
            }
        } break;
        case OP_PRINTLN: {
            if (opcode->operands[0].type == TOK_LITERAL_STR) {
                TODO("printing strings: not yet implemented");
            } else if (opcode->operands[0].type == TOK_LITERAL_CHAR) {
                TODO("printing characters: not yet implemented");
            } else {
                compile_print_int(f, &opcode->operands[0], true);
            }
        } break;
        case OP_PRINTB   : { TODO("OP_PRINTB: not yet implemented");   }break;
        case OP_PRINTBLN : { TODO("OP_PRINTBLN: not yet implemented"); }break;
        case OP_READ     : { TODO("OP_READ: not yet implemented");     }break;
        case OP_PUSH     : { TODO("OP_PUSH: not yet implemented");     }break;
        case OP_POP      : { TODO("OP_POP: not yet implemented");      }break;
        case OP_CMP: {
            if ((opcode->operands[0].type == TOK_LITERAL_NUM) && (opcode->operands[1].type == TOK_LITERAL_NUM)) {
                compile_instruction_to_str(f, "mov", "rax", &opcode->operands[0]);
                compile_instruction_to_str(f, "mov", "rbx", &opcode->operands[1]);
                fprintf(f, "    cmp rax, rbx\n");
            } else {
                fprintf(f, "    cmp ");
                compile_value_read(f, &opcode->operands[0]);
                fprintf(f, ", ");
                compile_value_read(f, &opcode->operands[1]);
                fprintf(f, "\n");
            }
        } break;
        case OP_JUMP: {
            fprintf(f, "    jmp %.*s\n", SV_FORMAT(opcode->operands[0].str));
        } break;
        case OP_JUMPZ: {
            fprintf(f, "    jz %.*s\n", SV_FORMAT(opcode->operands[0].str));
        }break;
        case OP_JUMPG: {
            fprintf(f, "    jg %.*s\n", SV_FORMAT(opcode->operands[0].str));
        }break;
        case OP_JUMPL: {
            fprintf(f, "    jl %.*s\n", SV_FORMAT(opcode->operands[0].str));
        } break;
        case OP_CALL     : { TODO("OP_CALL: not yet implemented");     }break;
        case OP_RETURN   : { TODO("OP_RETURN: not yet implemented");   }break;
        case OP_COUNT:
            UNREACHABLE_INFO("OP_COUNT is not a valid opcode");
    }
    return true;
}

bool compile(const char *output_path, Labels labels, OpCodes opcodes, size_t entry) {
    FILE *f = fopen(output_path, "w");
    fprintf(f, "section .text\n");
    fprintf(f, "    global _start\n");
    fprintf(f, "\n");

    // TODO: make sure that r8-r15 are all set to 0

    // putting entrypoint before code generation if its the default one
    if (entry == 0) {
        fprintf(f, "_start:\n");
    }

    size_t i = 0, j = 0;
    while (i < labels.size && j < opcodes.size) {
        while (i < labels.size && j == labels.data[i].index) {
            fprintf(f, "%.*s: ; (opcode: %s)\n",
                    SV_FORMAT(labels.data[i].name),
                    OPCODES[opcodes.data[labels.data[i].index].op].name);
            i++;
        }
        compile_opcode(f, &opcodes.data[j]);
        j++;
    }

    for (; i < labels.size; i++) {
        fprintf(f, "%.*s: ; (opcode: %s)\n",
                SV_FORMAT(labels.data[i].name),
                OPCODES[opcodes.data[labels.data[i].index].op].name);
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


