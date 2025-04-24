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

bool get_opcode(StringView string, OpType *type) {
    for (size_t i = 0; i < OP_COUNT; i++) {
        if (string_view_cstring_eq(string, OPCODES[i].name)) {
            *type = i;
            return true;
        }
    }
    return false;
}

bool is_space(char ch) {
    return ch == ' ' || ch == '\n' || ch == '\t'
        || ch == '\f' || ch == '\r' || ch == '\v';
}

bool is_alpha(char ch) {
    return ('A' <= ch && ch <= 'Z')
        || ('a' <= ch && ch <= 'z');
}

bool is_digit(char ch) {
    return '0' <= ch && ch <= '9';
}

bool is_alnum(char ch) {
    return is_alpha(ch) || is_digit(ch);
}

bool parse_num(Parser *parser, int *num, StringView *string) {
    // skip the `#` before numeric literals
    int skip = parser->end - parser->start;

    while (!is_space(peek(parser)) && peek(parser) != '\0') {
        next(parser);
    }

    *string = get_string(parser);
    if (string->length == 1) {
        fprintf(stderr, "bass:%d:%zu: expected number, got `%c`\n",
                parser->line, get_col(parser) + skip, peek(parser));
        return false;
    }

    const char *start = &parser->source.data[parser->start + skip];
    char *endptr;
    // TODO: assigning a long to an int here
    *num = strtol(start, &endptr, 0);
    if (endptr != &string->data[string->length]) {
        fprintf(stderr, "bass:%d:%zu: expected number, got `%.*s`\n",
                parser->line, get_col(parser) + skip,
                SV_FORMAT(get_slice(parser, parser->start+1, parser->end)));
        return false;
    }
    return true;
}

static inline StringView parse_identifier(Parser *parser) {
    while (is_alnum(peek(parser)) || peek(parser) == '_') {
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
                parser->line, get_col(parser), type);
        return false;
    }
    *string = get_slice(parser, parser->start + 1, parser->end);
    next(parser);
    return true;
}


bool parse_register(StringView string, int *reg_num) {
    if (string.length != 2) return false;
    if (string.data[0] != 'r') return false;
    *reg_num = string.data[1] - '0';
    if (*reg_num < 0 || *reg_num >= REG_COUNT) {
        return false;
    }
    return true;
}

bool next_token(Parser *parser, Token *token);

// start <= <parsed token> <= end denotes the range 
// in which <parsed token> should fall to be an operand of an opcode
// see the TokenType enum for more info
bool parse_operands(Parser *parser, OpType op_type, TokenType start, TokenType end, OpCode *opcode) {
    for (int i = 0; i < OPCODES[op_type].arity; i++) {
        Token operand = {0};
        if (!next_token(parser, &operand)) {
            return false;
        }
        if (start <= operand.type && operand.type <= end) { 
            opcode->operands[i] = operand;
        } else {
            fprintf(stderr, "bass:%d:%zu: error: unexpected %s", operand.line, operand.col, TOKEN_STRING[operand.type]);
            if (operand.type != TOK_EOF) fprintf(stderr, ", `%.*s`,", SV_FORMAT(operand.str));
            fprintf(stderr, " after opcode `%s`\n", OPCODES[op_type].name);

            fprintf(stderr, "help: opcode `%s` takes %d argument(s)\n",
                    OPCODES[op_type].name, OPCODES[op_type].arity);
            return false;
        }
    }
    opcode->op = op_type;
    return true;
}

bool parse_opcode(Parser *parser, Token opcode_token, OpCode *opcode) {
    OpType op_type = opcode_token.as_opcode;
    if (op_type == OP_JUMP || op_type == OP_JUMPZ
        || op_type == OP_JUMPG || op_type == OP_JUMPL
        || op_type == OP_CALL) {
        if (!parse_operands(parser, op_type, TOK_IDENTIFIER, TOK_ADDRESS_REG, opcode))
            return false;
    } else if (op_type == OP_PRINT || op_type == OP_PRINTLN || op_type == OP_STORE) {
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

bool next_token(Parser *parser, Token *token) {
    char current = 0;
    int num = 0;
    StringView string = {0};

    do {
        while (is_space(peek(parser))) {
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
    } while(is_space(peek(parser)));
    parser->start = parser->end;

    current = next(parser);
    switch (current) {
        case '"': {
            if (!parse_quoted_char(parser, &string, '\"', "string")) {
                return false;
            }
            *token = make_token(parser, TOK_LITERAL_STR, string, 0);
            return true;
        } break;
        case '\'': {
            if (!parse_quoted_char(parser, &string, '\'', "character")) {
                return false;
            }
            if (string.length == 0) {
                fprintf(stderr, "bass:%d:%zu: empty character literal\n", 
                        parser->line, get_col(parser));
                return false;
            }

            // newline escape character
            if (string.data[0] == '\\' && string.length == 2 &&
                string.data[1] == 'n') {
                *token = make_token(parser, TOK_LITERAL_CHAR, string, '\n');
            } else if (string.length > 1){
                    fprintf(stderr, "bass:%d:%zu character literal: `%.*s` is too long\n",
                            parser->line, get_col(parser), SV_FORMAT(string));
                    return false;
            } else {
                // regular character
                *token = make_token(parser, TOK_LITERAL_CHAR, string, string.data[0]);
            }
        } break;
        case '#': {
            if (!parse_num(parser, &num, &string)) {
                return false;
            }
            *token = make_token(parser, TOK_LITERAL_NUM, string, num);
        } break;
        case '@': {
            // parsing as memory address
            if (is_digit(peek(parser))) {
                if (!parse_num(parser, &num, &string)) {
                    return false;
                }
                *token = make_token(parser, TOK_ADDRESS, string, num);

            } else if (peek(parser) == '\n') {
                fprintf(
                    stderr,
                    "bass:%d:%zu: error: expected register or number after `@` got `\\n`\n",
                    parser->line, get_col(parser) + 2);
                return false;

            } else {
                // resetting parser to prevent `@` being picked up in parse_identifier
                parser->start = parser->end;
                // parsing as address at register
                string = parse_identifier(parser);
                if (!is_space(peek(parser)) && peek(parser) != '\0') {
                    fprintf(stderr, "bass:%d:%zu error: unexpected character `%c`\n",
                            parser->line, get_col(parser), peek(parser));
                    return false;
                }
                if (!parse_register(string, &num)) {
                    fprintf(stderr, "bass:%d:%zu: error: expected register or number after `@` got `%c`\n",
                            parser->line, get_col(parser),
                            (string.length > 0)? string.data[0]: peek(parser));
                    return false;
                }
                *token = make_token(parser, TOK_ADDRESS_REG, ((StringView){string.data-1, string.length+1}), num);
            }
        } break;

        default: {
            if (is_alpha(current) || current == '_') {
                string = parse_identifier(parser);
                if (!is_space(peek(parser)) && peek(parser) != '\0' && peek(parser) != ':') {
                    fprintf(stderr, "bass:%d:%zu error: unexpected character `%c`\n",
                            parser->line, get_col(parser), peek(parser));
                    return false;
                }
                if (parse_register(string, &num)) {
                    *token = make_token(parser, TOK_REGISTER, string, num);
                } else {
                    if (peek(parser) == ':') {
                        next(parser);
                        *token = make_token(parser, TOK_LABEL, string, -1);
                    } else {
                        OpType op_type = 0;
                        if (get_opcode(string, &op_type)) {
                            *token = make_token(parser, TOK_OPCODE, string, op_type);
                        } else {
                            *token = make_token(parser, TOK_IDENTIFIER, string, -1);
                        }
                    }
                }
            } else if (current == '\0') {
                *token = make_token(parser, TOK_EOF, (StringView){0}, -1);
            } else {
                fprintf(stderr, "bass:%d:%zu error: unexpected character `%c`\n",
                        parser->line, get_col(parser), current);

                if (is_digit(current)) {
                    fprintf(stderr, "help: try prefixing `%c` with `r` for register, `#` "
                                    "for a literal value or `@` for a memory address\n", current);
                }
                return false;
            }
        }
    }
    parser->start = parser->end;
    return true;
}

typedef struct {
    size_t *data;
    size_t size;
    size_t capacity;
} JumpIndexes;


// TODO: add searching label by hashing the name
int find_label(Labels *labels, StringView name) {
    for (size_t i = 0; i < labels->size; i++) {
        if (string_view_eq(labels->data[i].name, name)) return i;
    }
    return -1;
}

bool parse(Parser *parser, OpCodes *opcodes, Labels *labels) {
    JumpIndexes saved_jumps = {0};
    int op_index = 0;
    while (true) {
        Token tok = {0};
        if (!next_token(parser, &tok)) {
            return false;
        }

        switch (tok.type) {
            case TOK_LABEL: {
                Label label = {tok.str, op_index};
                // patching jumps
                for (size_t i = 0; i < saved_jumps.size;) {
                    size_t jump_index = saved_jumps.data[i];
                    if (string_view_eq(opcodes->data[jump_index].operands[0].str, label.name)) {
                        opcodes->data[jump_index].operands[0].as_int = label.index;
                        dyn_swap_remove(&saved_jumps, i);
                    } else {
                        i++;
                    }
                }
                if (find_label(labels, label.name) >= 0) {
                    fprintf(stderr, "bass:%d:%zu: error: label `%.*s` declared multiple times\n",
                            tok.line, tok.col, SV_FORMAT(tok.str));
                    return false;
                }
                dyn_append(labels, label);
            } break;
            case TOK_OPCODE: {
                OpCode opcode = {0};
                if (!parse_opcode(parser, tok, &opcode)) {
                    return false;
                }
                // patching jumps
                if ((opcode.op == OP_JUMP || opcode.op == OP_JUMPZ || opcode.op == OP_JUMPG
                    || opcode.op == OP_JUMPL || opcode.op == OP_CALL)
                    && opcode.operands[0].type == TOK_IDENTIFIER) {
                    int label_index = find_label(labels, opcode.operands[0].str);
                    if (label_index >= 0) {
                        opcode.operands[0].as_int = labels->data[label_index].index;
                    } else {
                        dyn_append(&saved_jumps, op_index);
                    }
                }
                dyn_append(opcodes, opcode);
                op_index++;
            } break;
            case TOK_EOF: {
                if (saved_jumps.size != 0) {
                    for (size_t i = 0; i < saved_jumps.size; i++) {
                        Operand label_name = opcodes->data[saved_jumps.data[i]].operands[0];
                        fprintf(stderr, "bass:%d:%zu: error: jump to an undeclared label, `%.*s`\n",
                                label_name.line, label_name.col, SV_FORMAT(label_name.str));
                    }
                    return false;
                }
                return true;
            } break;
            default: {
                fprintf(stderr, "bass:%d:%zu: error: expected opcode or label, got %s, `%.*s`\n",
                        tok.line, tok.col, TOKEN_STRING[tok.type], SV_FORMAT(tok.str));
                fprintf(stderr, "help: opcode `%s` takes %d argument(s)\n",
                        OPCODES[opcodes->data[opcodes->size-1].op].name, OPCODES[opcodes->data[opcodes->size-1].op].arity);
                return false;
            } break;
        }
    }
}

void display_opcode(OpCode op) {
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
                (val == '\n')? printf("\tVALUE: \\n\n"): printf("\tVALUE: %c\n", val);
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


void display_labels(Labels lbls) {
    for (size_t i = 0; i < lbls.size; i++) {
        Label t = lbls.data[i];
        printf("Label: %.*s (opcode: %zu)\n", SV_FORMAT(t.name), t.index);
    }
}

