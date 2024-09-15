#include "utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"
#include "parser.h"

void parser_init(Parser *parser, StringView source_code) {
    parser->source = source_code;
    parser->start = 0;
    parser->end = 0;
}

char next(Parser *parser) {
    if (parser->end < parser->source.length) {
        char next = parser->source.data[parser->end];
        parser->end++;
        return next;
    }
    return '\0';
}

char peek(Parser *parser) {
    if (parser->end < parser->source.length) {
        return parser->source.data[parser->end];
    }
    return '\0';
}

char current(Parser *parser) {
    if (parser->end > 0) {
        return parser->source.data[parser->end - 1];
    }
    return '\0';
}

#define get_string(parser)                                                     \
    (StringView) {                                                             \
        &(parser)->source.data[(parser)->start],                               \
            (parser)->end - (parser)->start                                    \
    }

// #define get_slice(parser, start, end)
// (StringView){&(parser)->source.data[(start)], (end) - (start)}

bool parse_num(Parser *parser, long *num, StringView *string) {
    while (isalnum(peek(parser))) {
        next(parser);
    }
    if (!(isspace(peek(parser)) || peek(parser) == '\0')) {
        fprintf(stderr, "bass: unexpected character: `%c` at: %zu\n",
                peek(parser), parser->end);
        return false;
    }
    *string = get_string(parser);
    if (string->length <= 1) {
        fprintf(stderr, "bass: expected value at: %zu\n", parser->start);
        return false;
    }
    *num = strtol(&parser->source.data[parser->start + 1], NULL, 0);
    return true;
}

StringView parse_identifier(Parser *parser) {
    while (isalnum(peek(parser))) {
        next(parser);
    }
    return get_string(parser);
}

bool get_opcode(char *string, OpType *type) {
    for (size_t i = 0; i < OP_COUNT; i++) {
        if (strcmp(string, OPCODES[i].name) == 0) {
            *type = i;
            return true;
        }
    }
    return false;
}

bool parse_operands(Parser *parser, OpType type,
                    Operand operands[MAX_OPERANDS]) {
    int i = 0;
    while (i < OPCODES[type].arity) {
        char current = next(parser);
        long num;
        StringView string;
        switch (current) {
        case 'r': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            if (num >= REG_COUNT) {
                fprintf(
                    stderr,
                    "bass: invalid register: `%ld` at %zu. Registers can range "
                    "from 0 to %d\n",
                    num, parser->start, REG_COUNT);
                return false;
            }
            operands[i++] = (Operand){TOK_REGISTER, string, num};
        } break;
        case '#': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            operands[i++] = (Operand){TOK_VALUE, string, num};
        } break;
        case '@': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            operands[i++] = (Operand){TOK_ADDRESS, string, num};
        } break;
        default: {
            if (isalpha(current)) {
                fprintf(stderr,
                        "bass: not enough operands for opcode `%s`, expected "
                        "%d but got %d\n",
                        OPCODES[type].name, OPCODES[type].arity, i);
                return false;
            } else if (!isspace(current)) {
                fprintf(stderr,
                        "bass: expected register, value or memory address but "
                        "got `%c` "
                        "at: %zu\n",
                        current, parser->end);
                return false;
            }
        }
        }
        parser->start = parser->end;
    }
    return true;
}

bool parse_label(Parser *parser, Operand *operand) {
    if (isalpha(next(parser))) {
        StringView string = parse_identifier(parser);
        parser->start = parser->end;
        *operand = (Operand){TOK_LABEL, string, -1};
        return true;
    }
    return false;
}

bool parse(Parser *parser, OpCodes *opcodes, Labels *labels) {
    char current;
    size_t op_index = 0;
    while ((current = next(parser))) {
        if (isalpha(current)) {
            StringView string = parse_identifier(parser);
            parser->start = parser->end;
            char next_char = next(parser);
            if (next_char == ':') {
                Label label = {string, op_index};
                dyn_append(labels, label);
            } else if ((isspace(next_char) || next_char == '\0')) {
                OpType op_type;
                char *str = string_view_to_cstring(string);
                if (!get_opcode(str, &op_type)) {
                    fprintf(stderr, "bass: invalid opcode `%s` at: %zu\n", str,
                            parser->start);
                    free(str);
                    return false;
                }
                free(str);
                parser->start = parser->end;
                Operand operands[MAX_OPERANDS] = {0};
                if (op_type == OP_JUMP || op_type == OP_JUMPZ ||
                    op_type == OP_JUMPG || op_type == OP_JUMPL) {
                    if (!parse_label(parser, &operands[0])) {
                        return false;
                    }
                } else {
                    if (!parse_operands(parser, op_type, operands)) {
                        return false;
                    }
                }
                OpCode opcode;
                opcode.op = op_type;
                memcpy(&opcode.operands, operands,
                       sizeof(Operand) * MAX_OPERANDS);
                dyn_append(opcodes, opcode);
                op_index++;
            } else {
                fprintf(stderr, "bass: unexpected character `%c` at: %zu\n",
                        next_char, parser->end);
                return false;
            }
            // skip comments
        } else if (current == ';') {
            while ((next(parser)) != '\n')
                ;
        } else if (!(isspace(current) || current == '\0')) {
            fprintf(stderr,
                    "bass: expected opcode or label, got `%c` at: %zu\n",
                    current, parser->end);
            return false;
        }
        parser->start = parser->end;
    }
    return true;
}

void patch_labels(OpCodes *opcodes, Labels labels) {
    for (size_t i = 0; i < opcodes->size; i++) {
        OpCode opcode = opcodes->data[i];
        if (opcode.op == OP_JUMP || opcode.op == OP_JUMPZ ||
            opcode.op == OP_JUMPG || opcode.op == OP_JUMPL) {
            StringView opcode_label = opcode.operands[0].string;
            for (size_t j = 0; j < labels.size; j++) {
                if (string_view_eq(opcode_label, labels.data[j].name)) {
                    opcodes->data[i].operands[0].value = labels.data[j].index;
                    break;
                }
            }
        }
    }
}

void display_opcodes(OpCodes ops) {
    for (size_t i = 0; i < ops.size; i++) {
        OpCode t = ops.data[i];
        printf("OpCode: %s\n", OPCODES[t.op].name);
        for (int i = 0; i < OPCODES[t.op].arity; i++) {
            int val = t.operands[i].value;
            switch (t.operands[i].type) {
            case TOK_REGISTER:
                printf("\tREGISTER: %d\n", val);
                break;
            case TOK_VALUE:
                printf("\tVALUE: %d\n", val);
                break;
            case TOK_ADDRESS:
                printf("\tADDRESS: %d\n", val);
                break;
            case TOK_LABEL: {
                char *str = string_view_to_cstring(t.operands[i].string);
                printf("\tLABEL: %s (to opcode: %d)\n", str, val);
                free(str);
            } break;
            }
        }
    }
}

void display_labels(Labels lbls) {
    for (size_t i = 0; i < lbls.size; i++) {
        Label t = lbls.data[i];
        char *str = string_view_to_cstring(t.name);
        printf("Label: %s (opcode: %zu)\n", str, t.index);
        free(str);
    }
}
