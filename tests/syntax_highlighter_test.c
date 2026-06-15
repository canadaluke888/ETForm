#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/syntax_highlighter.h"

typedef struct {
    SyntaxTokenKind kind;
    char text[64];
} Token;

typedef struct {
    Token items[64];
    int count;
} TokenList;

static void collect_token(
    SyntaxTokenKind kind,
    const char *text,
    size_t bytes,
    void *user_data
) {
    TokenList *list = user_data;
    if (!list || list->count >= 64) return;
    Token *token = &list->items[list->count++];
    token->kind = kind;
    snprintf(token->text, sizeof(token->text), "%.*s", (int)bytes, text);
}

static int has_token(
    const TokenList *list,
    SyntaxTokenKind kind,
    const char *text
) {
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].kind == kind &&
            strcmp(list->items[i].text, text) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    assert(syntax_language_for_name("python") == SYNTAX_LANGUAGE_PYTHON);
    assert(syntax_language_for_name("py") == SYNTAX_LANGUAGE_PYTHON);
    assert(syntax_language_for_name("c++") == SYNTAX_LANGUAGE_CPP);
    assert(syntax_language_for_name("app.tsx") == SYNTAX_LANGUAGE_TYPESCRIPT);
    assert(syntax_language_for_name(NULL) == SYNTAX_LANGUAGE_UNKNOWN);

    TokenList tokens = {0};
    syntax_highlight_line(
        SYNTAX_LANGUAGE_PYTHON,
        "def run(value): return \"ok\" # result",
        collect_token,
        &tokens
    );
    assert(has_token(&tokens, SYNTAX_TOKEN_KEYWORD, "def"));
    assert(has_token(&tokens, SYNTAX_TOKEN_KEYWORD, "return"));
    assert(has_token(&tokens, SYNTAX_TOKEN_STRING, "\"ok\""));
    assert(has_token(&tokens, SYNTAX_TOKEN_COMMENT, "# result"));

    tokens = (TokenList){0};
    syntax_highlight_line(
        SYNTAX_LANGUAGE_PYTHON,
        "print(π)",
        collect_token,
        &tokens
    );
    assert(has_token(&tokens, SYNTAX_TOKEN_PLAIN, "π"));

    puts("syntax highlighter tests passed");
    return 0;
}
