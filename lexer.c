// lexer.c  (transition-table version)
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

// Consume the current character and update line/column
static char advance(void)
{
    char c = src[pos++];
    if (c == '\n') { line++; col = 1; }
    else           { col++; }
    return c;
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
   TRANSITION TABLE
   One DFA recognizes words, integers and floats.

   States:
     S_START  nothing read yet
     S_IDENT  inside a word                (accepting)
     S_INT    inside a whole number        (accepting)
     S_DOT    read digits then '.'         (NOT accepting, needs a digit next)
     S_FLOAT  inside the decimal part      (accepting)
     S_DEAD   no valid move, stop scanning

   Character classes:
     C_LETTER  a-z A-Z _
     C_DIGIT   0-9
     C_DOT     .
     C_OTHER   anything else (space, ';', '(', end of text, ...)
   ===================================== */
typedef enum { S_START, S_IDENT, S_INT, S_DOT, S_FLOAT, S_DEAD,
               NUM_STATES } State;

typedef enum { C_LETTER, C_DIGIT, C_DOT, C_OTHER,
               NUM_CLASSES } CharClass;

static const State table[NUM_STATES][NUM_CLASSES] = {
    /*             LETTER    DIGIT    DOT     OTHER  */
    /* START */  { S_IDENT,  S_INT,   S_DEAD, S_DEAD },
    /* IDENT */  { S_IDENT,  S_IDENT, S_DEAD, S_DEAD },
    /* INT   */  { S_DEAD,   S_INT,   S_DOT,  S_DEAD },
    /* DOT   */  { S_DEAD,   S_FLOAT, S_DEAD, S_DEAD },
    /* FLOAT */  { S_DEAD,   S_FLOAT, S_DEAD, S_DEAD },
    /* DEAD  */  { S_DEAD,   S_DEAD,  S_DEAD, S_DEAD }
};

// Which column of the table does this character belong to?
static CharClass classify(char c)
{
    if (isalpha((unsigned char)c) || c == '_') return C_LETTER;
    if (isdigit((unsigned char)c))             return C_DIGIT;
    if (c == '.')                              return C_DOT;
    return C_OTHER;
}

// Is this a state where a complete token can end?
static int is_accepting(State s)
{
    return s == S_IDENT || s == S_INT || s == S_FLOAT;
}

// Run the DFA from the current position and return the token it finds.
// It remembers the LAST accepting state it passed, so "3." becomes
// the integer 3 followed by a separate '.' instead of a broken float.
static Token run_dfa(void)
{
    char  buf[MAX_LINE];
    int   startLine = line;
    int   startCol  = col;

    State state      = S_START;
    State lastAccept = S_DEAD;
    int   i          = 0;      // how far ahead we have looked
    int   lastLen    = 0;      // length of the longest valid token so far

    // Walk the table until there is no valid move
    for (;;) {
        state = table[state][classify(src[pos + i])];
        if (state == S_DEAD) break;
        i++;
        if (is_accepting(state)) {
            lastAccept = state;
            lastLen    = i;
        }
    }

    // Safety net: should not happen, but never loop forever
    if (lastLen == 0) {
        char c[2] = { advance(), '\0' };
        return make_token(TOKEN_UNKNOWN, c, startLine, startCol);
    }

    // Now actually consume the characters of the token
    int len = (lastLen < MAX_LINE - 1) ? lastLen : MAX_LINE - 1;
    for (int k = 0; k < lastLen; k++) {
        char c = advance();
        if (k < len) buf[k] = c;
    }
    buf[len] = '\0';

    // The final state tells us what kind of token it is
    switch (lastAccept) {
        case S_INT:   return make_token(TOKEN_INTEGER, buf, startLine, startCol);
        case S_FLOAT: return make_token(TOKEN_FLOAT,   buf, startLine, startCol);
        default:      return make_token(checkKeyword(buf), buf, startLine, startCol);
    }
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

// Words and numbers now share the same table-driven DFA.
// These two keep their old names so lexer.h does not change.
Token scanWord(void)   { return run_dfa(); }
Token scanNumber(void) { return run_dfa(); }

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