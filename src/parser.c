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

static inline char next(Parser *parser) {
    if (parser->end < parser->source.length) {
        char next = parser->source.data[parser->end];
        parser->end++;
        return next;
    }
    return '\0';
}

static inline char peek(Parser *parser) {
    if (parser->end < parser->source.length) {
        return parser->source.data[parser->end];
    }
    return '\0';
}

static inline char *peek_ref(Parser *parser) {
    if (parser->end > 0) {
        return &parser->source.data[parser->end];
    }
    return NULL;
}

#define get_string(parser)                                                     \
    (StringView) {                                                             \
        &(parser)->source.data[(parser)->start],                               \
            (parser)->end - (parser)->start                                    \
    }

bool get_opcode(StringView string, OpType *type) {
    for (size_t i = 0; i < OP_COUNT; i++) {
        if (string_view_cstring_eq(string, OPCODES[i].name)) {
            *type = i;
            return true;
        }
    }
    return false;
}

// #define get_slice(parser, start, end)
// (StringView){&(parser)->source.data[(start)], (end) - (start)}

bool parse_num(Parser *parser, long *num, StringView *string) {
    // allow negative integers
    if (!(isdigit(peek(parser)) || peek(parser) == '-')) {
        fprintf(stderr, "bass: unexpected character: `%c` at: %zu\n",
                peek(parser), parser->end);
        return false;
    }
    next(parser);
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

// TODO: Reset the parser->start
static inline StringView parse_identifier(Parser *parser) {
    while (isalnum(peek(parser))) {
        next(parser);
    }
    return get_string(parser);
}

// parses character or string literals delimited by `quote`
bool parse_quoted(Parser *parser, StringView *string, char quote,
                  const char *type) {
    parser->start = parser->end;
    while (peek(parser) != quote && peek(parser) != '\0') {
        next(parser);
    }
    if (peek(parser) != quote) {
        fprintf(stderr, "bass: unterminated %s literal at %zu\n", type,
                parser->start);
        return false;
    }
    *string = get_string(parser);
    next(parser);
    parser->start = parser->end;
    return true;
}

bool parse_operands(Parser *parser, OpType op, Operand operands[MAX_OPERANDS]) {
    int i = 0;
    while (i < OPCODES[op].arity) {
        char current = next(parser);
        long num;
        StringView string;
        switch (current) {
        case 'r': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            if (0 < num || num >= REG_COUNT) {
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
            operands[i++] = (Operand){TOK_LITERAL_NUM, string, num};
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
                        OPCODES[op].name, OPCODES[op].arity, i);
                return false;
            } else if (!isspace(current)) {
                fprintf(stderr,
                        "bass: expected register, value or memory address but "
                        "got `%c` after opcode: `%s` at: %zu\n",
                        current, OPCODES[op].name, parser->end);
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

bool parse_print(Parser *parser, Operand *operand) {
    char current = peek(parser);
    switch (current) {
    // TODO: parse escape characters
    case '\'': {
        next(parser);
        StringView string;
        if (!parse_quoted(parser, &string, '\'', "character")) {
            return false;
        }
        if (string.length > 1) {
            fprintf(stderr,
                    "bass: character literal: `%.*s` is too long at %zu\n",
                    SV_FORMAT(string), parser->end);
            return false;
        }
        *operand = (Operand){TOK_LITERAL_CHAR, string, string.data[0]};
        return true;
    }
    case '\"': {
        next(parser);
        StringView string;
        if (!parse_quoted(parser, &string, '\"', "string")) {
            return false;
        }
        *operand = (Operand){TOK_LITERAL_STR, string, 0};
        return true;
    }
    default:
        return parse_operands(parser, OP_PRINT, operand);
    }
}

bool parse_opcode(Parser *parser, StringView string, OpCode *opcode) {
    OpType op_type;
    if (!get_opcode(string, &op_type)) {
        fprintf(stderr, "bass: invalid opcode `%.*s` at: %zu\n",
                (int)string.length, string.data, parser->start);
        return false;
    }
    parser->start = parser->end;
    Operand operands[MAX_OPERANDS] = {0};
    if (op_type == OP_JUMP || op_type == OP_JUMPZ || op_type == OP_JUMPG ||
        op_type == OP_JUMPL) {
        if (!parse_label(parser, &operands[0])) {
            return false;
        }
    } else if (op_type == OP_PRINT) {
        if (!parse_print(parser, operands)) {
            return false;
        }
    } else {
        if (!parse_operands(parser, op_type, operands)) {
            return false;
        }
    }
    opcode->op = op_type;
    memcpy(&opcode->operands, operands, sizeof(Operand) * MAX_OPERANDS);
    return true;
}

bool parse(Parser *parser, OpCodes *opcodes, Labels *labels) {
    char current;
    size_t op_index = 0;
    while ((current = next(parser))) {
        if (isalpha(current)) {
            StringView string = parse_identifier(parser);
            parser->start = parser->end;
            char next_char = next(parser);
            // parse label
            if (next_char == ':') {
                Label label = {string, op_index};
                dyn_append(labels, label);

                // parse opcode
            } else if ((isspace(next_char) || next_char == '\0')) {
                OpCode opcode;
                if (!parse_opcode(parser, string, &opcode)) {
                    return false;
                }
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

// TODO: Multiple labels with the same name causes only the first one to be
// valid. Maybe make `Labels` a hashmap or set instead
bool patch_labels(OpCodes *opcodes, Labels labels) {
    for (size_t i = 0; i < opcodes->size; i++) {
        bool found = false;
        OpCode opcode = opcodes->data[i];
        if (opcode.op == OP_JUMP || opcode.op == OP_JUMPZ ||
            opcode.op == OP_JUMPG || opcode.op == OP_JUMPL) {
            StringView opcode_label = opcode.operands[0].string;
            for (size_t j = 0; j < labels.size; j++) {
                if (string_view_eq(opcode_label, labels.data[j].name)) {
                    opcodes->data[i].operands[0].value = labels.data[j].index;
                    found = true;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr,
                        "bass: couldnt find label: `%.*s` at opcode: `%s`\n",
                        SV_FORMAT(opcode_label), OPCODES[opcode.op].name);
                return false;
            }
        }
    }
    return true;
}

void display_opcodes(OpCodes ops) {
    for (size_t i = 0; i < ops.size; i++) {
        OpCode op = ops.data[i];
        printf("OpCode: %s\n", OPCODES[op.op].name);
        for (int i = 0; i < OPCODES[op.op].arity; i++) {
            int val = op.operands[i].value;
            switch (op.operands[i].type) {
            case TOK_REGISTER:
                printf("\tREGISTER: %d\n", val);
                break;
            case TOK_LITERAL_NUM:
                printf("\tVALUE: %d\n", val);
                break;
            case TOK_LITERAL_CHAR:
                printf("\tVALUE: %c\n", val);
                break;
            case TOK_LITERAL_STR:
                printf("\tVALUE: %.*s\n", SV_FORMAT(op.operands[0].string));
                break;
            case TOK_ADDRESS:
                printf("\tADDRESS: %d\n", val);
                break;
            case TOK_LABEL: {
                StringView str = op.operands[i].string;
                printf("\tLABEL: %.*s (to opcode: %d)\n", SV_FORMAT(str), val);

            } break;
            }
        }
    }
}

void display_labels(Labels lbls) {
    for (size_t i = 0; i < lbls.size; i++) {
        Label t = lbls.data[i];
        printf("Label: %.*s (opcode: %zu)\n", (int)t.name.length, t.name.data,
               t.index);
    }
}
