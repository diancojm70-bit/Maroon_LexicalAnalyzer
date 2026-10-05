/* ============================================================================
 *  MAROON LEXICAL ANALYZER
 *  ----------------------------------------------------------------------------
 *  Course  : COSC 303 - Principles of Programming Languages
 *  School  : Polytechnic University of the Philippines (CCIS)
 *  Project : Maroon, a Python-like interpreted language implemented in C
 *
 *  WHAT THIS PROGRAM DOES
 *  ----------------------
 *  A lexical analyzer (lexer / scanner) is the FIRST stage of a language
 *  processor. It reads raw source code (just a long string of characters) and
 *  chops it into meaningful pieces:
 *
 *      LEXEME  = the actual text found in the code      e.g.  "score"
 *      TOKEN   = the category that lexeme belongs to     e.g.  IDENTIFIER
 *
 *  Example:   when (score >= 90) {
 *      when -> KEYWORD     ( -> LPAREN     score -> IDENTIFIER
 *      >=   -> OPERATOR    90 -> INTEGER   )     -> RPAREN
 *      {    -> LBRACE
 *
 *  HOW TO USE
 *  ----------
 *  Compile :  gcc -Wall -o maroon_lexer maroon_lexer.c
 *  Run     :  ./maroon_lexer               (interactive menu)
 *             ./maroon_lexer program.mrn   (analyze a file directly)
 *
 *  HOW THE CODE IS ORGANIZED (read top to bottom)
 *  ----------------------------------------------
 *    1. Constants and data types   (TokenType, Token, Lexer) Member 1
 *    2. Keyword table              (words reserved by Maroon) Member 1
 *    3. Small helper functions     (peek, advance, ...) Member 1
 *    4. Skipping whitespace/comments                    Member 3
 *    5. Scanners                   (identifier, number, character, operator) Members 4-7
 *    6. next_token()               (decides which scanner to call)
 *    7. Printing the results       Member 8
 *    8. Getting input              (keyboard, file, sample) Member 2
 *    9. main()                     (menu) Member 8
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------------------
 * 1. CONSTANTS AND DATA TYPES
 * ------------------------------------------------------------------------- */

#define MAX_SOURCE   16384   /* biggest program we can read (characters)      */
#define MAX_LEXEME   128     /* longest single lexeme we store                */
#define MAX_MESSAGE  96      /* longest error message                         */
#define MAX_LINE     512     /* longest line typed at the keyboard            */

/* Every kind of token Maroon can produce. */
typedef enum {
    TOK_KEYWORD,      /* when, ifnot, instead, cycle, each, span, in, push, pull */
    TOK_IDENTIFIER,   /* variable names such as score, _price, User5name         */
    TOK_INTEGER,      /* whole numbers such as 45                                */
    TOK_FLOAT,        /* decimal numbers such as 3.9                             */
    TOK_CHARACTER,    /* one ASCII character in single quotes such as 'T'        */
    TOK_BOOLEAN,      /* yes or no                                               */
    TOK_OPERATOR,     /* + - * / % ^ // = == != > < >= <= && || !                */
    TOK_LPAREN,       /* (                                                       */
    TOK_RPAREN,       /* )                                                       */
    TOK_LBRACE,       /* {                                                       */
    TOK_RBRACE,       /* }                                                       */
    TOK_SEMICOLON,    /* ;                                                       */
    TOK_ERROR,        /* anything that breaks Maroon's rules                     */
    TOK_EOF           /* end of the source code (internal marker)                */
} TokenType;

/* Printable names, in the SAME ORDER as the enum above. */
static const char *TOKEN_NAMES[] = {
    "KEYWORD", "IDENTIFIER", "INTEGER", "FLOAT", "CHARACTER", "BOOLEAN",
    "OPERATOR", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMICOLON",
    "ERROR", "EOF"
};

/* One token = its type + the text it came from + where it was found. */
typedef struct {
    TokenType type;
    char      lexeme[MAX_LEXEME];
    int       line;                  /* line number (starts at 1)   */
    int       column;                /* column number (starts at 1) */
    char      message[MAX_MESSAGE];  /* only used when type == TOK_ERROR */
} Token;

/* The lexer's "bookmark": the code, where we are, and our line/column. */
typedef struct {
    const char *src;    /* the whole source code                    */
    int         pos;    /* index of the next character to read      */
    int         line;   /* current line                             */
    int         column; /* current column                           */
} Lexer;

/* ---------------------------------------------------------------------------
 * 2. KEYWORD TABLE  (from Section II-4 of the Maroon proposal)
 * ------------------------------------------------------------------------- */

static const char *KEYWORDS[] = {
    "when",    /* if          */
    "ifnot",   /* elif        */
    "instead", /* else        */
    "cycle",   /* while       */
    "each",    /* for         */
    "span",    /* range       */
    "in",      /* iterate     */
    "push",    /* print       */
    "pull",    /* input       */
    NULL       /* NULL marks the end of the list */
};

/* Returns 1 if 'word' is a Maroon keyword, otherwise 0. */
static int is_keyword(const char *word)
{
    for (int i = 0; KEYWORDS[i] != NULL; i++) {
        if (strcmp(word, KEYWORDS[i]) == 0) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * 3. SMALL HELPER FUNCTIONS
 * ------------------------------------------------------------------------- */

/* Look at the current character WITHOUT moving forward. */
static char peek(const Lexer *lx)
{
    return lx->src[lx->pos];
}

/* Look 'offset' characters ahead (1 = the next one). Never reads past the end. */
static char peek_at(const Lexer *lx, int offset)
{
    for (int i = 0; i < offset; i++) {
        if (lx->src[lx->pos + i] == '\0') return '\0';
    }
    return lx->src[lx->pos + offset];
}

/* Take the current character and MOVE forward, keeping line/column correct. */
static char advance(Lexer *lx)
{
    char c = lx->src[lx->pos];
    if (c == '\0') return c;
    lx->pos++;
    if (c == '\n') { lx->line++; lx->column = 1; }
    else           { lx->column++; }
    return c;
}

/* Is this character allowed inside an identifier (after the first one)? */
static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Fill in a Token in one call (keeps the scanners short and readable). */
static void make_token(Token *t, TokenType type, const char *lexeme,
                       int line, int column, const char *message)
{
    t->type = type;
    strncpy(t->lexeme, lexeme, MAX_LEXEME - 1);
    t->lexeme[MAX_LEXEME - 1] = '\0';
    t->line = line;
    t->column = column;
    strncpy(t->message, message, MAX_MESSAGE - 1);
    t->message[MAX_MESSAGE - 1] = '\0';
}

/* ---------------------------------------------------------------------------
 * 4. SKIPPING WHITESPACE AND COMMENTS
 *    Maroon is a FREE-FIELD language: spaces, tabs and newlines only separate
 *    tokens and are otherwise ignored. Comments are ignored too.
 *       #  ...            single-line comment (until the end of the line)
 *       /// ... \\\       multi-line comment  (three / to open, three \ to close)
 *
 *    Returns 1 if it found a problem (an unclosed /// comment) and filled
 *    'err' with an ERROR token; otherwise returns 0.
 * ------------------------------------------------------------------------- */

static int skip_ignored(Lexer *lx, Token *err)
{
    for (;;) {
        char c = peek(lx);

        if (c != '\0' && isspace((unsigned char)c)) {
            advance(lx);                                   /* whitespace */
        }
        else if (c == '#') {                               /* single-line */
            while (peek(lx) != '\0' && peek(lx) != '\n') advance(lx);
        }
        else if (c == '/' && peek_at(lx, 1) == '/' && peek_at(lx, 2) == '/') {
            int start_line = lx->line, start_col = lx->column;
            int closed = 0;

            advance(lx); advance(lx); advance(lx);         /* eat "///" */
            while (peek(lx) != '\0') {
                if (peek(lx) == '\\' && peek_at(lx, 1) == '\\' &&
                    peek_at(lx, 2) == '\\') {
                    advance(lx); advance(lx); advance(lx); /* eat "\\\" */
                    closed = 1;
                    break;
                }
                advance(lx);
            }
            if (!closed) {
                make_token(err, TOK_ERROR, "///", start_line, start_col,
                           "multi-line comment was never closed with \\\\\\");
                return 1;
            }
        }
        else {
            return 0;   /* something real: stop skipping */
        }
    }
}

/* ---------------------------------------------------------------------------
 * 5. SCANNERS - each one reads ONE kind of token
 * ------------------------------------------------------------------------- */

/* IDENTIFIER / KEYWORD / BOOLEAN
 * Rule: starts with a letter or '_', then letters, digits or '_' only.
 * After reading the word we check if it is reserved (keyword, yes, no). */
static void scan_word(Lexer *lx, Token *t, int line, int col)
{
    char buf[MAX_LEXEME];
    int  len = 0;

    while (is_ident_char(peek(lx))) {
        char c = advance(lx);
        if (len < MAX_LEXEME - 1) buf[len++] = c;
    }
    buf[len] = '\0';

    if (strcmp(buf, "yes") == 0 || strcmp(buf, "no") == 0)
        make_token(t, TOK_BOOLEAN, buf, line, col, "");
    else if (is_keyword(buf))
        make_token(t, TOK_KEYWORD, buf, line, col, "");
    else
        make_token(t, TOK_IDENTIFIER, buf, line, col, "");
}

/* INTEGER / FLOAT
 * Rule: digits              -> INTEGER   (45)
 *       digits . digits     -> FLOAT     (3.9)
 * Errors caught here:
 *       3.        (no digits after the point)
 *       3twobscs  (an identifier may not begin with a digit)        */
static void scan_number(Lexer *lx, Token *t, int line, int col)
{
    char buf[MAX_LEXEME];
    int  len = 0;
    int  is_float = 0;

    while (isdigit((unsigned char)peek(lx))) {
        char c = advance(lx);
        if (len < MAX_LEXEME - 1) buf[len++] = c;
    }

    if (peek(lx) == '.') {
        is_float = 1;
        if (len < MAX_LEXEME - 1) buf[len++] = advance(lx); else advance(lx);

        if (!isdigit((unsigned char)peek(lx))) {
            buf[len] = '\0';
            make_token(t, TOK_ERROR, buf, line, col,
                       "a float needs digits after the decimal point");
            return;
        }
        while (isdigit((unsigned char)peek(lx))) {
            char c = advance(lx);
            if (len < MAX_LEXEME - 1) buf[len++] = c;
        }
    }

    /* A letter or '_' glued to a number, e.g. 3twobscs */
    if (is_ident_char(peek(lx))) {
        while (is_ident_char(peek(lx))) {
            char c = advance(lx);
            if (len < MAX_LEXEME - 1) buf[len++] = c;
        }
        buf[len] = '\0';
        make_token(t, TOK_ERROR, buf, line, col,
                   "invalid identifier: it cannot begin with a digit");
        return;
    }

    buf[len] = '\0';
    make_token(t, is_float ? TOK_FLOAT : TOK_INTEGER, buf, line, col, "");
}

/* CHARACTER
 * Rule: exactly ONE ASCII character between single quotes, e.g. 'A'. */
static void scan_character(Lexer *lx, Token *t, int line, int col)
{
    char buf[MAX_LEXEME];
    int  len = 0;

    buf[len++] = advance(lx);                  /* the opening ' */

    if (peek(lx) == '\0' || peek(lx) == '\n') {
        buf[len] = '\0';
        make_token(t, TOK_ERROR, buf, line, col,
                   "character literal is missing its closing quote");
        return;
    }
    if (peek(lx) == '\'') {                    /* '' (empty) */
        buf[len++] = advance(lx);
        buf[len] = '\0';
        make_token(t, TOK_ERROR, buf, line, col,
                   "character literal is empty");
        return;
    }

    {
        char c = advance(lx);                  /* the character itself */
        buf[len++] = c;

        if (peek(lx) == '\'' && (unsigned char)c < 128) {
            buf[len++] = advance(lx);          /* the closing ' */
            buf[len] = '\0';
            make_token(t, TOK_CHARACTER, buf, line, col, "");
            return;
        }
    }

    /* Something went wrong: swallow the rest of the literal so we only
     * report ONE error instead of many confusing ones. */
    while (peek(lx) != '\0' && peek(lx) != '\n' && peek(lx) != '\'') {
        char c = advance(lx);
        if (len < MAX_LEXEME - 1) buf[len++] = c;
    }
    if (peek(lx) == '\'') {
        char c = advance(lx);
        if (len < MAX_LEXEME - 1) buf[len++] = c;
    }
    buf[len] = '\0';
    make_token(t, TOK_ERROR, buf, line, col,
               "character literal must hold exactly one ASCII character");
}

/* OPERATOR  (and the single-symbol delimiters)
 * Two-character operators are checked first so that '>=' is read as ONE
 * token instead of '>' followed by '='. This is called "longest match". */
static void scan_symbol(Lexer *lx, Token *t, int line, int col)
{
    char c    = advance(lx);
    char next = peek(lx);
    char buf[3] = { c, '\0', '\0' };

    switch (c) {
        /* delimiters */
        case '(': make_token(t, TOK_LPAREN,    buf, line, col, ""); return;
        case ')': make_token(t, TOK_RPAREN,    buf, line, col, ""); return;
        case '{': make_token(t, TOK_LBRACE,    buf, line, col, ""); return;
        case '}': make_token(t, TOK_RBRACE,    buf, line, col, ""); return;
        case ';': make_token(t, TOK_SEMICOLON, buf, line, col, ""); return;

        /* always a single-character operator */
        case '+': case '-': case '*': case '%': case '^':
            make_token(t, TOK_OPERATOR, buf, line, col, ""); return;

        /* '/' or '//'  (a '///' comment was already removed earlier) */
        case '/':
            if (next == '/') { buf[1] = advance(lx); }
            make_token(t, TOK_OPERATOR, buf, line, col, ""); return;

        /* '=' or '==',  '!' or '!=',  '<' or '<=',  '>' or '>=' */
        case '=': case '!': case '<': case '>':
            if (next == '=') { buf[1] = advance(lx); }
            make_token(t, TOK_OPERATOR, buf, line, col, ""); return;

        /* '&&' and '||' only. A lone '&' or '|' is not part of Maroon. */
        case '&':
            if (next == '&') {
                buf[1] = advance(lx);
                make_token(t, TOK_OPERATOR, buf, line, col, "");
            } else {
                make_token(t, TOK_ERROR, buf, line, col,
                           "incomplete operator: did you mean '&&'?");
            }
            return;
        case '|':
            if (next == '|') {
                buf[1] = advance(lx);
                make_token(t, TOK_OPERATOR, buf, line, col, "");
            } else {
                make_token(t, TOK_ERROR, buf, line, col,
                           "incomplete operator: did you mean '||'?");
            }
            return;

        /* anything else is NOT in Maroon's character set */
        default: {
            char msg[MAX_MESSAGE];
            if (isprint((unsigned char)c))
                snprintf(msg, sizeof msg,
                         "unknown symbol '%c' is not in Maroon's character set", c);
            else
                snprintf(msg, sizeof msg,
                         "unknown character (code %d) is not in Maroon's character set",
                         (unsigned char)c);
            make_token(t, TOK_ERROR, buf, line, col, msg);
            return;
        }
    }
}

/* ---------------------------------------------------------------------------
 * 6. next_token() - THE HEART OF THE LEXER
 *    Skip anything we ignore, look at the first character, and hand over to
 *    the right scanner. Each call produces exactly ONE token.
 * ------------------------------------------------------------------------- */

static void next_token(Lexer *lx, Token *t)
{
    if (skip_ignored(lx, t)) return;          /* unclosed comment error */

    int  line = lx->line;
    int  col  = lx->column;
    char c    = peek(lx);

    if (c == '\0') {
        make_token(t, TOK_EOF, "", line, col, "");
    } else if (isalpha((unsigned char)c) || c == '_') {
        scan_word(lx, t, line, col);
    } else if (isdigit((unsigned char)c)) {
        scan_number(lx, t, line, col);
    } else if (c == '\'') {
        scan_character(lx, t, line, col);
    } else {
        scan_symbol(lx, t, line, col);
    }
}

/* ---------------------------------------------------------------------------
 * 7. RUNNING THE LEXER AND PRINTING THE RESULTS
 * ------------------------------------------------------------------------- */

static void analyze(const char *source)
{
    Lexer lx = { source, 0, 1, 1 };
    Token t;
    int total = 0, errors = 0;

    printf("\n%-6s %-5s %-22s %s\n", "LINE", "COL", "LEXEME", "TOKEN");
    printf("------------------------------------------------------------\n");

    for (;;) {
        next_token(&lx, &t);
        if (t.type == TOK_EOF) break;

        total++;
        if (t.type == TOK_ERROR) {
            errors++;
            printf("%-6d %-5d %-22s %s  <-- %s\n", t.line, t.column,
                   t.lexeme, TOKEN_NAMES[t.type], t.message);
        } else {
            printf("%-6d %-5d %-22s %s\n", t.line, t.column,
                   t.lexeme, TOKEN_NAMES[t.type]);
        }
    }

    printf("------------------------------------------------------------\n");
    printf("Total tokens: %d | Errors: %d\n", total, errors);
    if (errors == 0) printf("Result: lexical analysis PASSED. No errors found.\n");
    else             printf("Result: lexical analysis FAILED. Please fix the errors above.\n");
}

/* ---------------------------------------------------------------------------
 * 8. GETTING INPUT
 * ------------------------------------------------------------------------- */

/* Sample program taken from the Maroon proposal (grading logic). */
static const char *SAMPLE_PROGRAM =
    "# Sample Maroon program\n"
    "score = 85;\n"
    "when (score >= 90) {\n"
    "    push('A');\n"
    "} ifnot (score >= 80) {\n"
    "    push('B');\n"
    "} ifnot (score >= 75) {\n"
    "    push('C');\n"
    "} instead {\n"
    "    push('F');\n"
    "}\n";

/* Type code at the keyboard. Finish by typing :end on a line by itself. */
static int read_from_keyboard(char *buffer)
{
    char line[MAX_LINE];
    int  used = 0;

    printf("\nType your Maroon code below.\n");
    printf("When you are finished, type  :end  on a line by itself.\n\n");

    buffer[0] = '\0';
    while (fgets(line, sizeof line, stdin) != NULL) {
        if (strncmp(line, ":end", 4) == 0) break;

        int len = (int)strlen(line);
        if (used + len >= MAX_SOURCE - 1) {
            printf("Input too long (limit is %d characters). Extra input ignored.\n",
                   MAX_SOURCE);
            break;
        }
        memcpy(buffer + used, line, len + 1);   /* +1 copies the '\0' too */
        used += len;
    }
    return used;
}

/* Load code from a text file. Returns the number of characters read, or -1. */
static int read_from_file(const char *path, char *buffer)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("Could not open \"%s\". Check the file name and try again.\n", path);
        return -1;
    }
    size_t n = fread(buffer, 1, MAX_SOURCE - 1, fp);
    buffer[n] = '\0';
    if (!feof(fp))
        printf("Warning: file is longer than %d characters; it was cut off.\n",
               MAX_SOURCE);
    fclose(fp);
    return (int)n;
}

/* ---------------------------------------------------------------------------
 * 9. main() - MENU
 * ------------------------------------------------------------------------- */

static void print_menu(void)
{
    printf("\n==============================================\n");
    printf("   MAROON LEXICAL ANALYZER\n");
    printf("==============================================\n");
    printf("  1. Type Maroon code\n");
    printf("  2. Analyze a file\n");
    printf("  3. Run the sample program\n");
    printf("  4. Quit\n");
    printf("Choose an option (1-4): ");
}

int main(int argc, char *argv[])
{
    static char source[MAX_SOURCE];   /* 'static' keeps this big array off the stack */
    char choice[MAX_LINE];

    /* Shortcut: ./maroon_lexer myfile.mrn */
    if (argc > 1) {
        if (read_from_file(argv[1], source) >= 0) analyze(source);
        return 0;
    }

    for (;;) {
        print_menu();
        if (fgets(choice, sizeof choice, stdin) == NULL) break;   /* Ctrl+D */

        switch (atoi(choice)) {
            case 1:
                read_from_keyboard(source);
                analyze(source);
                break;
            case 2: {
                char path[MAX_LINE];
                printf("Enter the file name: ");
                if (fgets(path, sizeof path, stdin) == NULL) break;
                path[strcspn(path, "\r\n")] = '\0';   /* remove the newline */
                if (read_from_file(path, source) >= 0) analyze(source);
                break;
            }
            case 3:
                printf("\nSource code:\n%s", SAMPLE_PROGRAM);
                analyze(SAMPLE_PROGRAM);
                break;
            case 4:
                printf("Goodbye!\n");
                return 0;
            default:
                printf("Invalid choice. Please enter a number from 1 to 4.\n");
        }
    }
    return 0;
}
