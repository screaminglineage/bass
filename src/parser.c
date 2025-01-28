// TODO: do not use ctype.h
#include <ctype.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"
#include "parser.h"
#include "utils.h"

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

static inline const char *peek_ref(Parser *parser) {
    if (parser->end > 0) {
        return &parser->source.data[parser->end];
    }
    return NULL;
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

bool parse_num(Parser *parser, long *num, StringView *string) {
    int skip = parser->end - parser->start;
    // TODO: maybe remove skip as it's always 1
    assert(skip == 1);

    // allow negative integers
    if (peek(parser) == '-') {
        next(parser);
    } else if (!isdigit(peek(parser))) {
        fprintf(stderr, "bass:%d:%zu: unexpected character: `%c`\n",
                parser->line, get_col(parser), peek(parser));
        return false;
    }

    while (isalnum(peek(parser))) {
        next(parser);
    }

    if (!(isspace(peek(parser)) || peek(parser) == '\0')) {
        fprintf(stderr, "bass:%d:%zu: unexpected character `%c`\n",
                parser->line, get_col(parser), peek(parser));
        return false;
    }

    *string = get_string(parser);
    if (string->length <= 1) {
        fprintf(stderr, "bass:%d:%zu: expected number, got `%.*s`\n",
                parser->line, get_col_start(parser), SV_FORMAT(*string));
        return false;
    }

    const char *start = &parser->source.data[parser->start + skip];
    char *endptr;
    *num = strtol(start, &endptr, 0);
    // TODO: print the error from string.data + skip
    if (endptr != &string->data[string->length]) {
        fprintf(stderr, "bass:%d:%zu: expected number, got `%.*s`\n",
                parser->line, get_col_start(parser), SV_FORMAT(*string));
        return false;
    }
    return true;
}


static inline StringView parse_identifier(Parser *parser) {
    while (isalnum(peek(parser)) || peek(parser) == '_') {
        next(parser);
    }
    return get_string(parser);
}

// parses character or string literals delimited by `quote`
// TODO: parse escape characters
bool parse_quoted_char(Parser *parser, StringView *string, char quote,
                       const char *type) {
    while (peek(parser) != quote && peek(parser) != '\0' && peek(parser) != '\n') {
        next(parser);
    }
    if (peek(parser) != quote) {
        fprintf(stderr, "bass:%d:%zu: unterminated %s literal\n",
                parser->line, get_col_start(parser), type);
        return false;
    }
    *string = get_slice(parser, parser->start + 1, parser->end);
    next(parser);
    return true;
}


bool parse_register_from_identifier(StringView identifier, long *num) {
    if (identifier.length != 2) return false;
    if (identifier.data[0] != 'r') return false;
    if (!('0' <= identifier.data[1] && identifier.data[1] < REG_COUNT + '0')) {
        return false;
    }
    *num = identifier.data[1] - '0';
    return true;
}

bool next_token(Parser *parser, Token *token);

bool parse_operands(Parser *parser, OpType op_type, TokenType start, TokenType end, OpCode *opcode) {
    Operand operands[MAX_OPERANDS] = {0};
    for (int i = 0; i < OPCODES[op_type].arity; i++) {
        Token operand = {0};
        if (!next_token(parser, &operand)) {
            return false;
        }
        if (start <= operand.type && operand.type <= end) { 
            operands[i] = operand;
        } else {
            fprintf(stderr, "bass:%d:%zu: error: unexpected %s, `%.*s`, after opcode `%s`\n",
                    operand.line, operand.col, TOKEN_STRING[operand.type], 
                    SV_FORMAT(operand.str), OPCODES[op_type].name);
            fprintf(stderr, "help: opcode `%s` takes %d argument(s)\n",
                    OPCODES[op_type].name, OPCODES[op_type].arity);
            return false;
        }
    }
    opcode->op = op_type;
    memcpy(&opcode->operands, operands, sizeof(*operands) * MAX_OPERANDS);
    return true;
}

bool parse_opcode(Parser *parser, Token opcode_token, OpCode *opcode) {
    OpType op_type = opcode_token.as_opcode;
    if (op_type == OP_JUMP || op_type == OP_JUMPZ 
        || op_type == OP_JUMPG || op_type == OP_JUMPL 
        || op_type == OP_CALL) {
        if (!parse_operands(parser, op_type, TOK_IDENTIFIER, TOK_ADDRESS_REG, opcode))
            return false;
    } else if (op_type == OP_PRINT || op_type == OP_PRINTLN) {
        if (!parse_operands(parser, op_type, TOK_REGISTER, TOK_LITERAL_STR, opcode))
            return false;
    } else {
        if (!parse_operands(parser, op_type, TOK_REGISTER, TOK_ADDRESS_REG, opcode))
            return false;
    }
    opcode->line = opcode_token.line;
    opcode->col = opcode_token.col;
    return true;
}

#define MAKE_TOKEN(parser, type, string, value) \
    ((Token){(parser)->line, get_col_start((parser)), (type), (string), {(value)}})



// TODO: the function assigns a value of long to an int (num is long, Token has int member variable)
bool next_token(Parser *parser, Token *token) {
    char current = 0;
    long num = 0;
    StringView string = {0};

    do {
        while (isspace(peek(parser))) {
            current = next(parser);
            if (current == '\n') {
                parser->line_start = parser->end;
                parser->line += 1;
            }
        }
        while (peek(parser) == ';') {
            while ((next(parser)) != '\n');
            parser->line += 1;
        }
    } while(isspace(peek(parser)));
    parser->start = parser->end;

    current = next(parser);
    switch (current) {
        case '"': {
            if (!parse_quoted_char(parser, &string, '\"', "string")) {
                return false;
            }
            *token = MAKE_TOKEN(parser, TOK_LITERAL_STR, string, 0);
            return true;
        } break;
        case '\'': {
            if (!parse_quoted_char(parser, &string, '\'', "character")) {
                return false;
            }
            if (string.length == 0) {
                fprintf(stderr, "bass:%d:%zu: empty character literal\n", 
                        parser->line, get_col_start(parser));
                return false;
            }

            // newline escape character
            if (string.data[0] == '\\' && string.length == 2 &&
                string.data[1] == 'n') {
                *token = MAKE_TOKEN(parser, TOK_LITERAL_CHAR, string, '\n');
            } else {
                // regular character
                if (string.length > 1) {
                    fprintf(stderr, "bass:%d:%zu character literal: `%.*s` is too long\n",
                            parser->line, get_col_start(parser), SV_FORMAT(string));
                    return false;
                }
                *token = MAKE_TOKEN(parser, TOK_LITERAL_CHAR, string, string.data[0]);
            }
        } break;
        case '#': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            *token = MAKE_TOKEN(parser, TOK_LITERAL_NUM, string, num);
        } break;
        case '@': {
            // parsing as memory address
            if (isdigit(peek(parser))) {
                if (!parse_num(parser, &num, &string)) {
                    return false;
                }
                *token = MAKE_TOKEN(parser, TOK_ADDRESS, string, num);

                // parsing as address at register
            } else if (peek(parser) == 'r') {
                next(parser);
                StringView identifier = parse_identifier(parser);
                if (!parse_register_from_identifier((StringView){identifier.data+1, identifier.length - 1} , &num)) {
                    return false;
                }
                *token = MAKE_TOKEN(parser, TOK_ADDRESS_REG, identifier, num);
            } else {
                if (peek(parser) == '\n') {
                    fprintf(
                        stderr,
                        "bass:%d:%zu: expected register or value after `@` got `\\n`\n",
                        parser->line, get_col_start(parser) + 2);
                } else {
                    fprintf(
                        stderr,
                        "bass:%d:%zu: expected register or value after `@` got `%c`\n",
                        parser->line, get_col_start(parser) + 2, peek(parser));
                }
                return false;
            }
        } break;

        default: {
            if (isalpha(current)) {
                StringView identifier = parse_identifier(parser);
                if (parse_register_from_identifier(identifier, &num)) {
                    *token = MAKE_TOKEN(parser, TOK_REGISTER, identifier, num);
                } else {
                    string = identifier;
                    if (peek(parser) == ':') {
                        next(parser);
                        *token = MAKE_TOKEN(parser, TOK_LABEL, string, -1);
                    } else {
                        OpType op_type = 0;
                        if (get_opcode(string, &op_type)) {
                            *token = MAKE_TOKEN(parser, TOK_OPCODE, string, op_type);
                        } else {
                            *token = MAKE_TOKEN(parser, TOK_IDENTIFIER, string, -1);
                        }
                    }
                }
            } else if (current == '\0') {
                *token = MAKE_TOKEN(parser, TOK_EOF, (StringView){0}, -1);
            } else {
                fprintf(stderr, "bass:%d:%zu unexpected character `%c`\n",
                        parser->line, get_col_start(parser), current);

                if (isdigit(current)) {
                    fprintf(
                        stderr,
                        "help: try prefixing `%c` with `r` for register, `#` "
                        "for a literal value or `@` for a memory address\n",
                        current);
                }
                return false;
            }
        }
    }
    parser->start = parser->end;
    return true;
}

bool parse(Parser *parser, OpCodes *opcodes, Labels *labels) {
    int op_index = 0;
    while (true) {
        Token tok = {0};
        if (!next_token(parser, &tok)) {
            return false;
        }
        // printf("%d:%zu: %s `%.*s`\n", tok.line, tok.col, TOKEN_STRING[tok.type], SV_FORMAT(tok.str));

        switch (tok.type) {
            case TOK_LABEL: {
                dyn_append(labels, ((Label){tok.str, op_index}));
            } break;
            case TOK_OPCODE: {
                OpCode opcode = {0};
                if (!parse_opcode(parser, tok, &opcode)) {
                    return false;
                }
                dyn_append(opcodes, opcode);
                op_index++;
            } break;
            case TOK_EOF: return true;
            default: {
                fprintf(stderr,
                        "bass:%d:%zu: error: expected opcode or label, got %s, `%.*s`\n",
                        tok.line, tok.col, TOKEN_STRING[tok.type], SV_FORMAT(tok.str));
                return false;
            } break;
        }
    }
}


// TODO: Duplicate labels cause only the last one to be valid.
// Make `Labels` a hashmap or set instead or check for duplicate labels.
void patch_labels(OpCodes *opcodes, Labels labels) {
    for (size_t i = 0; i < labels.size; i++) {
        StringView label_name = labels.data[i].name;
        for (size_t j = 0; j < opcodes->size; j++) {
            OpCode opcode = opcodes->data[j];
            if (opcode.op == OP_JUMP || opcode.op == OP_JUMPZ ||
                opcode.op == OP_JUMPG || opcode.op == OP_JUMPL ||
                opcode.op == OP_CALL) {
                StringView opcode_label = opcode.operands[0].str;
                if (string_view_eq(opcode_label, label_name)) {
                    opcodes->data[j].operands[0].as_int = labels.data[i].index;
                }
            }
        }
    }
}


void display_opcodes(OpCodes ops) {
    for (size_t i = 0; i < ops.size; i++) {
        OpCode op = ops.data[i];
        printf("OpCode: %s\n", OPCODES[op.op].name);
        for (int i = 0; i < OPCODES[op.op].arity; i++) {
            int val = op.operands[i].as_int;
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
                printf("\tVALUE: %.*s\n", SV_FORMAT(op.operands[0].str));
                break;
            case TOK_ADDRESS:
                printf("\tADDRESS: %d\n", val);
                break;
            case TOK_ADDRESS_REG:
                printf("\tADDRESS AT REGISTER: %d\n", val);
                break;
            case TOK_IDENTIFIER: {
                StringView str = op.operands[i].str;
                printf("\tLABEL: %.*s (to opcode: %d)\n", SV_FORMAT(str), val);
            } break;
            case TOK_LABEL:
            case TOK_OPCODE:
            case TOK_EOF:
            case TOK_COUNT:
                UNREACHABLE_INFO("Incorrect type as operand");
            }
        }
    }
}

void display_labels(Labels lbls) {
    for (size_t i = 0; i < lbls.size; i++) {
        Label t = lbls.data[i];
        printf("Label: %.*s (opcode: %zu)\n", SV_FORMAT(t.name), t.index);
    }
}

