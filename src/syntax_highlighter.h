#ifndef SYNTAX_HIGHLIGHTER_H
#define SYNTAX_HIGHLIGHTER_H

#include <stddef.h>

typedef enum {
    SYNTAX_LANGUAGE_UNKNOWN = 0,
    SYNTAX_LANGUAGE_C,
    SYNTAX_LANGUAGE_CPP,
    SYNTAX_LANGUAGE_PYTHON,
    SYNTAX_LANGUAGE_JAVASCRIPT,
    SYNTAX_LANGUAGE_TYPESCRIPT,
    SYNTAX_LANGUAGE_JSON,
    SYNTAX_LANGUAGE_MARKDOWN,
    SYNTAX_LANGUAGE_SHELL,
    SYNTAX_LANGUAGE_GO,
    SYNTAX_LANGUAGE_RUST,
    SYNTAX_LANGUAGE_JAVA,
    SYNTAX_LANGUAGE_CSS,
    SYNTAX_LANGUAGE_HTML,
    SYNTAX_LANGUAGE_YAML,
    SYNTAX_LANGUAGE_TOML,
    SYNTAX_LANGUAGE_RUBY,
    SYNTAX_LANGUAGE_PHP,
    SYNTAX_LANGUAGE_SQL
} SyntaxLanguage;

typedef enum {
    SYNTAX_TOKEN_PLAIN = 0,
    SYNTAX_TOKEN_COMMENT,
    SYNTAX_TOKEN_STRING,
    SYNTAX_TOKEN_NUMBER,
    SYNTAX_TOKEN_KEYWORD,
    SYNTAX_TOKEN_TYPE,
    SYNTAX_TOKEN_PREPROCESSOR,
    SYNTAX_TOKEN_OPERATOR
} SyntaxTokenKind;

typedef void (*SyntaxEmit)(
    SyntaxTokenKind kind,
    const char *text,
    size_t bytes,
    void *user_data
);

SyntaxLanguage syntax_language_for_path(const char *path);
SyntaxLanguage syntax_language_for_name(const char *name);
void syntax_highlight_line(
    SyntaxLanguage language,
    const char *line,
    SyntaxEmit emit,
    void *user_data
);

#endif
