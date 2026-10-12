
#ifndef LEXER_H
#define LEXER_H
/* =====================================
   1. TOKEN TYPES
   Labels for the different parts of Maroon code
   ===================================== */

typedef enum {

    /* Data types and identifiers */
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_CHARACTER,
    TOKEN_IDENTIFIER,

    /* Arithmetic operators */
    TOKEN_PLUS,              // +
    TOKEN_MINUS,             // -
    TOKEN_MULTIPLY,          // *
    TOKEN_DIVIDE,            // /
    TOKEN_MODULO,            // %
    TOKEN_POWER,             // ^
    TOKEN_INTEGER_DIVISION,  // //

    /* Assignment operator */
    TOKEN_ASSIGN,            // =

    /* Relational operators */
    TOKEN_EQUAL,             // ==
    TOKEN_NOT_EQUAL,         // !=
    TOKEN_GREATER,           // >
    TOKEN_LESS,              // <
    TOKEN_GREATER_EQUAL,     // >=
    TOKEN_LESS_EQUAL,        // <=

    /* Logical operators */
    TOKEN_AND,               // &&
    TOKEN_OR,                // ||
    TOKEN_NOT,               // !

    /* Keywords Changed to follow proposal accordingly */
    TOKEN_WHEN,
    TOKEN_IFNOT,
    TOKEN_INSTEAD,
    TOKEN_CYCLE,
    TOKEN_EACH,
    TOKEN_SPAN,
    TOKEN_IN,
    TOKEN_PUSH,
    TOKEN_PULL,

    /* Boolean values */
    TOKEN_YES,              // TRUE
    TOKEN_NO,             // FALSE

    /* Punctuation and grouping symbols */
    TOKEN_LEFT_PAREN,        // (
    TOKEN_RIGHT_PAREN,       // )
    TOKEN_LEFT_BRACE,        // {
    TOKEN_RIGHT_BRACE,       // }
    TOKEN_SEMICOLON,         // ;

    /* Special tokens */
    TOKEN_EOF,               // End of file
    TOKEN_UNKNOWN            // Unrecognized token

} TokenType;


/* =====================================
   2. TOKEN STRUCTURE
   Stores information about each token
   ===================================== */

typedef struct {
    TokenType type;
    char lexeme[100];
    int line;
    int column;
} Token;


/* =====================================
   3. KEYWORD STRUCTURE
   Connects a keyword to its token type
   ===================================== */

typedef struct {
    const char *word;
    TokenType type;
} Keyword;


/* =====================================
   4. KEYWORD TABLE DECLARATION
   The actual table is defined in one .c file
   ===================================== */

extern Keyword keywords[];
extern const int keywordCount;


/* =====================================
   5. LEXER FUNCTION DECLARATIONS
   Shared functions used by the lexer
   ===================================== */

void initLexer(const char *source);
void skipWhitespace(void);
Token scanWord(void);
Token scanNumber(void);
Token scanCharacter(void);
Token scanSymbol(void);
Token getNextToken(void);
TokenType checkKeyword(const char *word);

#endif