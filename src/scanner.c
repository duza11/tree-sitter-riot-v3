#include "tree_sitter/parser.h"
#include "tree_sitter/array.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
    RAW_TEXT,
    SCRIPT_JAVASCRIPT_TEXT,
    COMPONENT_JAVASCRIPT_TEXT,
    RIOT_METHOD_MODIFIER,
    RIOT_METHOD_NAME,
    RIOT_METHOD_PARAMETERS,
    RIOT_METHOD_BODY,
    RIOT_EXPRESSION_TEXT,
    RIOT_EACH_SHORTHAND_EXPRESSION_TEXT,
    RIOT_EACH_COLLECTION_EXPRESSION,
    RIOT_CLASS_IDENTIFIER_NAME,
    RIOT_CLASS_STRING_NAME,
    RIOT_CLASS_CONDITION,
    SCSS_STYLE_TAG_NAME,
    CSS_STYLE_TAG_NAME,
    SCRIPT_START_TAG_NAME,
    START_TAG_NAME,
    VOID_START_TAG_NAME,
    END_TAG_NAME,
    ERRONEOUS_END_TAG_NAME,
    IMPLICIT_END_TAG,
    SELF_CLOSING_TAG_DELIMITER,
};

typedef Array(char) TagName;
typedef Array(TagName) TagStack;

typedef struct {
    TagStack tags;
} Scanner;

static void clear_tags(Scanner *scanner) {
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        array_delete(&scanner->tags.contents[i]);
    }
    array_clear(&scanner->tags);
}

static void push_tag(Scanner *scanner, const char *name) {
    TagName tag = array_new();
    unsigned length = (unsigned)strlen(name);
    array_reserve(&tag, length);

    for (unsigned i = 0; i < length; i++) {
        array_push(&tag, name[i]);
    }

    array_push(&scanner->tags, tag);
}

static void pop_tag(Scanner *scanner) {
    if (scanner->tags.size == 0) {
        return;
    }

    TagName tag = array_pop(&scanner->tags);
    array_delete(&tag);
}

static bool tag_name_equals(const TagName *tag, const char *name) {
    unsigned length = (unsigned)strlen(name);
    return tag->size == length && memcmp(tag->contents, name, length) == 0;
}

void *tree_sitter_riot_v3_external_scanner_create(void) {
    Scanner *scanner = ts_calloc(1, sizeof(Scanner));
    scanner->tags = (TagStack)array_new();
    return scanner;
}

void tree_sitter_riot_v3_external_scanner_destroy(void *payload) {
    Scanner *scanner = payload;
    clear_tags(scanner);
    array_delete(&scanner->tags);
    ts_free(scanner);
}

void tree_sitter_riot_v3_external_scanner_reset(void *payload) {
    clear_tags(payload);
}

unsigned tree_sitter_riot_v3_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = payload;
    uint16_t tag_count = scanner->tags.size > UINT16_MAX ? UINT16_MAX : (uint16_t)scanner->tags.size;
    uint16_t serialized_count = 0;
    unsigned required_size = sizeof(tag_count) + sizeof(serialized_count);

    for (unsigned i = tag_count; i > 0; i--) {
        unsigned length = scanner->tags.contents[i - 1].size;
        if (length > UINT8_MAX) {
            length = UINT8_MAX;
        }
        if (required_size + 1 + length > TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
            break;
        }
        required_size += 1 + length;
        serialized_count++;
    }

    unsigned size = 0;
    memcpy(&buffer[size], &tag_count, sizeof(tag_count));
    size += sizeof(tag_count);
    memcpy(&buffer[size], &serialized_count, sizeof(serialized_count));
    size += sizeof(serialized_count);

    unsigned start = tag_count - serialized_count;
    for (unsigned i = start; i < tag_count; i++) {
        TagName *tag = &scanner->tags.contents[i];
        uint8_t length = tag->size > UINT8_MAX ? UINT8_MAX : (uint8_t)tag->size;
        buffer[size++] = (char)length;
        memcpy(&buffer[size], tag->contents, length);
        size += length;
    }

    return size;
}

void tree_sitter_riot_v3_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
    Scanner *scanner = payload;
    clear_tags(scanner);

    if (length < sizeof(uint16_t) * 2) {
        return;
    }

    unsigned size = 0;
    uint16_t tag_count = 0;
    uint16_t serialized_count = 0;
    memcpy(&tag_count, &buffer[size], sizeof(tag_count));
    size += sizeof(tag_count);
    memcpy(&serialized_count, &buffer[size], sizeof(serialized_count));
    size += sizeof(serialized_count);

    if (serialized_count > tag_count) {
        serialized_count = tag_count;
    }

    for (unsigned i = 0; i < tag_count - serialized_count; i++) {
        TagName placeholder = array_new();
        array_push(&scanner->tags, placeholder);
    }

    for (unsigned i = 0; i < serialized_count && size < length; i++) {
        uint8_t name_length = (uint8_t)buffer[size++];
        if (size + name_length > length) {
            break;
        }

        TagName tag = array_new();
        array_reserve(&tag, name_length);
        for (unsigned j = 0; j < name_length; j++) {
            array_push(&tag, buffer[size++]);
        }
        array_push(&scanner->tags, tag);
    }
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
    return
        (c >= '\t' && c <= '\r') ||
        c == ' ' ||
        c == 0x00A0 ||
        c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200A) ||
        c == 0x2028 ||
        c == 0x2029 ||
        c == 0x202F ||
        c == 0x205F ||
        c == 0x3000 ||
        c == 0xFEFF;
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

static inline bool is_riot_class_name_start(int c) {
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        c == '_' ||
        (c >= 0x00A0 && c <= 0x00FF);
}

static inline bool is_riot_class_name_continue(int c) {
    return
        is_riot_class_name_start(c) ||
        (c >= '0' && c <= '9') ||
        c == '-';
}

static inline void advance_js(TSLexer *lexer, bool mark_end) {
    lexer->advance(lexer, false);
    if (mark_end) {
        lexer->mark_end(lexer);
    }
}

static inline void consume(TSLexer *lexer) {
    advance_js(lexer, true);
}

static bool scan_js_word_allows_regex(TSLexer *lexer, bool mark_end) {
    char word[16];
    unsigned length = 0;

    do {
        if (length + 1 < sizeof(word)) {
            word[length++] = (char)lexer->lookahead;
        }
        advance_js(lexer, mark_end);
    } while (is_identifier_continue(lexer->lookahead));
    word[length] = '\0';

    return
        strcmp(word, "await") == 0 ||
        strcmp(word, "case") == 0 ||
        strcmp(word, "delete") == 0 ||
        strcmp(word, "do") == 0 ||
        strcmp(word, "else") == 0 ||
        strcmp(word, "in") == 0 ||
        strcmp(word, "instanceof") == 0 ||
        strcmp(word, "new") == 0 ||
        strcmp(word, "of") == 0 ||
        strcmp(word, "return") == 0 ||
        strcmp(word, "throw") == 0 ||
        strcmp(word, "typeof") == 0 ||
        strcmp(word, "void") == 0 ||
        strcmp(word, "yield") == 0;
}

static void scan_js_string_with_mark(
    TSLexer *lexer,
    int quote,
    bool mark_end
) {
    advance_js(lexer, mark_end);

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            advance_js(lexer, mark_end);
            if (!lexer->eof(lexer)) {
                advance_js(lexer, mark_end);
            }
            continue;
        }

        int current = lexer->lookahead;
        advance_js(lexer, mark_end);
        if (current == quote) {
            return;
        }
    }
}

static void scan_js_string(TSLexer *lexer, int quote) {
    scan_js_string_with_mark(lexer, quote, true);
}

static void scan_js_line_comment_with_mark(TSLexer *lexer, bool mark_end) {
    while (!lexer->eof(lexer) && lexer->lookahead != '\n' && lexer->lookahead != '\r') {
        advance_js(lexer, mark_end);
    }
}

static void scan_js_line_comment(TSLexer *lexer) {
    scan_js_line_comment_with_mark(lexer, true);
}

static void scan_js_block_comment_with_mark(TSLexer *lexer, bool mark_end) {
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '*') {
            advance_js(lexer, mark_end);
            if (lexer->lookahead == '/') {
                advance_js(lexer, mark_end);
                return;
            }
            continue;
        }

        advance_js(lexer, mark_end);
    }
}

static void scan_js_block_comment(TSLexer *lexer) {
    scan_js_block_comment_with_mark(lexer, true);
}

static void scan_js_regex_with_mark(TSLexer *lexer, bool mark_end) {
    bool in_character_class = false;

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            advance_js(lexer, mark_end);
            if (!lexer->eof(lexer)) {
                advance_js(lexer, mark_end);
            }
            continue;
        }

        if (lexer->lookahead == '[') {
            in_character_class = true;
            advance_js(lexer, mark_end);
            continue;
        }

        if (lexer->lookahead == ']' && in_character_class) {
            in_character_class = false;
            advance_js(lexer, mark_end);
            continue;
        }

        if (lexer->lookahead == '/' && !in_character_class) {
            advance_js(lexer, mark_end);
            while (is_identifier_continue(lexer->lookahead)) {
                advance_js(lexer, mark_end);
            }
            return;
        }

        if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            return;
        }

        advance_js(lexer, mark_end);
    }
}

static void scan_js_regex(TSLexer *lexer) {
    scan_js_regex_with_mark(lexer, true);
}

static void scan_js_template_with_mark(TSLexer *lexer, bool mark_end);

static void scan_js_template_interpolation_with_mark(
    TSLexer *lexer,
    bool mark_end
) {
    unsigned depth = 1;
    bool can_start_regex = true;

    while (!lexer->eof(lexer) && depth > 0) {
        int current = lexer->lookahead;

        if (current == '\'' || current == '"') {
            scan_js_string_with_mark(lexer, current, mark_end);
            can_start_regex = false;
            continue;
        }

        if (current == '`') {
            scan_js_template_with_mark(lexer, mark_end);
            can_start_regex = false;
            continue;
        }

        if (current == '/') {
            advance_js(lexer, mark_end);
            if (lexer->lookahead == '/') {
                advance_js(lexer, mark_end);
                scan_js_line_comment_with_mark(lexer, mark_end);
            } else if (lexer->lookahead == '*') {
                advance_js(lexer, mark_end);
                scan_js_block_comment_with_mark(lexer, mark_end);
            } else if (can_start_regex) {
                scan_js_regex_with_mark(lexer, mark_end);
                can_start_regex = false;
            } else {
                can_start_regex = true;
            }
            continue;
        }

        if (current == '{') {
            depth++;
            advance_js(lexer, mark_end);
            can_start_regex = true;
            continue;
        }

        if (current == '}') {
            depth--;
            advance_js(lexer, mark_end);
            can_start_regex = false;
            continue;
        }

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, mark_end);
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                advance_js(lexer, mark_end);
            }
            can_start_regex = false;
            continue;
        }

        advance_js(lexer, mark_end);
        if (!is_space(current)) {
            can_start_regex = current != ')' && current != ']';
        }
    }
}

static void scan_js_template_with_mark(TSLexer *lexer, bool mark_end) {
    advance_js(lexer, mark_end);

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '\\') {
            advance_js(lexer, mark_end);
            if (!lexer->eof(lexer)) {
                advance_js(lexer, mark_end);
            }
            continue;
        }

        if (lexer->lookahead == '`') {
            advance_js(lexer, mark_end);
            return;
        }

        if (lexer->lookahead == '$') {
            advance_js(lexer, mark_end);
            if (lexer->lookahead == '{') {
                advance_js(lexer, mark_end);
                scan_js_template_interpolation_with_mark(lexer, mark_end);
            }
            continue;
        }

        advance_js(lexer, mark_end);
    }
}

static void scan_js_template(TSLexer *lexer) {
    scan_js_template_with_mark(lexer, true);
}

static bool scan_riot_expression_text(
    TSLexer *lexer,
    const bool *valid_symbols,
    bool has_content,
    bool can_start_regex
) {
    unsigned brace_depth = 0;
    unsigned parenthesis_depth = 0;
    unsigned bracket_depth = 0;
    unsigned ternary_depth = 0;
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
            }
            consume(lexer);
            has_content = true;
            can_start_regex = true;
            continue;
        }

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, true);
            has_content = true;
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            has_content = true;
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        if (!is_space(current)) {
            has_content = true;
            can_start_regex = current != ')' && current != ']';
        }
    }

    if (!has_content) {
        return false;
    }

    if (valid_symbols[RIOT_EACH_COLLECTION_EXPRESSION]) {
        lexer->result_symbol = RIOT_EACH_COLLECTION_EXPRESSION;
        return true;
    }

    if (valid_symbols[RIOT_EACH_SHORTHAND_EXPRESSION_TEXT]) {
        lexer->result_symbol = RIOT_EACH_SHORTHAND_EXPRESSION_TEXT;
        return true;
    }

    if (valid_symbols[RIOT_EXPRESSION_TEXT]) {
        lexer->result_symbol = RIOT_EXPRESSION_TEXT;
        return true;
    }

    return false;
}

static bool skip_each_spaces(TSLexer *lexer) {
    bool skipped = false;

    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        lexer->advance(lexer, false);
        skipped = true;
    }

    return skipped;
}

static bool scan_each_identifier(TSLexer *lexer) {
    if (!is_identifier_start(lexer->lookahead)) {
        return false;
    }

    do {
        lexer->advance(lexer, false);
    } while (is_identifier_continue(lexer->lookahead));

    return true;
}

static bool scan_riot_each_shorthand_expression(
    TSLexer *lexer,
    const bool *valid_symbols
) {
    skip_each_spaces(lexer);

    if (!scan_each_identifier(lexer)) {
        return scan_riot_expression_text(lexer, valid_symbols, false, true);
    }

    bool has_space = skip_each_spaces(lexer);

    if (lexer->lookahead == ',') {
        lexer->advance(lexer, false);
        skip_each_spaces(lexer);

        if (!scan_each_identifier(lexer)) {
            lexer->mark_end(lexer);
            return scan_riot_expression_text(lexer, valid_symbols, true, false);
        }

        has_space = skip_each_spaces(lexer);
    }

    if (has_space && lexer->lookahead == 'i') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == 'n') {
            lexer->advance(lexer, false);
            if (skip_each_spaces(lexer)) {
                return false;
            }
        }
    }

    lexer->mark_end(lexer);
    return scan_riot_expression_text(lexer, valid_symbols, true, false);
}

static bool scan_riot_class_condition(TSLexer *lexer) {
    unsigned brace_depth = 0;
    unsigned parenthesis_depth = 0;
    unsigned bracket_depth = 0;
    bool has_content = false;
    bool can_start_regex = true;

    while (!lexer->eof(lexer)) {
        int current = lexer->lookahead;

        if (
            brace_depth == 0 &&
            parenthesis_depth == 0 &&
            bracket_depth == 0 &&
            (current == ',' || current == '}')
        ) {
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
            if (brace_depth == 0) {
                break;
            }
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

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, true);
            has_content = true;
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            has_content = true;
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        if (!is_space(current)) {
            has_content = true;
            can_start_regex = current != ')' && current != ']';
        }
    }

    if (!has_content) {
        return false;
    }

    lexer->result_symbol = RIOT_CLASS_CONDITION;
    return true;
}

static bool scan_riot_class_name(TSLexer *lexer, const bool *valid_symbols) {
    bool is_string = false;
    bool has_content = false;

    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        lexer->advance(lexer, true);
    }

    if (valid_symbols[RIOT_CLASS_STRING_NAME] &&
        (lexer->lookahead == '"' || lexer->lookahead == '\'')
    ) {
        scan_js_string(lexer, lexer->lookahead);
        is_string = true;
        has_content = true;
    } else if (valid_symbols[RIOT_CLASS_IDENTIFIER_NAME]) {
        if (lexer->lookahead == '-') {
            lexer->advance(lexer, false);
            has_content = true;
        }

        if (!is_riot_class_name_start(lexer->lookahead)) {
            if (has_content && valid_symbols[RIOT_EXPRESSION_TEXT]) {
                return scan_riot_expression_text(lexer, valid_symbols, true, false);
            }
            return false;
        }

        lexer->advance(lexer, false);
        has_content = true;
        while (is_riot_class_name_continue(lexer->lookahead)) {
            lexer->advance(lexer, false);
        }
    } else {
        return false;
    }

    lexer->mark_end(lexer);
    skip_each_spaces(lexer);

    if (lexer->lookahead != ':') {
        if (valid_symbols[RIOT_EXPRESSION_TEXT]) {
            return scan_riot_expression_text(lexer, valid_symbols, has_content, false);
        }
        return false;
    }

    lexer->result_symbol = is_string
        ? RIOT_CLASS_STRING_NAME
        : RIOT_CLASS_IDENTIFIER_NAME;
    return true;
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

static bool scan_expected_end_tag(Scanner *scanner, TSLexer *lexer) {
    if (lexer->lookahead != '<') {
        return false;
    }

    lexer->advance(lexer, false);

    if (scanner->tags.size == 0) {
        return false;
    }

    TagName *expected = array_back(&scanner->tags);
    if (expected->size == 0) {
        return false;
    }

    if (lexer->lookahead != '/') {
        return false;
    }
    lexer->advance(lexer, false);

    for (unsigned i = 0; i < expected->size; i++) {
        if (lower(lexer->lookahead) != expected->contents[i]) {
            return false;
        }
        lexer->advance(lexer, false);
    }

    return !is_name_char(lexer->lookahead);
}

typedef enum {
    NO_RIOT_METHOD_TOKEN,
    RIOT_METHOD_MODIFIER_TOKEN,
    RIOT_METHOD_NAME_TOKEN,
} RiotMethodToken;

static inline void advance_lookahead(TSLexer *lexer) {
    advance_js(lexer, false);
}

static void skip_js_string_lookahead(TSLexer *lexer, int quote) {
    scan_js_string_with_mark(lexer, quote, false);
}

static void skip_js_line_comment_lookahead(TSLexer *lexer) {
    scan_js_line_comment_with_mark(lexer, false);
}

static void skip_js_block_comment_lookahead(TSLexer *lexer) {
    scan_js_block_comment_with_mark(lexer, false);
}

static void skip_js_regex_lookahead(TSLexer *lexer) {
    scan_js_regex_with_mark(lexer, false);
}

static void skip_js_template_lookahead(TSLexer *lexer) {
    scan_js_template_with_mark(lexer, false);
}

static void skip_js_spaces_lookahead(TSLexer *lexer) {
    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        advance_lookahead(lexer);
    }
}

static bool scan_identifier_lookahead(
    TSLexer *lexer,
    char *name,
    unsigned capacity,
    bool mark_token
) {
    if (!is_identifier_start(lexer->lookahead)) {
        return false;
    }

    unsigned length = 0;
    do {
        if (length + 1 < capacity) {
            name[length++] = (char)lexer->lookahead;
        }
        advance_lookahead(lexer);
        if (mark_token) {
            lexer->mark_end(lexer);
        }
    } while (is_identifier_continue(lexer->lookahead));
    name[length] = '\0';
    return true;
}

static bool is_riot_method_keyword(const char *name) {
    return
        strcmp(name, "if") == 0 ||
        strcmp(name, "while") == 0 ||
        strcmp(name, "for") == 0 ||
        strcmp(name, "switch") == 0 ||
        strcmp(name, "catch") == 0 ||
        strcmp(name, "function") == 0;
}

static bool scan_riot_method_parameters_lookahead(TSLexer *lexer) {
    bool can_start_regex = true;

    if (lexer->lookahead != '(') {
        return false;
    }
    advance_lookahead(lexer);

    while (!lexer->eof(lexer) && lexer->lookahead != ')') {
        int current = lexer->lookahead;

        if (current == '(') {
            return false;
        }

        if (current == '\'' || current == '"') {
            skip_js_string_lookahead(lexer, current);
            can_start_regex = false;
            continue;
        }

        if (current == '`') {
            skip_js_template_lookahead(lexer);
            can_start_regex = false;
            continue;
        }

        if (current == '/') {
            advance_lookahead(lexer);
            if (lexer->lookahead == '/') {
                advance_lookahead(lexer);
                skip_js_line_comment_lookahead(lexer);
            } else if (lexer->lookahead == '*') {
                advance_lookahead(lexer);
                skip_js_block_comment_lookahead(lexer);
            } else if (can_start_regex) {
                skip_js_regex_lookahead(lexer);
                can_start_regex = false;
            } else {
                can_start_regex = true;
            }
            continue;
        }

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, false);
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                advance_lookahead(lexer);
            }
            can_start_regex = false;
            continue;
        }

        advance_lookahead(lexer);
        if (!is_space(current)) {
            can_start_regex = current != ']';
        }
    }

    if (lexer->lookahead != ')') {
        return false;
    }
    advance_lookahead(lexer);
    skip_js_spaces_lookahead(lexer);
    return lexer->lookahead == '{';
}

// Mirrors the Riot v3 compiler's JS_ES6SIGN method detection.
static RiotMethodToken scan_riot_method_candidate(
    TSLexer *lexer,
    bool allow_modifier,
    bool mark_token
) {
    char name[16];

    if (lexer->lookahead == '*') {
        if (!allow_modifier) {
            return NO_RIOT_METHOD_TOKEN;
        }

        advance_lookahead(lexer);
        if (mark_token) {
            lexer->mark_end(lexer);
        }
        skip_js_spaces_lookahead(lexer);
        if (!scan_identifier_lookahead(lexer, name, sizeof(name), false) ||
            is_riot_method_keyword(name)
        ) {
            return NO_RIOT_METHOD_TOKEN;
        }
        skip_js_spaces_lookahead(lexer);
        return scan_riot_method_parameters_lookahead(lexer)
            ? RIOT_METHOD_MODIFIER_TOKEN
            : NO_RIOT_METHOD_TOKEN;
    }

    if (!scan_identifier_lookahead(lexer, name, sizeof(name), mark_token)) {
        return NO_RIOT_METHOD_TOKEN;
    }

    bool is_async = strcmp(name, "async") == 0;
    skip_js_spaces_lookahead(lexer);

    if (allow_modifier && is_async && is_identifier_start(lexer->lookahead)) {
        char method_name[16];
        if (!scan_identifier_lookahead(
                lexer,
                method_name,
                sizeof(method_name),
                false
            ) ||
            is_riot_method_keyword(method_name)
        ) {
            return NO_RIOT_METHOD_TOKEN;
        }
        skip_js_spaces_lookahead(lexer);
        return scan_riot_method_parameters_lookahead(lexer)
            ? RIOT_METHOD_MODIFIER_TOKEN
            : NO_RIOT_METHOD_TOKEN;
    }

    if (is_riot_method_keyword(name)) {
        return NO_RIOT_METHOD_TOKEN;
    }

    return scan_riot_method_parameters_lookahead(lexer)
        ? RIOT_METHOD_NAME_TOKEN
        : NO_RIOT_METHOD_TOKEN;
}

static bool emit_riot_method_token(
    TSLexer *lexer,
    const bool *valid_symbols,
    bool allow_modifier
) {
    while (
        allow_modifier
            ? lexer->lookahead == ' ' || lexer->lookahead == '\t'
            : is_space(lexer->lookahead)
    ) {
        lexer->advance(lexer, true);
    }

    RiotMethodToken token = scan_riot_method_candidate(
        lexer,
        allow_modifier,
        true
    );
    if (token == RIOT_METHOD_MODIFIER_TOKEN &&
        valid_symbols[RIOT_METHOD_MODIFIER]
    ) {
        lexer->result_symbol = RIOT_METHOD_MODIFIER;
        return true;
    }
    if (token == RIOT_METHOD_NAME_TOKEN && valid_symbols[RIOT_METHOD_NAME]) {
        lexer->result_symbol = RIOT_METHOD_NAME;
        return true;
    }
    return false;
}

static bool scan_riot_method_parameters(TSLexer *lexer) {
    bool has_content = false;
    bool can_start_regex = true;

    while (!lexer->eof(lexer) && lexer->lookahead != ')') {
        int current = lexer->lookahead;

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

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, true);
            has_content = true;
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            has_content = true;
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        if (!is_space(current)) {
            has_content = true;
            can_start_regex = current != ']';
        }
    }

    if (!has_content) {
        return false;
    }

    lexer->result_symbol = RIOT_METHOD_PARAMETERS;
    return true;
}

static bool scan_riot_method_body(TSLexer *lexer) {
    unsigned brace_depth = 0;
    bool has_content = false;
    bool can_start_regex = true;

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

        if (is_identifier_start(current)) {
            can_start_regex = scan_js_word_allows_regex(lexer, true);
            has_content = true;
            continue;
        }

        if (current >= '0' && current <= '9') {
            while (is_identifier_continue(lexer->lookahead)) {
                consume(lexer);
            }
            has_content = true;
            can_start_regex = false;
            continue;
        }

        consume(lexer);
        if (!is_space(current)) {
            has_content = true;
            can_start_regex = current != ')' && current != ']';
        }
    }

    if (!has_content) {
        return false;
    }

    lexer->result_symbol = RIOT_METHOD_BODY;
    return true;
}

static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
    bool has_content = false;

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '<') {
            lexer->mark_end(lexer);

            if (scan_expected_end_tag(scanner, lexer)) {
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

static bool scan_script_javascript_text(
    Scanner *scanner,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    bool has_content = false;
    bool at_line_start = lexer->get_column(lexer) == 0;

    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            at_line_start = true;
        }
        lexer->advance(lexer, true);
    }

    while (!lexer->eof(lexer)) {
        if (at_line_start) {
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                lexer->advance(lexer, !has_content);
            }

            if (
                is_identifier_start(lexer->lookahead) ||
                lexer->lookahead == '*'
            ) {
                RiotMethodToken method = scan_riot_method_candidate(
                    lexer,
                    valid_symbols[RIOT_METHOD_MODIFIER],
                    !has_content
                );
                if (method != NO_RIOT_METHOD_TOKEN) {
                    if (has_content) {
                        lexer->result_symbol = SCRIPT_JAVASCRIPT_TEXT;
                        return true;
                    }
                    if (method == RIOT_METHOD_MODIFIER_TOKEN &&
                        valid_symbols[RIOT_METHOD_MODIFIER]
                    ) {
                        lexer->result_symbol = RIOT_METHOD_MODIFIER;
                        return true;
                    }
                    if (method == RIOT_METHOD_NAME_TOKEN &&
                        valid_symbols[RIOT_METHOD_NAME]
                    ) {
                        lexer->result_symbol = RIOT_METHOD_NAME;
                        return true;
                    }
                    return false;
                }

                lexer->mark_end(lexer);
                has_content = true;
                at_line_start = lexer->get_column(lexer) == 0;
            }
        }

        if (lexer->lookahead == '<') {
            lexer->mark_end(lexer);
            if (scan_expected_end_tag(scanner, lexer)) {
                if (!has_content) {
                    return false;
                }
                lexer->result_symbol = SCRIPT_JAVASCRIPT_TEXT;
                return true;
            }

            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = false;
            continue;
        }

        int current = lexer->lookahead;
        if (current == '\'' || current == '"') {
            scan_js_string(lexer, current);
            has_content = true;
            at_line_start = false;
            continue;
        }
        if (current == '`') {
            scan_js_template(lexer);
            has_content = true;
            at_line_start = false;
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
            }
            at_line_start = false;
            continue;
        }

        consume(lexer);
        has_content = true;
        if (current == '\n' || current == '\r') {
            at_line_start = true;
        } else if (!is_space(current)) {
            at_line_start = false;
        }
    }

    if (!has_content) {
        return false;
    }

    lexer->result_symbol = SCRIPT_JAVASCRIPT_TEXT;
    return true;
}

static bool scan_component_javascript_text(
    TSLexer *lexer,
    const bool *valid_symbols
) {
    bool has_content = false;
    bool has_non_space = false;
    bool at_line_start = lexer->get_column(lexer) == 0;

    while (!lexer->eof(lexer) && is_space(lexer->lookahead)) {
        if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            at_line_start = true;
        }
        lexer->advance(lexer, true);
    }

    while (!lexer->eof(lexer)) {
        if (at_line_start) {
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                lexer->advance(lexer, !has_non_space);
            }

            if (
                is_identifier_start(lexer->lookahead) ||
                lexer->lookahead == '*'
            ) {
                RiotMethodToken method = scan_riot_method_candidate(
                    lexer,
                    valid_symbols[RIOT_METHOD_MODIFIER],
                    !has_non_space
                );
                if (method != NO_RIOT_METHOD_TOKEN) {
                    if (has_non_space) {
                        lexer->result_symbol = COMPONENT_JAVASCRIPT_TEXT;
                        return true;
                    }
                    if (method == RIOT_METHOD_MODIFIER_TOKEN &&
                        valid_symbols[RIOT_METHOD_MODIFIER]
                    ) {
                        lexer->result_symbol = RIOT_METHOD_MODIFIER;
                        return true;
                    }
                    if (method == RIOT_METHOD_NAME_TOKEN &&
                        valid_symbols[RIOT_METHOD_NAME]
                    ) {
                        lexer->result_symbol = RIOT_METHOD_NAME;
                        return true;
                    }
                    return false;
                }

                lexer->mark_end(lexer);
                has_content = true;
                has_non_space = true;
                at_line_start = lexer->get_column(lexer) == 0;
            }
        }

        if (lexer->lookahead == '<') {
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);

            if (lexer->lookahead == '/') {
                if (!has_non_space) {
                    return false;
                }
                lexer->result_symbol = COMPONENT_JAVASCRIPT_TEXT;
                return true;
            }

            if (has_non_space) {
                char name[8];
                unsigned len = 0;

                while (!lexer->eof(lexer) && is_name_char(lexer->lookahead)) {
                    if (len + 1 < sizeof(name)) {
                        name[len++] = (char)lower(lexer->lookahead);
                    }
                    lexer->advance(lexer, false);
                }

                name[len] = '\0';

                if (
                    strcmp(name, "script") == 0 ||
                    strcmp(name, "style") == 0
                ) {
                    lexer->result_symbol = COMPONENT_JAVASCRIPT_TEXT;
                    return true;
                }
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
            at_line_start = false;
            continue;
        }

        int current = lexer->lookahead;
        if (current == '\'' || current == '"') {
            scan_js_string(lexer, current);
            has_content = true;
            has_non_space = true;
            at_line_start = false;
            continue;
        }
        if (current == '`') {
            scan_js_template(lexer);
            has_content = true;
            has_non_space = true;
            at_line_start = false;
            continue;
        }
        if (current == '/') {
            consume(lexer);
            has_content = true;
            has_non_space = true;
            if (lexer->lookahead == '/') {
                consume(lexer);
                scan_js_line_comment(lexer);
            } else if (lexer->lookahead == '*') {
                consume(lexer);
                scan_js_block_comment(lexer);
            }
            at_line_start = false;
            continue;
        }

        consume(lexer);
        has_content = true;
        if (!is_space(current)) {
            has_non_space = true;
            at_line_start = false;
        } else if (current == '\n' || current == '\r') {
            at_line_start = true;
        }
    }

    if (!has_content || !has_non_space) {
        return false;
    }

    lexer->result_symbol = COMPONENT_JAVASCRIPT_TEXT;
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

static bool scan_start_tag_is_self_closing(TSLexer *lexer) {
    unsigned expression_depth = 0;
    int quote = 0;
    bool quote_allows_escape = false;

    while (!lexer->eof(lexer)) {
        int current = lexer->lookahead;

        if (quote != 0) {
            lexer->advance(lexer, false);

            if (quote_allows_escape && current == '\\' && !lexer->eof(lexer)) {
                lexer->advance(lexer, false);
            } else if (current == quote) {
                quote = 0;
            }
            continue;
        }

        if (current == '\'' || current == '"' || current == '`') {
            quote = current;
            quote_allows_escape = expression_depth > 0;
            lexer->advance(lexer, false);
            continue;
        }

        if (current == '{') {
            expression_depth++;
            lexer->advance(lexer, false);
            continue;
        }

        if (current == '}' && expression_depth > 0) {
            expression_depth--;
            lexer->advance(lexer, false);
            continue;
        }

        if (expression_depth == 0) {
            if (current == '>') {
                return false;
            }

            if (current == '<') {
                return false;
            }

            if (current == '/') {
                lexer->advance(lexer, false);
                return lexer->lookahead == '>';
            }
        }

        lexer->advance(lexer, false);
    }

    return false;
}

static bool scan_start_tag_name(
    Scanner *scanner,
    TSLexer *lexer,
    const bool *valid_symbols
) {
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
            push_tag(scanner, name);
            lexer->result_symbol = SCSS_STYLE_TAG_NAME;
            return true;
        }

        if (!is_scss && valid_symbols[CSS_STYLE_TAG_NAME]) {
            push_tag(scanner, name);
            lexer->result_symbol = CSS_STYLE_TAG_NAME;
            return true;
        }

        if (valid_symbols[START_TAG_NAME]) {
            push_tag(scanner, name);
            lexer->result_symbol = START_TAG_NAME;
            return true;
        }

        return false;
    }

    if (is_script_tag_name(name) && valid_symbols[SCRIPT_START_TAG_NAME]) {
        push_tag(scanner, name);
        lexer->result_symbol = SCRIPT_START_TAG_NAME;
        return true;
    }

    if (is_void_tag_name(name) && valid_symbols[VOID_START_TAG_NAME]) {
        if (
            valid_symbols[START_TAG_NAME] &&
            scan_start_tag_is_self_closing(lexer)
        ) {
            push_tag(scanner, name);
            lexer->result_symbol = START_TAG_NAME;
            return true;
        }

        lexer->result_symbol = VOID_START_TAG_NAME;
        return true;
    }

    if (valid_symbols[START_TAG_NAME]) {
        push_tag(scanner, name);
        lexer->result_symbol = START_TAG_NAME;
        return true;
    }

    return false;
}

static bool scan_end_tag_name(
    Scanner *scanner,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    if (!valid_symbols[END_TAG_NAME] && !valid_symbols[ERRONEOUS_END_TAG_NAME]) {
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

    bool matches =
        scanner->tags.size > 0 &&
        tag_name_equals(array_back(&scanner->tags), name);

    if (matches && valid_symbols[END_TAG_NAME]) {
        pop_tag(scanner);
        lexer->result_symbol = END_TAG_NAME;
        return true;
    }

    if (!matches && valid_symbols[ERRONEOUS_END_TAG_NAME]) {
        lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
        return true;
    }

    return false;
}

static bool scan_implicit_end_tag(Scanner *scanner, TSLexer *lexer) {
    if (scanner->tags.size < 2 || lexer->lookahead != '<') {
        return false;
    }

    lexer->mark_end(lexer);
    lexer->advance(lexer, false);
    if (lexer->lookahead != '/') {
        return false;
    }
    lexer->advance(lexer, false);

    char name[128];
    unsigned len = 0;
    while (!lexer->eof(lexer) && is_name_char(lexer->lookahead)) {
        if (len + 1 < sizeof(name)) {
            name[len++] = (char)lower(lexer->lookahead);
        }
        lexer->advance(lexer, false);
    }
    name[len] = '\0';

    if (len == 0 || tag_name_equals(array_back(&scanner->tags), name)) {
        return false;
    }

    for (unsigned i = scanner->tags.size - 1; i > 0; i--) {
        if (tag_name_equals(&scanner->tags.contents[i - 1], name)) {
            pop_tag(scanner);
            lexer->result_symbol = IMPLICIT_END_TAG;
            return true;
        }
    }

    return false;
}

static bool scan_self_closing_tag_delimiter(Scanner *scanner, TSLexer *lexer) {
    while (is_space(lexer->lookahead)) {
        lexer->advance(lexer, true);
    }

    if (lexer->lookahead != '/') {
        return false;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead != '>') {
        return false;
    }

    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    pop_tag(scanner);
    lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
    return true;
}

bool tree_sitter_riot_v3_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    Scanner *scanner = payload;

    if (scan_start_tag_name(scanner, lexer, valid_symbols)) {
        return true;
    }

    if (scan_end_tag_name(scanner, lexer, valid_symbols)) {
        return true;
    }

    if (valid_symbols[IMPLICIT_END_TAG] && scan_implicit_end_tag(scanner, lexer)) {
        return true;
    }

    if (
        valid_symbols[SELF_CLOSING_TAG_DELIMITER] &&
        scan_self_closing_tag_delimiter(scanner, lexer)
    ) {
        return true;
    }

    if (valid_symbols[RAW_TEXT]) {
        return scan_raw_text(scanner, lexer);
    }

    if (valid_symbols[RIOT_METHOD_PARAMETERS]) {
        return scan_riot_method_parameters(lexer);
    }

    if (valid_symbols[RIOT_METHOD_BODY]) {
        return scan_riot_method_body(lexer);
    }

    if (valid_symbols[SCRIPT_JAVASCRIPT_TEXT]) {
        return scan_script_javascript_text(scanner, lexer, valid_symbols);
    }

    if (valid_symbols[COMPONENT_JAVASCRIPT_TEXT]) {
        return scan_component_javascript_text(lexer, valid_symbols);
    }

    if (
        valid_symbols[RIOT_METHOD_MODIFIER] ||
        valid_symbols[RIOT_METHOD_NAME]
    ) {
        return emit_riot_method_token(
            lexer,
            valid_symbols,
            valid_symbols[RIOT_METHOD_MODIFIER]
        );
    }

    if (
        valid_symbols[RIOT_EACH_SHORTHAND_EXPRESSION_TEXT]
    ) {
        return scan_riot_each_shorthand_expression(lexer, valid_symbols);
    }

    if (
        valid_symbols[RIOT_CLASS_IDENTIFIER_NAME] ||
        valid_symbols[RIOT_CLASS_STRING_NAME]
    ) {
        if (scan_riot_class_name(lexer, valid_symbols)) {
            return true;
        }
    }

    if (valid_symbols[RIOT_CLASS_CONDITION]) {
        return scan_riot_class_condition(lexer);
    }

    if (
        valid_symbols[RIOT_EXPRESSION_TEXT] ||
        valid_symbols[RIOT_EACH_COLLECTION_EXPRESSION]
    ) {
        return scan_riot_expression_text(lexer, valid_symbols, false, true);
    }

    return false;
}
