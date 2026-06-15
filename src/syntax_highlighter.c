#include "syntax_highlighter.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

static void emit_token(
    SyntaxEmit emit,
    SyntaxTokenKind kind,
    const char *text,
    size_t bytes,
    void *user_data
) {
    if (emit && bytes > 0) {
        emit(kind, text, bytes, user_data);
    }
}

static const char *base_name(const char *path) {
    const char *slash = path ? strrchr(path, '/') : NULL;
    return slash ? slash + 1 : path;
}

static const char *path_extension(const char *path) {
    const char *base = base_name(path);
    const char *dot = base ? strrchr(base, '.') : NULL;
    return dot ? dot : "";
}

static int ext_is(const char *ext, const char *value) {
    return ext && value && strcasecmp(ext, value) == 0;
}

SyntaxLanguage syntax_language_for_path(const char *path) {
    const char *base = base_name(path);
    const char *ext = path_extension(path);
    if (!path || !*path) {
        return SYNTAX_LANGUAGE_UNKNOWN;
    }
    if (base && (strcasecmp(base, "makefile") == 0 || strcasecmp(base, "bashrc") == 0 ||
        strcasecmp(base, ".bashrc") == 0 || strcasecmp(base, ".zshrc") == 0)) {
        return SYNTAX_LANGUAGE_SHELL;
    }
    if (ext_is(ext, ".c") || ext_is(ext, ".h")) return SYNTAX_LANGUAGE_C;
    if (ext_is(ext, ".cc") || ext_is(ext, ".cpp") || ext_is(ext, ".cxx") || ext_is(ext, ".hpp") ||
        ext_is(ext, ".hh") || ext_is(ext, ".hxx")) return SYNTAX_LANGUAGE_CPP;
    if (ext_is(ext, ".py") || ext_is(ext, ".pyw")) return SYNTAX_LANGUAGE_PYTHON;
    if (ext_is(ext, ".js") || ext_is(ext, ".jsx") || ext_is(ext, ".mjs") || ext_is(ext, ".cjs")) {
        return SYNTAX_LANGUAGE_JAVASCRIPT;
    }
    if (ext_is(ext, ".ts") || ext_is(ext, ".tsx")) return SYNTAX_LANGUAGE_TYPESCRIPT;
    if (ext_is(ext, ".json") || ext_is(ext, ".jsonc")) return SYNTAX_LANGUAGE_JSON;
    if (ext_is(ext, ".md") || ext_is(ext, ".markdown")) return SYNTAX_LANGUAGE_MARKDOWN;
    if (ext_is(ext, ".sh") || ext_is(ext, ".bash") || ext_is(ext, ".zsh") || ext_is(ext, ".fish")) {
        return SYNTAX_LANGUAGE_SHELL;
    }
    if (ext_is(ext, ".go")) return SYNTAX_LANGUAGE_GO;
    if (ext_is(ext, ".rs")) return SYNTAX_LANGUAGE_RUST;
    if (ext_is(ext, ".java")) return SYNTAX_LANGUAGE_JAVA;
    if (ext_is(ext, ".css") || ext_is(ext, ".scss") || ext_is(ext, ".sass")) return SYNTAX_LANGUAGE_CSS;
    if (ext_is(ext, ".html") || ext_is(ext, ".htm") || ext_is(ext, ".xml") || ext_is(ext, ".svg")) {
        return SYNTAX_LANGUAGE_HTML;
    }
    if (ext_is(ext, ".yml") || ext_is(ext, ".yaml")) return SYNTAX_LANGUAGE_YAML;
    if (ext_is(ext, ".toml")) return SYNTAX_LANGUAGE_TOML;
    if (ext_is(ext, ".rb")) return SYNTAX_LANGUAGE_RUBY;
    if (ext_is(ext, ".php")) return SYNTAX_LANGUAGE_PHP;
    if (ext_is(ext, ".sql")) return SYNTAX_LANGUAGE_SQL;
    return SYNTAX_LANGUAGE_UNKNOWN;
}

SyntaxLanguage syntax_language_for_name(const char *name) {
    if (!name || !*name) return SYNTAX_LANGUAGE_UNKNOWN;
    if (strcasecmp(name, "c") == 0) return SYNTAX_LANGUAGE_C;
    if (strcasecmp(name, "c++") == 0 || strcasecmp(name, "cpp") == 0 ||
        strcasecmp(name, "cxx") == 0) return SYNTAX_LANGUAGE_CPP;
    if (strcasecmp(name, "python") == 0 || strcasecmp(name, "py") == 0) {
        return SYNTAX_LANGUAGE_PYTHON;
    }
    if (strcasecmp(name, "javascript") == 0 || strcasecmp(name, "js") == 0 ||
        strcasecmp(name, "jsx") == 0) return SYNTAX_LANGUAGE_JAVASCRIPT;
    if (strcasecmp(name, "typescript") == 0 || strcasecmp(name, "ts") == 0 ||
        strcasecmp(name, "tsx") == 0) return SYNTAX_LANGUAGE_TYPESCRIPT;
    if (strcasecmp(name, "json") == 0 || strcasecmp(name, "jsonc") == 0) {
        return SYNTAX_LANGUAGE_JSON;
    }
    if (strcasecmp(name, "markdown") == 0 || strcasecmp(name, "md") == 0) {
        return SYNTAX_LANGUAGE_MARKDOWN;
    }
    if (strcasecmp(name, "shell") == 0 || strcasecmp(name, "sh") == 0 ||
        strcasecmp(name, "bash") == 0 || strcasecmp(name, "zsh") == 0) {
        return SYNTAX_LANGUAGE_SHELL;
    }
    if (strcasecmp(name, "go") == 0 || strcasecmp(name, "golang") == 0) {
        return SYNTAX_LANGUAGE_GO;
    }
    if (strcasecmp(name, "rust") == 0 || strcasecmp(name, "rs") == 0) {
        return SYNTAX_LANGUAGE_RUST;
    }
    if (strcasecmp(name, "java") == 0) return SYNTAX_LANGUAGE_JAVA;
    if (strcasecmp(name, "css") == 0 || strcasecmp(name, "scss") == 0 ||
        strcasecmp(name, "sass") == 0) return SYNTAX_LANGUAGE_CSS;
    if (strcasecmp(name, "html") == 0 || strcasecmp(name, "xml") == 0 ||
        strcasecmp(name, "svg") == 0) return SYNTAX_LANGUAGE_HTML;
    if (strcasecmp(name, "yaml") == 0 || strcasecmp(name, "yml") == 0) {
        return SYNTAX_LANGUAGE_YAML;
    }
    if (strcasecmp(name, "toml") == 0) return SYNTAX_LANGUAGE_TOML;
    if (strcasecmp(name, "ruby") == 0 || strcasecmp(name, "rb") == 0) {
        return SYNTAX_LANGUAGE_RUBY;
    }
    if (strcasecmp(name, "php") == 0) return SYNTAX_LANGUAGE_PHP;
    if (strcasecmp(name, "sql") == 0) return SYNTAX_LANGUAGE_SQL;
    return syntax_language_for_path(name);
}

static int word_in_list(const char *word, size_t len, const char *const *items) {
    for (int i = 0; items[i]; i++) {
        if (strlen(items[i]) == len && strncmp(word, items[i], len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_ident_start(unsigned char ch) {
    return isalpha(ch) || ch == '_';
}

static int is_ident(unsigned char ch) {
    return isalnum(ch) || ch == '_';
}

static int is_operator_char(unsigned char ch) {
    return strchr("{}[]()<>+-=*/%!&|^~?:;,.@", ch) != NULL;
}

static size_t utf8_char_bytes(const char *text) {
    unsigned char first = (unsigned char)text[0];
    size_t bytes = 1;
    if (first >= 0xC2 && first <= 0xDF) {
        bytes = 2;
    } else if (first >= 0xE0 && first <= 0xEF) {
        bytes = 3;
    } else if (first >= 0xF0 && first <= 0xF4) {
        bytes = 4;
    }
    for (size_t i = 1; i < bytes; i++) {
        unsigned char next = (unsigned char)text[i];
        if (next == '\0' || (next & 0xC0) != 0x80) return 1;
    }
    return bytes;
}

static int is_hash_comment_language(SyntaxLanguage language) {
    return language == SYNTAX_LANGUAGE_PYTHON ||
        language == SYNTAX_LANGUAGE_SHELL ||
        language == SYNTAX_LANGUAGE_RUBY ||
        language == SYNTAX_LANGUAGE_YAML ||
        language == SYNTAX_LANGUAGE_TOML;
}

static int is_slash_comment_language(SyntaxLanguage language) {
    return language == SYNTAX_LANGUAGE_C ||
        language == SYNTAX_LANGUAGE_CPP ||
        language == SYNTAX_LANGUAGE_JAVASCRIPT ||
        language == SYNTAX_LANGUAGE_TYPESCRIPT ||
        language == SYNTAX_LANGUAGE_GO ||
        language == SYNTAX_LANGUAGE_RUST ||
        language == SYNTAX_LANGUAGE_JAVA ||
        language == SYNTAX_LANGUAGE_CSS ||
        language == SYNTAX_LANGUAGE_PHP ||
        language == SYNTAX_LANGUAGE_JSON;
}

static int is_c_like(SyntaxLanguage language) {
    return language == SYNTAX_LANGUAGE_C || language == SYNTAX_LANGUAGE_CPP;
}

static SyntaxTokenKind classify_word(SyntaxLanguage language, const char *word, size_t len) {
    static const char *const c_keywords[] = {
        "auto", "break", "case", "const", "continue", "default", "do", "else", "enum", "extern", "for",
        "goto", "if", "inline", "register", "restrict", "return", "sizeof", "static", "struct", "switch",
        "typedef", "union", "volatile", "while", NULL
    };
    static const char *const c_types[] = {
        "bool", "char", "double", "float", "int", "long", "short", "signed", "size_t", "ssize_t", "uint8_t",
        "uint16_t", "uint32_t", "uint64_t", "unsigned", "void", NULL
    };
    static const char *const js_keywords[] = {
        "async", "await", "break", "case", "catch", "class", "const", "continue", "default", "delete", "do",
        "else", "export", "extends", "finally", "for", "from", "function", "if", "import", "in", "instanceof",
        "let", "new", "of", "return", "switch", "throw", "try", "typeof", "var", "while", "yield", NULL
    };
    static const char *const js_types[] = {
        "boolean", "false", "null", "number", "string", "true", "undefined", "unknown", "void", NULL
    };
    static const char *const py_keywords[] = {
        "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del", "elif", "else",
        "except", "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
        "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL
    };
    static const char *const py_types[] = {"False", "None", "True", "bool", "dict", "float", "int", "list", "str", "tuple", NULL};
    static const char *const go_keywords[] = {
        "break", "case", "chan", "const", "continue", "defer", "default", "else", "fallthrough", "for", "func",
        "go", "goto", "if", "import", "interface", "map", "package", "range", "return", "select", "struct",
        "switch", "type", "var", NULL
    };
    static const char *const go_types[] = {
        "bool", "byte", "complex64", "complex128", "error", "false", "float32", "float64", "int", "int8", "int16",
        "int32", "int64", "rune", "string", "true", "uint", "uint8", "uint16", "uint32", "uint64", "uintptr", NULL
    };
    static const char *const rust_keywords[] = {
        "as", "async", "await", "break", "const", "continue", "crate", "else", "enum", "extern", "false", "fn",
        "for", "if", "impl", "in", "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return", "self",
        "static", "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while", NULL
    };
    static const char *const rust_types[] = {
        "bool", "char", "f32", "f64", "i8", "i16", "i32", "i64", "i128", "isize", "str", "String", "u8", "u16",
        "u32", "u64", "u128", "usize", NULL
    };
    static const char *const java_keywords[] = {
        "abstract", "break", "case", "catch", "class", "const", "continue", "default", "do", "else", "enum",
        "extends", "final", "finally", "for", "if", "implements", "import", "instanceof", "interface", "new",
        "package", "private", "protected", "public", "return", "static", "super", "switch", "this", "throw",
        "throws", "try", "while", NULL
    };
    static const char *const java_types[] = {
        "boolean", "byte", "char", "double", "false", "float", "int", "long", "null", "short", "String", "true",
        "void", NULL
    };
    static const char *const shell_keywords[] = {
        "case", "do", "done", "elif", "else", "esac", "fi", "for", "function", "if", "in", "select", "then",
        "until", "while", NULL
    };
    static const char *const ruby_keywords[] = {
        "begin", "break", "case", "class", "def", "do", "else", "elsif", "end", "ensure", "false", "for", "if",
        "in", "module", "nil", "not", "raise", "rescue", "return", "self", "super", "then", "true", "unless",
        "until", "when", "while", "yield", NULL
    };
    static const char *const php_keywords[] = {
        "abstract", "array", "as", "break", "case", "catch", "class", "const", "continue", "default", "echo",
        "else", "elseif", "extends", "final", "finally", "for", "foreach", "function", "if", "implements",
        "interface", "namespace", "new", "private", "protected", "public", "return", "static", "switch", "throw",
        "trait", "try", "use", "var", "while", NULL
    };
    static const char *const sql_keywords[] = {
        "and", "as", "by", "case", "create", "delete", "desc", "distinct", "drop", "else", "from", "group",
        "having", "in", "insert", "into", "is", "join", "left", "like", "limit", "not", "null", "on", "or",
        "order", "right", "select", "set", "table", "then", "union", "update", "values", "when", "where", NULL
    };
    static const char *const sql_types[] = {
        "bigint", "boolean", "char", "date", "decimal", "float", "int", "integer", "numeric", "real", "text",
        "timestamp", "varchar", NULL
    };

    if (language == SYNTAX_LANGUAGE_C || language == SYNTAX_LANGUAGE_CPP) {
        if (word_in_list(word, len, c_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, c_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_JAVASCRIPT ||
        language == SYNTAX_LANGUAGE_TYPESCRIPT) {
        if (word_in_list(word, len, js_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, js_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_PYTHON) {
        if (word_in_list(word, len, py_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, py_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_GO) {
        if (word_in_list(word, len, go_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, go_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_RUST) {
        if (word_in_list(word, len, rust_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, rust_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_JAVA) {
        if (word_in_list(word, len, java_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, java_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_SHELL) {
        if (word_in_list(word, len, shell_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_RUBY) {
        if (word_in_list(word, len, ruby_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_PHP) {
        if (word_in_list(word, len, php_keywords)) return SYNTAX_TOKEN_KEYWORD;
    } else if (language == SYNTAX_LANGUAGE_SQL) {
        if (word_in_list(word, len, sql_types)) return SYNTAX_TOKEN_TYPE;
        if (word_in_list(word, len, sql_keywords)) return SYNTAX_TOKEN_KEYWORD;
    }
    return SYNTAX_TOKEN_PLAIN;
}

static size_t scan_string(const char *line, size_t i) {
    char quote = line[i];
    size_t j = i + 1;
    while (line[j]) {
        if (line[j] == '\\' && line[j + 1]) {
            j += 2;
            continue;
        }
        if (line[j] == quote) {
            return j + 1;
        }
        j++;
    }
    return j;
}

static int is_key_after_string(SyntaxLanguage language, const char *line, size_t end) {
    if (language != SYNTAX_LANGUAGE_JSON &&
        language != SYNTAX_LANGUAGE_YAML &&
        language != SYNTAX_LANGUAGE_TOML) {
        return 0;
    }
    while (line[end] && isspace((unsigned char)line[end])) {
        end++;
    }
    return line[end] == ':' || line[end] == '=';
}

static int yaml_or_toml_bare_key(SyntaxLanguage language, const char *line, size_t i, size_t *end) {
    size_t j = i;
    if (language != SYNTAX_LANGUAGE_YAML && language != SYNTAX_LANGUAGE_TOML &&
        language != SYNTAX_LANGUAGE_CSS) {
        return 0;
    }
    if (!is_ident_start((unsigned char)line[j])) {
        return 0;
    }
    while (line[j] && (is_ident((unsigned char)line[j]) || line[j] == '-' || line[j] == '.')) {
        j++;
    }
    size_t k = j;
    while (line[k] && isspace((unsigned char)line[k])) {
        k++;
    }
    if (line[k] == ':' || line[k] == '=') {
        *end = j;
        return 1;
    }
    return 0;
}

static void highlight_markup_line(SyntaxLanguage language, const char *line, SyntaxEmit emit, void *user_data) {
    size_t i = 0;
    while (line[i] && isspace((unsigned char)line[i])) {
        i++;
    }
    if (language == SYNTAX_LANGUAGE_MARKDOWN && line[i] == '#') {
        emit_token(emit, SYNTAX_TOKEN_PREPROCESSOR, line, strlen(line), user_data);
        return;
    }

    i = 0;
    while (line[i]) {
        if (language == SYNTAX_LANGUAGE_HTML && strncmp(line + i, "<!--", 4) == 0) {
            const char *end = strstr(line + i + 4, "-->");
            size_t bytes = end ? (size_t)(end - (line + i)) + 3 : strlen(line + i);
            emit_token(emit, SYNTAX_TOKEN_COMMENT, line + i, bytes, user_data);
            i += bytes;
        } else if (language == SYNTAX_LANGUAGE_HTML && line[i] == '<') {
            size_t j = i + 1;
            while (line[j] && line[j] != '>') {
                j++;
            }
            if (line[j] == '>') {
                j++;
            }
            emit_token(emit, SYNTAX_TOKEN_PREPROCESSOR, line + i, j - i, user_data);
            i = j;
        } else if (language == SYNTAX_LANGUAGE_MARKDOWN && line[i] == '`') {
            size_t j = scan_string(line, i);
            emit_token(emit, SYNTAX_TOKEN_STRING, line + i, j - i, user_data);
            i = j;
        } else if (language == SYNTAX_LANGUAGE_MARKDOWN && (line[i] == '[' || line[i] == ']' ||
            line[i] == '(' || line[i] == ')')) {
            emit_token(emit, SYNTAX_TOKEN_OPERATOR, line + i, 1, user_data);
            i++;
        } else {
            size_t bytes = utf8_char_bytes(line + i);
            emit_token(emit, SYNTAX_TOKEN_PLAIN, line + i, bytes, user_data);
            i += bytes;
        }
    }
}

void syntax_highlight_line(
    SyntaxLanguage language,
    const char *line,
    SyntaxEmit emit,
    void *user_data
) {
    size_t i = 0;
    size_t first = 0;
    if (!line || !emit) {
        return;
    }
    if (language == SYNTAX_LANGUAGE_UNKNOWN) {
        emit_token(emit, SYNTAX_TOKEN_PLAIN, line, strlen(line), user_data);
        return;
    }
    if (language == SYNTAX_LANGUAGE_HTML || language == SYNTAX_LANGUAGE_MARKDOWN) {
        highlight_markup_line(language, line, emit, user_data);
        return;
    }

    while (line[first] && isspace((unsigned char)line[first])) {
        first++;
    }
    if (is_c_like(language) && line[first] == '#') {
        emit_token(emit, SYNTAX_TOKEN_PREPROCESSOR, line, strlen(line), user_data);
        return;
    }
    if (language == SYNTAX_LANGUAGE_PHP && strncmp(line + first, "<?php", 5) == 0) {
        emit_token(emit, SYNTAX_TOKEN_PREPROCESSOR, line, first + 5, user_data);
        i = first + 5;
    }

    while (line[i]) {
        if (is_hash_comment_language(language) && line[i] == '#') {
            emit_token(emit, SYNTAX_TOKEN_COMMENT, line + i, strlen(line + i), user_data);
            return;
        }
        if (is_slash_comment_language(language) && line[i] == '/' && line[i + 1] == '/') {
            emit_token(emit, SYNTAX_TOKEN_COMMENT, line + i, strlen(line + i), user_data);
            return;
        }
        if (is_slash_comment_language(language) && line[i] == '/' && line[i + 1] == '*') {
            const char *end = strstr(line + i + 2, "*/");
            size_t bytes = end ? (size_t)(end - (line + i)) + 2 : strlen(line + i);
            emit_token(emit, SYNTAX_TOKEN_COMMENT, line + i, bytes, user_data);
            i += bytes;
            continue;
        }
        if (language == SYNTAX_LANGUAGE_SQL && line[i] == '-' && line[i + 1] == '-') {
            emit_token(emit, SYNTAX_TOKEN_COMMENT, line + i, strlen(line + i), user_data);
            return;
        }
        if (line[i] == '\'' || line[i] == '"' ||
            ((language == SYNTAX_LANGUAGE_JAVASCRIPT || language == SYNTAX_LANGUAGE_TYPESCRIPT ||
            language == SYNTAX_LANGUAGE_SHELL) && line[i] == '`')) {
            size_t end = scan_string(line, i);
            SyntaxTokenKind kind = is_key_after_string(language, line, end) ?
                SYNTAX_TOKEN_PREPROCESSOR : SYNTAX_TOKEN_STRING;
            emit_token(emit, kind, line + i, end - i, user_data);
            i = end;
            continue;
        }
        if (isdigit((unsigned char)line[i])) {
            size_t j = i + 1;
            while (line[j] && (isalnum((unsigned char)line[j]) || line[j] == '.' || line[j] == '_')) {
                j++;
            }
            emit_token(emit, SYNTAX_TOKEN_NUMBER, line + i, j - i, user_data);
            i = j;
            continue;
        }
        if (is_ident_start((unsigned char)line[i])) {
            size_t end = 0;
            size_t j = i + 1;
            if (yaml_or_toml_bare_key(language, line, i, &end)) {
                emit_token(emit, SYNTAX_TOKEN_PREPROCESSOR, line + i, end - i, user_data);
                i = end;
                continue;
            }
            while (line[j] && is_ident((unsigned char)line[j])) {
                j++;
            }
            emit_token(emit, classify_word(language, line + i, j - i), line + i, j - i, user_data);
            i = j;
            continue;
        }
        if (is_operator_char((unsigned char)line[i])) {
            emit_token(emit, SYNTAX_TOKEN_OPERATOR, line + i, 1, user_data);
            i++;
            continue;
        }
        size_t bytes = utf8_char_bytes(line + i);
        emit_token(emit, SYNTAX_TOKEN_PLAIN, line + i, bytes, user_data);
        i += bytes;
    }
}
