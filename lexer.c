// lexer.c  (Maroon lexical analyzer, written to follow the Maroon proposal)
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

// Remembers an unterminated /// comment so getNextToken can report it
static int commentError = 0;
static int commentLine  = 0;
static int commentCol   = 0;

/* =====================================
   HELPERS
   ===================================== */

// Look n characters ahead without consuming (safe: never reads past '\0')
static char peekAt(int n)
{
    for (int i = 0; i < n; i++) {
        if (src[pos + i] == '\0') return '\0';
    }
    return src[pos + n];
}

static char peek(void)     { return peekAt(0); }   // current character
static char peekNext(void) { return peekAt(1); }   // one ahead

// Consume the current character and update line/column
static char advance(void)
{
    char c = src[pos];
    if (c == '\0') return '\0';      // never move past the end
    pos++;
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
   TRANSITION TABLE  (proposal sections 2 and 9: identifiers, numbers)
   One DFA recognizes words, integers, floats and "bad words".

   States:
     S_START  nothing read yet
     S_IDENT  inside a word                      (accepting)
     S_INT    inside a whole number              (accepting)
     S_DOT    read digits then '.'               (NOT accepting)
     S_FLOAT  inside the decimal part            (accepting)
     S_BAD    a number glued to letters, like    (accepting, but invalid)
              3twobscs: identifiers can never
              begin with a digit
     S_DEAD   no valid move, stop scanning

   Character classes:
     C_LETTER  a-z A-Z _
     C_DIGIT   0-9
     C_DOT     .
     C_OTHER   anything else
   ===================================== */
typedef enum { S_START, S_IDENT, S_INT, S_DOT, S_FLOAT, S_BAD, S_DEAD,
               NUM_STATES } State;

typedef enum { C_LETTER, C_DIGIT, C_DOT, C_OTHER,
               NUM_CLASSES } CharClass;

static const State table[NUM_STATES][NUM_CLASSES] = {
    /*             LETTER    DIGIT    DOT     OTHER  */
    /* START */  { S_IDENT,  S_INT,   S_DEAD, S_DEAD },
    /* IDENT */  { S_IDENT,  S_IDENT, S_DEAD, S_DEAD },
    /* INT   */  { S_BAD,    S_INT,   S_DOT,  S_DEAD },
    /* DOT   */  { S_DEAD,   S_FLOAT, S_DEAD, S_DEAD },
    /* FLOAT */  { S_BAD,    S_FLOAT, S_DEAD, S_DEAD },
    /* BAD   */  { S_BAD,    S_BAD,   S_DEAD, S_DEAD },
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
    return s == S_IDENT || s == S_INT || s == S_FLOAT || s == S_BAD;
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
        case S_BAD:   return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
        default:      return make_token(checkKeyword(buf), buf, startLine, startCol);
    }
}

/* =====================================
   PUBLIC FUNCTIONS
   ===================================== */

// Start (or restart) scanning a new source text
void initLexer(const char *source)
{
    src          = source;
    pos          = 0;
    line         = 1;
    col          = 1;
    commentError = 0;
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

// Skip blanks and comments (proposal sections 6 and 7)
//   spaces, tabs, newlines   -> ignored
//   # ...                    -> single-line comment, ends at the line end
//   /// ... \\\              -> multi-line comment
void skipWhitespace(void)
{
    for (;;) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        }
        else if (c == '#') {
            // Single-line comment: skip until the end of the line
            while (peek() != '\n' && peek() != '\0')
                advance();
        }
        else if (c == '/' && peekAt(1) == '/' && peekAt(2) == '/') {
            // Multi-line comment: opens with three slashes, closes with three backslashes
            int startLine = line;
            int startCol  = col;
            int closed    = 0;

            advance(); advance(); advance();          // the opening ///

            while (peek() != '\0') {
                if (peek() == '\\' && peekAt(1) == '\\' && peekAt(2) == '\\') {
                    advance(); advance(); advance();  // the closing three backslashes
                    closed = 1;
                    break;
                }
                advance();
            }

            if (!closed) {                            // ran into end of file
                commentError = 1;
                commentLine  = startLine;
                commentCol   = startCol;
            }
        }
        else {
            break;   // real code starts here
        }
    }
}

// Words and numbers share the same table-driven DFA.
// These two keep their old names so lexer.h does not change.
Token scanWord(void)   { return run_dfa(); }
Token scanNumber(void) { return run_dfa(); }

// Scan a character literal (proposal section: data types)
// Valid form: a single quote, ONE printable ASCII character, a single quote.
// Examples: 'A'   'c'   '7'      Errors: 'AB'   ''   'A
Token scanCharacter(void)
{
    char buf[MAX_LINE];
    int  len = 0;

    int startLine = line;
    int startCol  = col;

    buf[len++] = advance();                       // the opening '

    char c = peek();
    if (c != '\'' && isprint((unsigned char)c) && peekNext() == '\'') {
        buf[len++] = advance();                   // the character
        buf[len++] = advance();                   // the closing '
        buf[len] = '\0';
        return make_token(TOKEN_CHARACTER, buf, startLine, startCol);
    }

    // Not a valid character literal: take everything up to the closing
    // quote (or the end of the line) so the error shows the whole thing
    while (peek() != '\'' && peek() != '\n' && peek() != '\0') {
        char ch = advance();
        if (len < MAX_LINE - 2) buf[len++] = ch;
    }
    if (peek() == '\'') buf[len++] = advance();
    buf[len] = '\0';
    return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
}

// Scan punctuation and operators (proposal sections 3 and 8)
Token scanSymbol(void)
{
    int  startLine = line;
    int  startCol  = col;
    char c = advance();
    char buf[4] = { c, '\0', '\0', '\0' };

    switch (c) {
        // Delimiters and brackets
        case '(': return make_token(TOKEN_LEFT_PAREN,  buf, startLine, startCol);
        case ')': return make_token(TOKEN_RIGHT_PAREN, buf, startLine, startCol);
        case '{': return make_token(TOKEN_LEFT_BRACE,  buf, startLine, startCol);
        case '}': return make_token(TOKEN_RIGHT_BRACE, buf, startLine, startCol);
        case ';': return make_token(TOKEN_SEMICOLON,   buf, startLine, startCol);

        // One or two characters: = == ! != < <= > >=
        case '=': case '!': case '<': case '>':
            if (peek() == '=') buf[1] = advance();
            return make_token(TOKEN_OPERATOR, buf, startLine, startCol);

        // Two characters only: && ||
        case '&': case '|':
            if (peek() == c) {
                buf[1] = advance();
                return make_token(TOKEN_OPERATOR, buf, startLine, startCol);
            }
            return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);

        // Division or integer division: / //
        // (/// was already handled as a comment by skipWhitespace)
        case '/':
            if (peek() == '/') buf[1] = advance();
            return make_token(TOKEN_OPERATOR, buf, startLine, startCol);

        // Single-character arithmetic operators: + - * % ^
        case '+': case '-': case '*': case '%': case '^':
            return make_token(TOKEN_OPERATOR, buf, startLine, startCol);

        // A stray closing comment delimiter \\\ with no opening ///
        case '\\':
            if (peek() == '\\' && peekNext() == '\\') {
                buf[1] = advance();
                buf[2] = advance();
            }
            return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
    }

    // Anything else is not part of the Maroon character set
    return make_token(TOKEN_UNKNOWN, buf, startLine, startCol);
}

// Decide which scan function to use, and return the next token
Token getNextToken(void)
{
    skipWhitespace();

    // A multi-line comment that never found its closing delimiter
    if (commentError) {
        commentError = 0;
        return make_token(TOKEN_UNKNOWN, "///", commentLine, commentCol);
    }

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