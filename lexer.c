// lexer.c
#include <string.h>
#include <ctype.h>
#include "lexer.h"

#define MAX_LINE 256

/* =====================================
   LEXER STATE
   'static' keeps these private to this file
   ===================================== */
static const char *src = NULL;   // the source code text
static int pos  = 0;             // current position in the text
static int line = 1;             // current line number
static int col  = 1;             // current column number

/* =====================================
   HELPERS
   ===================================== */

// Look at the current character without consuming it
static char peek(void)
{
    return src[pos];
}

// Look one character ahead (safe: returns '\0' at the end)
static char peekNext(void)
{
    if (src[pos] == '\0') return '\0';
    return src[pos + 1];
}

// Consume the current character and update line/column
static char advance(void)
{
    char c = src[pos++];
    if (c == '\n') { line++; col = 1; }
    else           { col++; }
    return c;
}

// Can this character be part of a word?
static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

// Build a token and return it
static Token make_token(TokenType type, const char *text, int tline, int tcol)
{
    Token t;
    t.type = type;
    strncpy(t.lexeme, text, sizeof(t.lexeme) - 1);
    t.lexeme[sizeof(t.lexeme) - 1] = '\0';   // lexeme holds only 100 chars
    t.line   = tline;
    t.column = tcol;
    return t;
}

/* =====================================
   PUBLIC FUNCTIONS
   ===================================== */

// Start (or restart) scanning a new source text
void initLexer(const char *source)
{
    src  = source;
    pos  = 0;
    line = 1;
    col  = 1;
}

// Look the word up in the keyword table (defined in main.c).
// Returns the keyword's token type, or TOKEN_IDENTIFIER if not found.
TokenType checkKeyword(const char *word)
{
    for (int i = 0; i < keywordCount; i++) {
        if (strcmp(word, keywords[i].word) == 0)
            return keywords[i].type;
    }
    return TOKEN_IDENTIFIER;
}

// Skip spaces, tabs, newlines, and # comments
void skipWhitespace(void)
{
    for (;;) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        }
        else if (c == '#') {
            // Comment: skip until the end of the line
            while (peek() != '\n' && peek() != '\0')
                advance();
        }
        else {
            break;   // real code starts here
        }
    }
}

// Scan a word: yes/no, keyword, or identifier
Token scanWord(void)
{
    char buf[MAX_LINE];
    int  len = 0;

    int startLine = line;
    int startCol  = col;

    while (is_ident_char(peek())) {
        char c = advance();
        if (len < MAX_LINE - 1) buf[len++] = c;
    }
    buf[len] = '\0';

    if (strcmp(buf, "yes") == 0 || strcmp(buf, "no") == 0) {
        if (strcmp(buf, "yes") == 0) {
            return make_token(TOKEN_YES, buf, startLine, startCol);
        } else {
            return make_token(TOKEN_NO, buf, startLine, startCol);
        }
    }
    else if (checkKeyword(buf) == TOKEN_KEYWORD) {
        return make_token(TOKEN_KEYWORD, buf, startLine, startCol);
    }
    else {
        return make_token(TOKEN_IDENTIFIER, buf, startLine, startCol);
    }
}

// Scan a number: 85 is an INTEGER, 3.14 is a FLOAT
Token scanNumber(void)
{
    char buf[MAX_LINE];
    int  len = 0;
    int  isFloat = 0;

    int startLine = line;
    int startCol  = col;

    // Whole-number part
    while (isdigit((unsigned char)peek())) {
        char c = advance();
        if (len < MAX_LINE - 1) buf[len++] = c;
    }

    // Decimal part: a '.' followed by at least one digit
    if (peek() == '.' && isdigit((unsigned char)peekNext())) {
        isFloat = 1;
        char c = advance();                       // the '.'
        if (len < MAX_LINE - 1) buf[len++] = c;

        while (isdigit((unsigned char)peek())) {
            c = advance();
            if (len < MAX_LINE - 1) buf[len++] = c;
        }
    }
    buf[len] = '\0';

    return make_token(isFloat ? TOKEN_FLOAT : TOKEN_INTEGER,
                      buf, startLine, startCol);
}

// Scan a character literal like 'A'
Token scanCharacter(void)
{
    char buf[MAX_LINE];
    int  len = 0;

    int startLine = line;
    int startCol  = col;

    buf[len++] = advance();                       // the opening '

    // Read until the closing ' (stop at end of line or end of file)
    while (peek() != '\'' && peek() != '\n' && peek() != '\0') {
        char c = advance();
        if (len < MAX_LINE - 2) buf[len++] = c;
    }

    if (peek() == '\'') {
        buf[len++] = advance();                   // the closing '
        buf[len] = '\0';
        return make_token(TOKEN_CHARACTER, buf, startLine, startCol);
    }

    // No closing quote was found
    buf[len] = '\0';
    return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
}

// Scan punctuation and operators
Token scanSymbol(void)
{
    int  startLine = line;
    int  startCol  = col;
    char c = advance();
    char buf[3] = { c, '\0', '\0' };

    switch (c) {
        case '(': return make_token(TOKEN_LEFT_PAREN,  buf, startLine, startCol);
        case ')': return make_token(TOKEN_RIGHT_PAREN, buf, startLine, startCol);
        case '{': return make_token(TOKEN_LEFT_BRACE,  buf, startLine, startCol);
        case '}': return make_token(TOKEN_RIGHT_BRACE, buf, startLine, startCol);
        case ';': return make_token(TOKEN_SEMICOLON,   buf, startLine, startCol);

        // Operators that can be one or two characters: = == ! != < <= > >=
        case '=': case '!': case '<': case '>':
            if (peek() == '=') buf[1] = advance();
            return make_token(TOKEN_OPERATOR, buf, startLine, startCol);

        // Operators that need two characters: && ||
        case '&': case '|':
            if (peek() == c) {
                buf[1] = advance();
                return make_token(TOKEN_OPERATOR, buf, startLine, startCol);
            }
            return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);

        // Single-character arithmetic operators
        case '+': case '-': case '*': case '/': case '%':
            return make_token(TOKEN_OPERATOR, buf, startLine, startCol);
    }

    // Anything else is not part of Maroon
    return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
}

// Decide which scan function to use, and return the next token
Token getNextToken(void)
{
    skipWhitespace();

    char c = peek();

    if (c == '\0')
        return make_token(TOKEN_EOF, "EOF", line, col);

    if (isalpha((unsigned char)c) || c == '_')
        return scanWord();

    if (isdigit((unsigned char)c))
        return scanNumber();

    if (c == '\'')
        return scanCharacter();

    return scanSymbol();
}