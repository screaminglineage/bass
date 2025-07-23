#include "constants.h"
#include "utils.h"
#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "compiler.h"

// TODO: support windows compilation
#ifdef _WIN32
#error "Compiling on windows is not yet supported"
#endif

void compile_value_read(FILE *f, Operand *operand) {
    switch (operand->type) {
        case TOK_IDENTIFIER: {
            if (string_view_eq(operand->str, SV("_"))) {
                fprintf(f, ENTRY_POINT_NAME);
            } else {
                fprintf(f, "%.*s", SV_FORMAT(operand->str));
            }
        } break;
        case TOK_REGISTER: {
            int x86_64_register = operand->as_int + 8;
            fprintf(f, "r%d", x86_64_register);
        } break;
        case TOK_LITERAL_NUM: {
            fprintf(f, "%d", operand->as_int);
        } break;
        case TOK_LITERAL_CHAR: {
            fprintf(f, "%d", operand->as_int);
        } break;
        case TOK_ADDRESS: {
            fprintf(f, "[%d]", operand->as_int);
        } break;
        case TOK_ADDRESS_REG: {
            int x86_64_register = operand->as_int + 8;
            fprintf(f, "[r%d]", x86_64_register);
        } break;
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
        case TOK_ADDRESS: {
            // TODO: change this to qword when updating int to 64 bit
            fprintf(f, "dword [%d]", operand->as_int);
        } break;
        case TOK_ADDRESS_REG: {
            int x86_64_register = operand->as_int + 8;
            // TODO: change this to qword when updating int to 64 bit
            fprintf(f, "dword [r%d]", x86_64_register);
        } break;
        case TOK_LITERAL_NUM:
        default:
            fprintf(stderr, "invalid operand for writes: %s\n", TOKEN_STRING[operand->type]);
            UNREACHABLE();
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
    compile_instruction_to_str(f, "mov", "rcx", &opcode->operands[1]);
    compile_instruction_to_str(f, instruction, "rcx", &opcode->operands[2]);
    compile_mov_from_str(f, &opcode->operands[0], "rcx");
}

void compile_print_char(FILE *f, Operand *operand, bool add_newline) {
    fprintf(f, "    mov rbp, rsp\n");

    if (add_newline) {
        fprintf(f, "    dec rsp\n");
        fprintf(f, "    mov byte [rsp], 0xA\n");
    }
    fprintf(f, "    dec rsp\n");
    compile_instruction_to_str(f, "mov byte", "[rsp]", operand);

    fprintf(f, "    mov rax, 1\n");
    fprintf(f, "    mov rdi, 1\n");
    fprintf(f, "    mov rsi, rsp\n");
    fprintf(f, "    mov rdx, %d\n", add_newline? 2: 1);
    fprintf(f, "    syscall\n");

    fprintf(f, "    mov rsp, rbp\n");
}

void compile_print_int(FILE *f, Operand *operand, bool add_newline) {
    // TODO: make print a function and fix this temporary label hack
    static int print_count = 0;
    fprintf(f, "print_num_%d:\n", print_count++);

    // TODO: check if this can be done better
    fprintf(f, "    mov rbp, rsp\n");
    compile_instruction_to_str(f, "mov", "rsi", operand);
    fprintf(f, "    mov rbx, 10\n");
    fprintf(f, "    mov rcx, 0\n");

    if (add_newline) {
        fprintf(f, "    inc rcx\n");
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


void compile_read_stdin(FILE *f, Operand *operand_0, Operand *operand_1) {
    // TODO: make read a function and fix this temporary label hack
    static int read_count = 0;

    fprintf(f, "    mov rax, 0\n");
    fprintf(f, "    mov rdi, 0\n");
    compile_instruction_to_str(f, "mov", "rsi", operand_0);
    compile_instruction_to_str(f, "mov", "rdx", operand_1);
    fprintf(f, "    syscall\n");

    // replacing the first newline with a '\0'
    compile_instruction_to_str(f, "mov", "rax", operand_0);
    compile_instruction_to_str(f, "mov", "rbx", operand_1);
    fprintf(f, ".read_loop_%d:\n", read_count);
    fprintf(f, "    cmp rbx, 0\n");
    fprintf(f, "    jz .read_end_%d\n", read_count);
    fprintf(f, "    dec rbx\n");
    fprintf(f, "    cmp byte [rax], 0xA\n");
    fprintf(f, "    jz .read_out_%d\n", read_count);
    fprintf(f, "    inc rax\n");
    fprintf(f, "    jmp .read_loop_%d\n", read_count);
    fprintf(f, ".read_out_%d:\n", read_count);
    fprintf(f, "    mov byte [rax], 0\n");
    fprintf(f, ".read_end_%d:\n", read_count);

    read_count++;
}

void compile_exit_syscall(FILE *f) {
#ifdef _WIN32
#error "Cannot compile exit for windows"
#else
    fprintf(f, "\n\n; exit syscall\n");
    fprintf(f, "    mov rax, 60\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    syscall\n");
#endif
}


void compile_mmap_syscall(FILE *f, size_t len) {
#ifdef _WIN32
#error "Cannot compile mmap for windows"
#else
    fprintf(f, "\n\n; mmap syscall\n");

    fprintf(f, "    push r8\n");
    fprintf(f, "    push r9\n");
    fprintf(f, "    push r10\n");

    // mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    fprintf(f, "    mov rax, 9\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    mov rsi, %zu\n", len);
    fprintf(f, "    mov rdx, 1 | 2\n");
    fprintf(f, "    mov r10, 2 | 32\n");
    fprintf(f, "    mov r8, -1\n");
    fprintf(f, "    mov r9, 0\n");
    fprintf(f, "    syscall\n");

    // TODO: check rax for MAP_FAILED?
    // TODO: where to store pointer to allocated memory? (data section?)

    fprintf(f, "    pop r10\n");
    fprintf(f, "    pop r9\n");
    fprintf(f, "    pop r8\n");
#endif
}


// TODO: not all x86 instructions support memory operands
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
                // rdx needs to be zeroed
                fprintf(f, "    mov rdx, 0\n");
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
                // rdx needs to be zeroed
                fprintf(f, "    mov rdx, 0\n");
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
            fprintf(f, "\n; print opcode\n");
            if (opcode->operands[0].type == TOK_LITERAL_STR) {
                TODO("printing strings: not yet implemented");
            } else if (opcode->operands[0].type == TOK_LITERAL_CHAR) {
                compile_print_char(f, &opcode->operands[0], false);
            } else {
                compile_print_int(f, &opcode->operands[0], false);
            }
        } break;
        case OP_PRINTLN: {
            fprintf(f, "\n; println opcode\n");
            if (opcode->operands[0].type == TOK_LITERAL_STR) {
                TODO("printing strings: not yet implemented");
            } else if (opcode->operands[0].type == TOK_LITERAL_CHAR) {
                compile_print_char(f, &opcode->operands[0], true);
            } else {
                compile_print_int(f, &opcode->operands[0], true);
            }
        } break;
        case OP_PRINTB: {
            fprintf(f, "\n; printb opcode\n");
            fprintf(f, "    mov rax, 1\n");
            fprintf(f, "    mov rdi, 1\n");
            compile_instruction_to_str(f, "mov", "rsi", &opcode->operands[0]);
            compile_instruction_to_str(f, "mov", "rdx", &opcode->operands[1]);
            fprintf(f, "    syscall\n");
        } break;
        case OP_PRINTBLN: {
            fprintf(f, "\n; printbln opcode\n");
            fprintf(f, "    mov rax, 1\n");
            fprintf(f, "    mov rdi, 1\n");
            compile_instruction_to_str(f, "mov", "rsi", &opcode->operands[0]);
            compile_instruction_to_str(f, "mov", "rdx", &opcode->operands[1]);
            fprintf(f, "    syscall\n");

            // writing a newline
            fprintf(f, "    mov rbp, rsp\n");
            fprintf(f, "    dec rsp\n");
            fprintf(f, "    mov byte [rsp], 0xA\n");
            fprintf(f, "    mov rax, 1\n");
            fprintf(f, "    mov rdi, 1\n");
            fprintf(f, "    mov rsi, rsp\n");
            fprintf(f, "    mov rdx, 1\n");
            fprintf(f, "    syscall\n");
            fprintf(f, "    mov rsp, rbp\n");
        } break;
        case OP_READ: {
            fprintf(f, "\n; read opcode\n");
            compile_read_stdin(f, &opcode->operands[0], &opcode->operands[1]);
        } break;
        case OP_PUSH: {
            fprintf(f, "    push ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        } break;
        case OP_POP: { 
            fprintf(f, "    pop ");
            compile_value_write(f, &opcode->operands[0]);
            fprintf(f, "\n");
        } break;
        case OP_CMP: {
            if ((opcode->operands[0].type == TOK_LITERAL_NUM) && (opcode->operands[1].type == TOK_LITERAL_NUM)) {
                compile_instruction_to_str(f, "mov", "rax", &opcode->operands[0]);
                compile_instruction_to_str(f, "mov", "rbx", &opcode->operands[1]);
                fprintf(f, "    cmp rax, rbx\n");
            } else if (opcode->operands[0].type == TOK_ADDRESS || opcode->operands[0].type == TOK_ADDRESS_REG) {
                compile_instruction_to_str(f, "mov", "rax", &opcode->operands[0]);
                compile_instruction_to_str(f, "cmp", "rax", &opcode->operands[1]);
            } else {
                fprintf(f, "    cmp ");
                compile_value_read(f, &opcode->operands[0]);
                fprintf(f, ", ");
                compile_value_read(f, &opcode->operands[1]);
                fprintf(f, "\n");
            }
        } break;
        case OP_JUMP: {
            if (opcode->operands[0].type != TOK_IDENTIFIER) {
                TODO("OP_JUMP: implement jumping to non labels");
            }
            fprintf(f, "    jmp ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        } break;
        case OP_JUMPZ: {
            if (opcode->operands[0].type != TOK_IDENTIFIER) {
                TODO("OP_JUMPZ: implement jumping to non labels");
            }
            fprintf(f, "    jz ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        }break;
        case OP_JUMPG: {
            if (opcode->operands[0].type != TOK_IDENTIFIER) {
                TODO("OP_JUMPG: implement jumping to non labels");
            }
            fprintf(f, "    jg ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        }break;
        case OP_JUMPL: {
            if (opcode->operands[0].type != TOK_IDENTIFIER) {
                TODO("OP_JUMPL: implement jumping to non labels");
            }
            fprintf(f, "    jl ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        } break;
        case OP_CALL: {
            if (opcode->operands[0].type != TOK_IDENTIFIER) {
                TODO("OP_CALL: implement jumping to non labels");
            }
            fprintf(f, "    call ");
            compile_value_read(f, &opcode->operands[0]);
            fprintf(f, "\n");
        } break;
        case OP_RETURN: { 
            fprintf(f, "    ret\n");
        } break;
        case OP_COUNT:
            UNREACHABLE_INFO("OP_COUNT is not a valid opcode");
    }
    return true;
}

bool compile(const char *output_path, Labels labels, OpCodes opcodes) {
    FILE *f = fopen(output_path, "w");
    fprintf(f, "section .text\n");
    fprintf(f, "    global _start\n");
    fprintf(f, "\n");
    fprintf(f, "_start:\n");

    // initializing allocated registers for bass to 0
    for (int i = 8; i <= 15; i++) {
        fprintf(f, "    xor r%d, r%d\n", i, i);
    }

    // TODO: save rax returned by mmap somewhere
    compile_mmap_syscall(f, MEMORY_SIZE);

    int entry = find_label(&labels, SV("_"));
    if (entry != -1) {
        labels.data[entry].name = SV(ENTRY_POINT_NAME);
        fprintf(f, "    jmp %s\n", ENTRY_POINT_NAME);
    }

    size_t i = 0, j = 0;
    while (i < labels.size && j < opcodes.size) {
        if (labels.data[i].index == j) {
            fprintf(f, "%.*s: ; (opcode: %s)\n",
                    SV_FORMAT(labels.data[i].name),
                    OPCODES[opcodes.data[labels.data[i].index].op].name);
            i++;
        } else {
            compile_opcode(f, &opcodes.data[j++]);
        }
    }

    for (; i < labels.size; i++) {
        fprintf(f, "%.*s: ; (opcode: %s)\n",
                SV_FORMAT(labels.data[i].name),
                OPCODES[opcodes.data[labels.data[i].index].op].name);
    }

    for (; j < opcodes.size; j++) {
        compile_opcode(f, &opcodes.data[j]);
    }

    compile_exit_syscall(f);
    fclose(f);
    return true;
}


