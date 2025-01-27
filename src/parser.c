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
        fprintf(stderr, "bass:%d:%zu: expected number\n", parser->line,
                parser->start);
        return false;
    }

    // TODO: strtol: check for errors
    *num = strtol(&parser->source.data[parser->start + skip], NULL, 0);
    return true;
}

// TODO: Reset the parser->start
static inline StringView parse_identifier(Parser *parser) {
    while (isalnum(peek(parser)) || peek(parser) == '_') {
        next(parser);
    }
    return get_string(parser);
}

// parses character or string literals delimited by `quote`
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

bool parse_register(Parser *parser, long *num, StringView *string) {
    if (!parse_num(parser, num, string)) {
        return false;
    }
    if (*num < 0 || REG_COUNT <= *num) {
        fprintf(stderr,
                "bass:%d:%zu: invalid register `%ld`\n"
                "help: registers can range from 0 to %d\n", 
                parser->line, get_col(parser), *num, REG_COUNT - 1);
        return false;
    }
    return true;
}


bool parse_register_from_identifier(StringView identifier, long *num) {
    if (identifier.length != 2) return false;
    if (identifier.data[0] != 'r') return false;
    if (!('0' <= identifier.data[1] && identifier.data[1] < REG_COUNT + '0')) {
        // TODO: this help text is probably useless
        // fprintf(stderr,
        //         "bass:%d:%zu: invalid register `%d`\n"
        //         "help: registers can range from 0 to %d\n",
        //         parser->line, get_col_start(parser), identifier.data[1] - '0', REG_COUNT - 1);
        return false;
    }
    *num = identifier.data[1] - '0';
    return true;
}

// TODO: the function assignes a value of long to an int (num is long, Operand
// has int member variable)
bool parse_operands(Parser *parser, OpType op, Operand operands[MAX_OPERANDS]) {
    int i = 0;
    while (i < OPCODES[op].arity) {
        char current = next(parser);
        long num;
        StringView string;
        switch (current) {
        case 'r': {
            if (!parse_register(parser, &num, &string)) {
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
            // parsing as memory address
            if (isdigit(peek(parser))) {
                if (!parse_num(parser, &num, &string)) {
                    return false;
                }
                operands[i++] = (Operand){TOK_ADDRESS, string, num};

                // parsing as address at register
            } else if (peek(parser) == 'r') {
                next(parser);
                if (!parse_register(parser, &num, &string)) {
                    return false;
                }
                operands[i++] = (Operand){TOK_ADDRESS_REG, string, num};
            } else {
                if (peek(parser) == '\n') {
                    fprintf(
                        stderr,
                        "bass: expected register or value after `@` got `\\n` "
                        "at: %d:%zu\n",
                        parser->line, get_col(parser) + 2);

                } else {
                    fprintf(
                        stderr,
                        "bass: expected register or value after `@` got `%c` "
                        "at: %d:%zu\n",
                        peek(parser), parser->line, get_col(parser) + 2);
                }
                return false;
            }
        } break;
        case '\n': {
            parser->line_start = parser->end;
            parser->line++;
        } break;

        default: {
            if (!isspace(current)) {
                if (current == '\0') {
                    fprintf(
                        stderr,
                        "bass: expected register, value or memory address but "
                        "got EOF after: %d:%zu\n",
                        parser->line, get_col(parser));
                } else {
                    fprintf(
                        stderr,
                        "bass: expected register, value or memory address but "
                        "got `%c` at: %d:%zu\n",
                        current, parser->line, get_col(parser));
                }
                if (isdigit(current)) {
                    fprintf(
                        stderr,
                        "help: try prefixing `%c` with `r` for register, `#` "
                        "for a literal value or `@` for a memory address\n",
                        current);
                } else {
                    fprintf(stderr,
                            "help: opcode `%s` takes %d arguments but got "
                            "%d instead\n",
                            OPCODES[op].name, OPCODES[op].arity, i);
                }
                return false;
            }
        }
        }
        parser->start = parser->end;
    }
    return true;
}

bool parse_jump(Parser *parser, Operand *operand) {
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
        if (!parse_quoted_char(parser, &string, '\'', "character")) {
            return false;
        }
        if (string.length == 0) {
            fprintf(stderr, "bass: empty character literal at %zu\n",
                    parser->end);
            return false;
        }

        // newline escape character
        if (string.data[0] == '\\' && string.length == 2 &&
            string.data[1] == 'n') {
            *operand = (Operand){TOK_LITERAL_CHAR, string, '\n'};
            return true;
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
        if (!parse_quoted_char(parser, &string, '\"', "string")) {
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
    size_t col = parser->start - parser->line_start + 1;
    if (!get_opcode(string, &op_type)) {
        fprintf(stderr, "bass: invalid opcode `%.*s` at: %d:%zu\n",
                SV_FORMAT(string), parser->line, col);
        return false;
    }
    parser->start = parser->end;
    Operand operands[MAX_OPERANDS] = {0};
    if (op_type == OP_JUMP || op_type == OP_JUMPZ || op_type == OP_JUMPG ||
        op_type == OP_JUMPL || op_type == OP_CALL) {
        if (!parse_jump(parser, &operands[0])) {
            return false;
        }
    } else if (op_type == OP_PRINT || op_type == OP_PRINTLN) {
        if (!parse_print(parser, operands)) {
            return false;
        }
    } else {
        if (!parse_operands(parser, op_type, operands)) {
            return false;
        }
    }
    opcode->op = op_type;
    opcode->line = parser->line;
    opcode->col = col;
    memcpy(&opcode->operands, operands, sizeof(Operand) * MAX_OPERANDS);
    return true;
}

#define MAKE_TOKEN(parser, type, string, value) \
    ((Token){(parser)->line, get_col_start((parser)), (type), (string), {(value)}})


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
                        *token = MAKE_TOKEN(parser, TOK_LABEL, string, 0);
                    } else {
                        OpType op_type = 0;
                        if (!get_opcode(string, &op_type)) {
                            *token = MAKE_TOKEN(parser, TOK_IDENTIFIER, string, 0);
                        } else {
                            *token = MAKE_TOKEN(parser, TOK_OPCODE, string, op_type);
                        }
                    }
                }
            } else {
                (current == '\0')
                    ? fprintf(stderr, "bass:%d:%zu unexpected EOF\n", parser->line, get_col_start(parser))
                    : fprintf(stderr, "bass:%d:%zu unexpected character `%c`\n", parser->line, get_col_start(parser), current);

                if (isdigit(current)) {
                    fprintf(
                        stderr,
                        "help: try prefixing `%c` with `r` for register, `#` "
                        "for a literal value or `@` for a memory address\n",
                        current);
                }
                return false;
            }
            // TODO: mention this help text when parsing specific opcodes instead
            // fprintf(stderr, "help: opcode `%s` takes %d arguments but got %d instead\n", OPCODES[op].name, OPCODES[op].arity, i);
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
        printf("%d:%zu: %s `%.*s`\n", tok.line, tok.col, TOKEN_STRING[tok.type], SV_FORMAT(tok.str));
        continue;

        if (tok.type == TOK_LABEL) {
            dyn_append(labels, ((Label){tok.str, op_index}));
        } else if (tok.type == TOK_OPCODE) {
            TODO("parse opcodes");
            op_index++;
        } else { 
            fprintf(stderr,
                    "bass:%d:%zu: error: expected opcode or label, got %s, `%.*s`\n",
                    parser->line, get_col(parser), TOKEN_STRING[tok.type], SV_FORMAT(tok.str));
            return false;
        }
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
            opcode.op == OP_JUMPG || opcode.op == OP_JUMPL ||
            opcode.op == OP_CALL) {
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
            case TOK_ADDRESS_REG:
                printf("\tADDRESS AT REGISTER: %d\n", val);
                break;
            case TOK_IDENTIFIER: {
                StringView str = op.operands[i].string;
                printf("\tLABEL: %.*s (to opcode: %d)\n", SV_FORMAT(str), val);
            } break;
            case TOK_LABEL:
            case TOK_OPCODE:
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
