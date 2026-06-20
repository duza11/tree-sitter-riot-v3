#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
    RAW_TEXT,
    COMPONENT_SCRIPT,
    RIOT_EXPRESSION_TEXT,
    RIOT_EACH_EXPRESSION_TEXT,
    RIOT_CLASS_EXPRESSION_TEXT,
    SCSS_STYLE_TAG_NAME,
    CSS_STYLE_TAG_NAME,
    SCRIPT_START_TAG_NAME,
    START_TAG_NAME,
    VOID_START_TAG_NAME,
};

void *tree_sitter_riot_v3_external_scanner_create(void) {
    return NULL;
}

void tree_sitter_riot_v3_external_scanner_destroy(void *payload) {
    (void)payload;
}

void tree_sitter_riot_v3_external_scanner_reset(void *payload) {
    (void)payload;
}

unsigned tree_sitter_riot_v3_external_scanner_serialize(void *payload, char *buffer) {
    (void)payload;
    (void)buffer;
    return 0;
}

void tree_sitter_riot_v3_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
    (void)payload;
    (void)buffer;
    (void)length;
}

static inline int lower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

static inline bool is_name_char(int c) {
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '_' ||
        c == ':' ||
        c == '.' ||
        c == '-';
}

static inline bool is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static inline bool is_identifier_start(int c) {
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        c == '_' ||
        c == '$';
}

static inline bool is_identifier_continue(int c) {
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

static inline void consume(TSLexer *lexer) {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
}

static void scan_js_string(TSLexer *lexer, int quote) {
    consume(lexer);

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            consume(lexer);
            if (!lexer->eof(lexer)) {
                consume(lexer);
            }
            continue;
        }

        int current = lexer->lookahead;
        consume(lexer);
        if (current == quote) {
            return;
        }
    }
}

static void scan_js_line_comment(TSLexer *lexer) {
    while (!lexer->eof(lexer) && lexer->lookahead != '\n' && lexer->lookahead != '\r') {
        consume(lexer);
    }
}

static void scan_js_block_comment(TSLexer *lexer) {
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '*') {
            consume(lexer);
            if (lexer->lookahead == '/') {
                consume(lexer);
                return;
            }
            continue;
        }

        consume(lexer);
    }
}

static void scan_js_regex(TSLexer *lexer) {
    bool in_character_class = false;

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            consume(lexer);
            if (!lexer->eof(lexer)) {
                consume(lexer);
            }
            continue;
        }

        if (lexer->lookahead == '[') {
            in_character_class = true;
            consume(lexer);
            continue;
        }

        if (lexer->lookahead == ']' && in_character_class) {
            in_character_class = false;
            consume(lexer);
            continue;
        }

        if (lexer->lookahead == '/' && !in_character_class) {
            consume(lexer);
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            return;
        }

        if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            return;
        }

        consume(lexer);
    }
}

static void scan_js_template(TSLexer *lexer);

static void scan_js_template_interpolation(TSLexer *lexer) {
    unsigned depth = 1;
    bool can_start_regex = true;

    while (!lexer->eof(lexer) && depth > 0) {
        int current = lexer->lookahead;

        if (current == '\'' || current == '"') {
            scan_js_string(lexer, current);
            can_start_regex = false;
            continue;
        }

        if (current == '`') {
            scan_js_template(lexer);
            can_start_regex = false;
            continue;
        }

        if (current == '/') {
            consume(lexer);
            if (lexer->lookahead == '/') {
                consume(lexer);
                scan_js_line_comment(lexer);
            } else if (lexer->lookahead == '*') {
                consume(lexer);
                scan_js_block_comment(lexer);
            } else if (can_start_regex) {
                scan_js_regex(lexer);
                can_start_regex = false;
            } else {
                can_start_regex = true;
            }
            continue;
        }

        if (current == '{') {
            depth++;
            consume(lexer);
            can_start_regex = true;
            continue;
        }

        if (current == '}') {
            depth--;
            consume(lexer);
            can_start_regex = false;
            continue;
        }

        if (is_identifier_start(current) || (current >= '0' && current <= '9')) {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        if (!is_space(current)) {
            can_start_regex = current != ')' && current != ']';
        }
    }
}

static void scan_js_template(TSLexer *lexer) {
    consume(lexer);

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            consume(lexer);
            if (!lexer->eof(lexer)) {
                consume(lexer);
            }
            continue;
        }

        if (lexer->lookahead == '`') {
            consume(lexer);
            return;
        }

        if (lexer->lookahead == '$') {
            consume(lexer);
            if (lexer->lookahead == '{') {
                consume(lexer);
                scan_js_template_interpolation(lexer);
            }
            continue;
        }

        consume(lexer);
    }
}

static bool scan_riot_expression_text(TSLexer *lexer, const bool *valid_symbols) {
    unsigned brace_depth = 0;
    unsigned parenthesis_depth = 0;
    unsigned bracket_depth = 0;
    unsigned ternary_depth = 0;
    bool has_content = false;
    bool can_start_regex = true;
    bool is_class_shorthand = false;

    while (!lexer->eof(lexer)) {
        int current = lexer->lookahead;

        if (current == '}' && brace_depth == 0) {
            break;
        }

        if (current == '\'' || current == '"') {
            scan_js_string(lexer, current);
            has_content = true;
            can_start_regex = false;
            continue;
        }

        if (current == '`') {
            scan_js_template(lexer);
            has_content = true;
            can_start_regex = false;
            continue;
        }

        if (current == '/') {
            consume(lexer);
            has_content = true;
            if (lexer->lookahead == '/') {
                consume(lexer);
                scan_js_line_comment(lexer);
            } else if (lexer->lookahead == '*') {
                consume(lexer);
                scan_js_block_comment(lexer);
            } else if (can_start_regex) {
                scan_js_regex(lexer);
                can_start_regex = false;
            } else {
                can_start_regex = true;
            }
            continue;
        }

        if (current == '{') {
            brace_depth++;
            consume(lexer);
            has_content = true;
            can_start_regex = true;
            continue;
        }

        if (current == '}') {
            brace_depth--;
            consume(lexer);
            has_content = true;
            can_start_regex = false;
            continue;
        }

        if (current == '(') {
            parenthesis_depth++;
            consume(lexer);
            has_content = true;
            can_start_regex = true;
            continue;
        }

        if (current == ')') {
            if (parenthesis_depth > 0) {
                parenthesis_depth--;
            }
            consume(lexer);
            has_content = true;
            can_start_regex = false;
            continue;
        }

        if (current == '[') {
            bracket_depth++;
            consume(lexer);
            has_content = true;
            can_start_regex = true;
            continue;
        }

        if (current == ']') {
            if (bracket_depth > 0) {
                bracket_depth--;
            }
            consume(lexer);
            has_content = true;
            can_start_regex = false;
            continue;
        }

        bool is_top_level =
            brace_depth == 0 &&
            parenthesis_depth == 0 &&
            bracket_depth == 0;

        if (current == '?') {
            consume(lexer);
            has_content = true;
            can_start_regex = true;

            if (lexer->lookahead == '?') {
                consume(lexer);
            } else if (is_top_level && lexer->lookahead != '.') {
                ternary_depth++;
            }
            continue;
        }

        if (current == ':' && is_top_level) {
            if (ternary_depth > 0) {
                ternary_depth--;
            } else {
                is_class_shorthand = true;
            }
            consume(lexer);
            has_content = true;
            can_start_regex = true;
            continue;
        }

        if (is_identifier_start(current) || (current >= '0' && current <= '9')) {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            has_content = true;
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        has_content = true;
        if (!is_space(current)) {
            can_start_regex = current != ')' && current != ']';
        }
    }

    if (!has_content) {
        return false;
    }

    if (valid_symbols[RIOT_EACH_EXPRESSION_TEXT]) {
        lexer->result_symbol = RIOT_EACH_EXPRESSION_TEXT;
        return true;
    }

    if (valid_symbols[RIOT_CLASS_EXPRESSION_TEXT] && is_class_shorthand) {
        lexer->result_symbol = RIOT_CLASS_EXPRESSION_TEXT;
        return true;
    }

    if (valid_symbols[RIOT_EXPRESSION_TEXT]) {
        lexer->result_symbol = RIOT_EXPRESSION_TEXT;
        return true;
    }

    return false;
}

static bool is_void_tag_name(const char *name) {
    return
        strcmp(name, "area") == 0 ||
        strcmp(name, "base") == 0 ||
        strcmp(name, "br") == 0 ||
        strcmp(name, "col") == 0 ||
        strcmp(name, "embed") == 0 ||
        strcmp(name, "hr") == 0 ||
        strcmp(name, "img") == 0 ||
        strcmp(name, "input") == 0 ||
        strcmp(name, "link") == 0 ||
        strcmp(name, "meta") == 0 ||
        strcmp(name, "param") == 0 ||
        strcmp(name, "source") == 0 ||
        strcmp(name, "track") == 0 ||
        strcmp(name, "wbr") == 0;
}

static bool is_style_tag_name(const char *name) {
    return strcmp(name, "style") == 0;
}

static bool is_script_tag_name(const char *name) {
    return strcmp(name, "script") == 0;
}

static bool scan_remaining(TSLexer *lexer, const char *rest) {
    for (unsigned i = 0; rest[i] != '\0'; i++) {
        if (lower(lexer->lookahead) != rest[i]) {
            return false;
        }
        lexer->advance(lexer, false);
    }

    return true;
}

static bool scan_script_or_style_end_tag(TSLexer *lexer) {
    if (lexer->lookahead != '<') {
        return false;
    }
    lexer->advance(lexer, false);

    if (lexer->lookahead != '/') {
        return false;
    }
    lexer->advance(lexer, false);

    if (lower(lexer->lookahead) != 's') {
        return false;
    }
    lexer->advance(lexer, false);

    if (lower(lexer->lookahead) == 'c') {
        lexer->advance(lexer, false);
        return scan_remaining(lexer, "ript>");
    }

    if (lower(lexer->lookahead) == 't') {
        lexer->advance(lexer, false);
        return scan_remaining(lexer, "yle>");
    }

    return false;
}

static bool scan_raw_text(TSLexer *lexer) {
    bool has_content = false;

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '<') {
            lexer->mark_end(lexer);

            if (scan_script_or_style_end_tag(lexer)) {
                if (!has_content) {
                    return false;
                }
                lexer->result_symbol = RAW_TEXT;
                return true;
            }

            lexer->mark_end(lexer);
            has_content = true;
            continue;
        }

        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        has_content = true;
    }

    if (!has_content) {
        return false;
    }

    lexer->result_symbol = RAW_TEXT;
    return true;
}

static bool scan_component_script(TSLexer *lexer) {
    bool has_content = false;
    bool has_non_space = false;

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '<') {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);

            if (lexer->lookahead == '/') {
                if (!has_non_space) {
                    return false;
                }
                lexer->result_symbol = COMPONENT_SCRIPT;
                return true;
            }

            if (
                (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') ||
                (lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
                lexer->lookahead == '!'
            ) {
                return false;
            }

            has_content = true;
            has_non_space = true;
            continue;
        }

        if (!is_space(lexer->lookahead)) {
            has_non_space = true;
        }
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        has_content = true;
    }

    if (!has_content || !has_non_space) {
        return false;
    }

    lexer->result_symbol = COMPONENT_SCRIPT;
    return true;
}

static void skip_spaces(TSLexer *lexer) {
    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        lexer->advance(lexer, false);
    }
}

static bool scan_attr_name(TSLexer *lexer, char *buffer, unsigned buffer_len) {
    unsigned i = 0;

    if (!is_name_char(lexer->lookahead)) {
        return false;
    }

    while (!lexer->eof(lexer) && is_name_char(lexer->lookahead)) {
        if (i + 1 < buffer_len) {
            buffer[i++] = (char)lower(lexer->lookahead);
        }
        lexer->advance(lexer, false);
    }

    buffer[i] = '\0';
    return i > 0;
}

static bool scan_attr_value_is_scss(TSLexer *lexer) {
    skip_spaces(lexer);

    if (lexer->lookahead != '=') {
        return false;
    }

    lexer->advance(lexer, false);
    skip_spaces(lexer);

    if (lexer->lookahead == '"' || lexer->lookahead == '\'') {
        int quote = lexer->lookahead;
        lexer->advance(lexer, false);

        bool is_scss = lower(lexer->lookahead) == 's';

        if (is_scss) lexer->advance(lexer, false);
        if (is_scss && lower(lexer->lookahead) == 'c') lexer->advance(lexer, false);
        else is_scss = false;
        if (is_scss && lower(lexer->lookahead) == 's') lexer->advance(lexer, false);
        else is_scss = false;
        if (is_scss && lower(lexer->lookahead) == 's') lexer->advance(lexer, false);
        else is_scss = false;

        if (is_scss && lexer->lookahead == quote) {
            lexer->advance(lexer, false);
            return true;
        }

        while (!lexer->eof(lexer) && lexer->lookahead != quote) {
            lexer->advance(lexer, false);
        }

        if (lexer->lookahead == quote) {
            lexer->advance(lexer, false);
        }

        return false;
    }

    bool is_scss = lower(lexer->lookahead) == 's';

    if (is_scss) lexer->advance(lexer, false);
    if (is_scss && lower(lexer->lookahead) == 'c') lexer->advance(lexer, false);
    else is_scss = false;
    if (is_scss && lower(lexer->lookahead) == 's') lexer->advance(lexer, false);
    else is_scss = false;
    if (is_scss && lower(lexer->lookahead) == 's') lexer->advance(lexer, false);
    else is_scss = false;

    if (is_scss && !is_name_char(lexer->lookahead)) {
        return true;
    }

    while (!lexer->eof(lexer) && !is_space(lexer->lookahead) && lexer->lookahead != '>') {
        lexer->advance(lexer, false);
    }

    return false;
}

static bool scan_style_attrs_is_scss(TSLexer *lexer) {
    bool is_scss = false;

    while (!lexer->eof(lexer)) {
        skip_spaces(lexer);

        if (lexer->lookahead == '>') {
            return is_scss;
        }

        if (lexer->lookahead == '/') {
            lexer->advance(lexer, false);
            continue;
        }

        char name[64];
        if (!scan_attr_name(lexer, name, sizeof(name))) {
            lexer->advance(lexer, false);
            continue;
        }

        if (strcmp(name, "type") == 0) {
            if (scan_attr_value_is_scss(lexer)) {
                is_scss = true;
            }
            continue;
        }

        skip_spaces(lexer);

        if (lexer->lookahead != '=') {
            continue;
        }

        lexer->advance(lexer, false);
        skip_spaces(lexer);

        if (lexer->lookahead == '"' || lexer->lookahead == '\'') {
            int quote = lexer->lookahead;
            lexer->advance(lexer, false);

            while (!lexer->eof(lexer) && lexer->lookahead != quote) {
                lexer->advance(lexer, false);
            }

            if (lexer->lookahead == quote) {
                lexer->advance(lexer, false);
            }
        } else {
            while (!lexer->eof(lexer) && !is_space(lexer->lookahead) && lexer->lookahead != '>') {
                lexer->advance(lexer, false);
            }
        }
    }

    return is_scss;
}

static bool scan_tag_name(TSLexer *lexer, const bool *valid_symbols) {
    if (
        !valid_symbols[SCSS_STYLE_TAG_NAME] &&
        !valid_symbols[CSS_STYLE_TAG_NAME] &&
        !valid_symbols[SCRIPT_START_TAG_NAME] &&
        !valid_symbols[START_TAG_NAME] &&
        !valid_symbols[VOID_START_TAG_NAME]
    ) {
        return false;
    }

    if (!is_name_char(lexer->lookahead)) {
        return false;
    }

    char name[128];
    unsigned len = 0;

    while (!lexer->eof(lexer) && is_name_char(lexer->lookahead)) {
        if (len + 1 < sizeof(name)) {
            name[len++] = (char)lower(lexer->lookahead);
        }
        lexer->advance(lexer, false);
    }

    name[len] = '\0';
    lexer->mark_end(lexer);

    if (is_style_tag_name(name)) {
        bool is_scss = scan_style_attrs_is_scss(lexer);

        if (is_scss && valid_symbols[SCSS_STYLE_TAG_NAME]) {
            lexer->result_symbol = SCSS_STYLE_TAG_NAME;
            return true;
        }

        if (!is_scss && valid_symbols[CSS_STYLE_TAG_NAME]) {
            lexer->result_symbol = CSS_STYLE_TAG_NAME;
            return true;
        }

        if (valid_symbols[START_TAG_NAME]) {
            lexer->result_symbol = START_TAG_NAME;
            return true;
        }

        return false;
    }

    if (is_script_tag_name(name) && valid_symbols[SCRIPT_START_TAG_NAME]) {
        lexer->result_symbol = SCRIPT_START_TAG_NAME;
        return true;
    }

    if (is_void_tag_name(name) && valid_symbols[VOID_START_TAG_NAME]) {
        lexer->result_symbol = VOID_START_TAG_NAME;
        return true;
    }

    if (valid_symbols[START_TAG_NAME]) {
        lexer->result_symbol = START_TAG_NAME;
        return true;
    }

    return false;
}

bool tree_sitter_riot_v3_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    (void)payload;

    if (scan_tag_name(lexer, valid_symbols)) {
        return true;
    }

    if (valid_symbols[RAW_TEXT]) {
        return scan_raw_text(lexer);
    }

    if (valid_symbols[COMPONENT_SCRIPT]) {
        return scan_component_script(lexer);
    }

    if (
        valid_symbols[RIOT_EXPRESSION_TEXT] ||
        valid_symbols[RIOT_EACH_EXPRESSION_TEXT] ||
        valid_symbols[RIOT_CLASS_EXPRESSION_TEXT]
    ) {
        return scan_riot_expression_text(lexer, valid_symbols);
    }

    return false;
}
