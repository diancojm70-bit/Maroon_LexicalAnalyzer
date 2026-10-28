#ifndef LEXER_H
#define LEXER_H
//This header files has no logic, but is called by the lexer and the main file
//Above is a fail-safe declaration of the file itself, these prevents multiple declaration of this file

//enum declares named constants/tokens, these tokens are to be used later for flagging inputs in the console
typedef enum {
    // These are the primitive data types
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_CHARACTER,
    TOKEN_BOOLEAN,

    // For names
    TOKEN_IDENTIFIER,

    //Flags operators/symbols
    TOKEN_OPERATOR,     

    //Flags Keywords, was first made to identify all keywords separately but now flags all indifferently
    TOKEN_KEYWORD,       

    //identify delimiters
    TOKEN_LEFT_PAREN,        // (
    TOKEN_RIGHT_PAREN,       // )
    TOKEN_LEFT_BRACE,        // {
    TOKEN_RIGHT_BRACE,       // }
    TOKEN_SEMICOLON,         // ;

    //Flags input end, alongside unrecognized names
    TOKEN_EOF,         
    TOKEN_UNKNOWN            

} TokenType;

//Each token will have a defined structure, this will store its type & lexeme, as well as its placement
typedef struct { 
    TokenType type;    //TOKEN_LEFT_PAREN
    char lexeme[100];  //(
    int line;          //6
    int column;        //7
} Token;

//Notice: Keyword table has been removed since keywords are now hardcoded natively into the DFA

void initLexer(const char *source); //used to signal the start of scanning of source text
Token getNextToken(void);           //used to take the next token word

#endif