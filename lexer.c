/*
   Three small additions that the diagram does not draw (marked EXTRA below):
     - a /// comment that never reaches its closing delimiter is reported
       as one TOKEN_UNKNOWN instead of being read as // followed by /
     - a # comment on the very last line (no newline after it) is accepted
     - a number glued to letters, like 3twobscs, is reported as one
       TOKEN_UNKNOWN (state Q38), because an identifier can never begin
       with a digit
*/

//lexer.c is the files function storage for the project, uses a DFA diagram to simulate states, whether a passed lexeme is accepted as a token or not
#include <string.h> //used to copy and compare text NOT identifying words immediately
#include "lexer.h" //Bring in the self-made library
#define MAX_LINE 256 //size of temporary text buffer

//lexer's memory

//static to maintain consistency in this file
static const char *src = NULL;   // the text itself
static int pos  = 0;             // current position in the text 
static int line = 1;             // current line number 
static int col  = 1;             // current column number 

// Helper

// Consume the current character and updates line/column, if its EOF don't move, if not move forwards
static char advance(void)
{
    char c = src[pos];
    if (c == '\0') return '\0';         // never move past the end 
    pos++;
    if (c == '\n') { line++; col = 1; } //new line restarts column, increments line
    else           { col++; }           //increments column if there's no new line
    return c;                           //return character consumed
}

// Build a token and return it 
static Token make_token(TokenType type, const char *text, int tline, int tcol)
{
    Token t;
    t.type = type;                                   //stores the token type
    strncpy(t.lexeme, text, sizeof(t.lexeme) - 1);   //copies the lexeme, at most 99 chars
    t.lexeme[sizeof(t.lexeme) - 1] = '\0';           //stores the null terminator/EOF marker at the edge
    t.line   = tline;                                //copies start line
    t.column = tcol;                                 //copies start col
    return t;                                        //return the finished token
}

// One class per kind of character the DFA diagram distinguishes (the table's columns)
// plus extra classes so every other possible byte is also handled
typedef enum {
    C_LETTER,    // a-z A-Z _
    C_DIGIT,     // 0-9
    C_DOT,       // .
    C_QUOTE,     // '
    C_HASH,      // #
    C_SLASH,     // /
    C_BSLASH,    //backslash
    C_EQ,        // =
    C_BANG,      // !
    C_LT,        // <
    C_GT,        // >
    C_AMP,       // &
    C_PIPE,      // |
    C_PLUS,      // +
    C_MINUS,     // -
    C_STAR,      // *
    C_PERCENT,   // %
    C_CARET,     // ^
    C_LPAREN,    // (
    C_RPAREN,    // )
    C_LBRACE,    // {
    C_RBRACE,    // }
    C_SEMI,      // ;
    C_SPACE,     //space
    C_TABCR,     //tab and CR
    C_NEWLINE,   // /n
    C_PRINT,     //printable ASCII
    C_OTHER,     //non-ASCII / control
    C_EOF,       //end of text
    NUM_CLASSES  //class counter
} CharClass;

static CharClass classify(char ch) //takes single char input to return unique class
{
    unsigned char c = (unsigned char)ch; //ensure values stay within 0-255

    switch (c) { //a switch case for all single character matching
        case '\0': return C_EOF;
        case '.':  return C_DOT;
        case '\'': return C_QUOTE;
        case '#':  return C_HASH;
        case '/':  return C_SLASH;
        case '\\': return C_BSLASH;
        case '=':  return C_EQ;
        case '!':  return C_BANG;
        case '<':  return C_LT;
        case '>':  return C_GT;
        case '&':  return C_AMP;
        case '|':  return C_PIPE;
        case '+':  return C_PLUS;
        case '-':  return C_MINUS;
        case '*':  return C_STAR;
        case '%':  return C_PERCENT;
        case '^':  return C_CARET;
        case '(':  return C_LPAREN;
        case ')':  return C_RPAREN;
        case '{':  return C_LBRACE;
        case '}':  return C_RBRACE;
        case ';':  return C_SEMI;
        case ' ':  return C_SPACE;
        case '\t': case '\r':
                   return C_TABCR;
        case '\n': return C_NEWLINE;
    }
   //handles letters & digits
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') 
        return C_LETTER;
    if (c >= '0' && c <= '9')
        return C_DIGIT;
    if (c >= 32 && c <= 126)
        return C_PRINT;          //printable, but not part of Maroon 
    return C_OTHER;
}

// Q_DEAD is not drawn, no arrow for this character.

//the DFA States
typedef enum {
    Q0,        //start; loops on whitespace
    Q1,        //identifier / keyword
    Q2,        //integer
    Q3,        //digits then "."
    Q4,        //float
    Q5,        //opening ' of a character
    Q6,        //one allowed character read
    Q7,        //character
    Q8,        //div /
    Q9,        //integer division //
    Q10,       //inside a /// comment
    Q11,       //comment, saw 1 backslash
    Q12,       //comment, saw 2 backslashes
    Q13,       //discard (comment closed by 3 backslashes)
    Q14,       //inside a # comment
    Q15,       //discard (# comment ended by newline)
    Q16,       //assign =
    Q17,       //equal to ==
    Q18,       //not !
    Q19,       //not equal to !=
    Q20,       //less than <
    Q21,       //less than or equal <=
    Q22,       //greater than >
    Q23,       //greater or equal >=
    Q24,       //one &
    Q25,       //and &&
    Q26,       //one |
    Q27,       //or ||
    Q28,       //add +
    Q29,       //sub -
    Q30,       //mul *
    Q31,       //mod %
    Q32,       //exp ^
    Q33,       //Lparen (
    Q34,       //Rparen )
    Q35,       //Lbrace {
    Q36,       //Rbrace }
    Q37,       //semicolon ;
    Q38,       //error: number glued to letters (e.g., 3twobscs)
    Q_DEAD,    //dead state
    NUM_STATES //state counter
} State;


static State table[NUM_STATES][NUM_CLASSES]; //creates a transition table using both counters
static int   table_ready = 0;                //tracks if the the table is already built

static void rule(State from, CharClass on, State to)
{
    table[from][on] = to; //rule declaration means when in the first indicated state, and a specific class was given, then proceed to this state
}
/* "any character" rule; specific rules written after it override it */
static void rule_all(State from, State to)
{
    for (int c = 0; c < NUM_CLASSES; c++) table[from][c] = to;
    //for rule_all declarations within the same state, all inputs remains within this state
}

static void build_table(void)
{
    int s, c;

    for (s = 0; s < NUM_STATES; s++)   // default all transitions to dead
        for (c = 0; c < NUM_CLASSES; c++)
            table[s][c] = Q_DEAD;

    // q0: initial transitions
    rule(Q0, C_SPACE,   Q0);   // whitespace loop
    rule(Q0, C_TABCR,   Q0);
    rule(Q0, C_NEWLINE, Q0);
    rule(Q0, C_LETTER,  Q1);   // identifier start
    rule(Q0, C_DIGIT,   Q2);
    rule(Q0, C_QUOTE,   Q5);
    rule(Q0, C_SLASH,   Q8);
    rule(Q0, C_HASH,    Q14);
    rule(Q0, C_EQ,      Q16);
    rule(Q0, C_BANG,    Q18);
    rule(Q0, C_LT,      Q20);
    rule(Q0, C_GT,      Q22);
    rule(Q0, C_AMP,     Q24);
    rule(Q0, C_PIPE,    Q26);
    rule(Q0, C_PLUS,    Q28);
    rule(Q0, C_MINUS,   Q29);
    rule(Q0, C_STAR,    Q30);
    rule(Q0, C_PERCENT, Q31);
    rule(Q0, C_CARET,   Q32);
    rule(Q0, C_LPAREN,  Q33);
    rule(Q0, C_RPAREN,  Q34);
    rule(Q0, C_LBRACE,  Q35);
    rule(Q0, C_RBRACE,  Q36);
    rule(Q0, C_SEMI,    Q37);

    // q1: identifier / keyword
    rule(Q1, C_LETTER, Q1);
    rule(Q1, C_DIGIT,  Q1);

    // q2, q3, q4: integer and float
    rule(Q2, C_DIGIT, Q2);
    rule(Q2, C_DOT,   Q3);
    rule(Q3, C_DIGIT, Q4);
    rule(Q4, C_DIGIT, Q4);

    // q38: invalid format (e.g., 3twobscs)
    rule(Q2,  C_LETTER, Q38);
    rule(Q4,  C_LETTER, Q38);
    rule(Q38, C_LETTER, Q38);
    rule(Q38, C_DIGIT,  Q38);

    // q5, q6, q7: char literal 'A'
    {
        static const CharClass allowed[] = {
            C_LETTER, C_DIGIT, C_DOT, C_HASH, C_SLASH, C_BSLASH, C_EQ,
            C_BANG, C_LT, C_GT, C_AMP, C_PIPE, C_PLUS, C_MINUS, C_STAR,
            C_PERCENT, C_CARET, C_LPAREN, C_RPAREN, C_LBRACE, C_RBRACE,
            C_SEMI, C_SPACE
        };
        for (int i = 0; i < (int)(sizeof allowed / sizeof allowed[0]); i++)
            rule(Q5, allowed[i], Q6);
    }
    rule(Q6, C_QUOTE, Q7);

    // q8..q13: slash and /// comment
    rule(Q8, C_SLASH, Q9);          // integer division //
    rule(Q9, C_SLASH, Q10);         // start /// comment

    rule_all(Q10, Q10);             // loop body
    rule(Q10, C_BSLASH, Q11);
    rule(Q10, C_EOF,    Q_DEAD);

    rule_all(Q11, Q10);             // saw 1 
    rule(Q11, C_BSLASH, Q12);
    rule(Q11, C_EOF,    Q_DEAD);

    rule_all(Q12, Q10);             // saw 2 
    rule(Q12, C_BSLASH, Q13);       // \\\ closes comment
    rule(Q12, C_EOF,    Q_DEAD);

    // q14, q15: # comment
    rule_all(Q14, Q14);             // loop until newline
    rule(Q14, C_NEWLINE, Q15);
    rule(Q14, C_EOF,     Q_DEAD);

    // q16..q23: relational operators
    rule(Q16, C_EQ, Q17);
    rule(Q18, C_EQ, Q19);
    rule(Q20, C_EQ, Q21);
    rule(Q22, C_EQ, Q23);

    // q24..q27: logical AND/OR
    rule(Q24, C_AMP,  Q25);
    rule(Q26, C_PIPE, Q27);

    table_ready = 1;
}

//accepting states

//defining the action depending on the state arrvied after the DFA processing
typedef enum {
    ACT_NONE,         /* not an accepting state                          */
    ACT_SKIP,         /* discard: throw the text away and look again     */
    ACT_WORD,         /* a word: the keyword table decides the type      */
    ACT_COMMENT_ERR,  /* EXTRA: a /// comment that never closed          */
    ACT_TOKEN         /* emit a token of the given type                  */
} Action;

//pairs every action with a token classfication
typedef struct {
    Action    action;
    TokenType type;
} Accept;

//pairs every state to their token rules, if the state will be accepted or not
static const Accept accept[NUM_STATES] = {
    [Q1]  = { ACT_WORD,        TOKEN_IDENTIFIER  },
    [Q2]  = { ACT_TOKEN,       TOKEN_INTEGER     },
    [Q4]  = { ACT_TOKEN,       TOKEN_FLOAT       },
    [Q7]  = { ACT_TOKEN,       TOKEN_CHARACTER   },

    [Q8]  = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q9]  = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q10] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },
    [Q11] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },
    [Q12] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },
    [Q13] = { ACT_SKIP,        TOKEN_UNKNOWN     },

    [Q14] = { ACT_SKIP,        TOKEN_UNKNOWN     },   // EXTRA
    [Q15] = { ACT_SKIP,        TOKEN_UNKNOWN     },

    [Q16] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q17] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q18] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q19] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q20] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q21] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q22] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q23] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q25] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q27] = { ACT_TOKEN,       TOKEN_OPERATOR    },

    [Q28] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q29] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q30] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q31] = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q32] = { ACT_TOKEN,       TOKEN_OPERATOR    },

    [Q33] = { ACT_TOKEN,       TOKEN_LEFT_PAREN  },
    [Q34] = { ACT_TOKEN,       TOKEN_RIGHT_PAREN },
    [Q35] = { ACT_TOKEN,       TOKEN_LEFT_BRACE  },
    [Q36] = { ACT_TOKEN,       TOKEN_RIGHT_BRACE },
    [Q37] = { ACT_TOKEN,       TOKEN_SEMICOLON   },

    [Q38] = { ACT_TOKEN,       TOKEN_UNKNOWN     }    /* EXTRA: 3twobscs is an error */
};


/* Start (or restart) scanning a new source text */
void initLexer(const char *source)
{
    src  = source;
    pos  = 0;
    line = 1;
    col  = 1;
    if (!table_ready) build_table(); //checks table_ready boolean, if not built, then it will be
}

// Look the word up in the keyword table (defined in main.c)
//Returns the keyword's token type, or TOKEN_IDENTIFIER if not found.
//Uses keyword table in main.c, first declared in lexer.h
//Scans the words and passses it through the DFA, but difference between keywords and identifiers are done after, here
TokenType checkKeyword(const char *word)
{
    for (int i = 0; i < keywordCount; i++) {
        if (strcmp(word, keywords[i].word) == 0)  
            return keywords[i].type;
    }
    return TOKEN_IDENTIFIER;
}


Token getNextToken(void)   //returns the next token, or TOKEN_EOF if there is no more text to scan
{
    for (;;) { //infinite loop, doesen't stop until TOKEN_EOF is passed
        char  buf[MAX_LINE];

        // q0 loops on whitespace: skip it before a token starts
        while (table[Q0][classify(src[pos])] == Q0)
            advance();

        //If the next character is the end of the text, return a TOKEN_EOF token
        if (src[pos] == '\0')
            return make_token(TOKEN_EOF, "EOF", line, col);

        // Remember where the token starts, so we can report it in the token struct
        int   startLine = line;
        int   startCol  = col;

        // Follow the table until there is no arrow for the next character, keeping track of the last accepting state and how far ahead we have looked
        State state      = Q0;
        State lastAccept = Q_DEAD;
        int   i          = 0;      // how far ahead we have looked
        int   lastLen    = 0;      // how many characters were consumed to reach the last accepting state

        // Follow the table until there is no arrow for the next character, keeping track of the last accepting state and how far ahead we have looked Maximal Munch
        for (;;) {                                            //infinite loop, doesen't stop until TOKEN_EOF is passed
            state = table[state][classify(src[pos + i])];     //lookahead, checks the next character in the text, but doesn't consume it yet
            if (state == Q_DEAD) break;                       //no arrow for the next character, so stop looking ahead
            i++;                                              //advance the lookahead counter
            if (accept[state].action != ACT_NONE) {           //if the state reached is an accepting state, then store it as the lastAccept state
                lastAccept = state;                           //store the last accepting state
                lastLen    = i;                               //store how many characters were consumed to reach the last accepting state
            }
        }

        //No double circle was ever reached: this character cannot start a token, so report it as unknown and move on 
        if (lastLen == 0) {                                              //no accepting state was reached, so the character is not valid
            char c[2] = { advance(), '\0' };                             //consume the character and store it in a string
            return make_token(TOKEN_UNKNOWN, c, startLine, startCol);   //return a TOKEN_UNKNOWN token with the character and its position
        }

        // Consume the characters of the match 
        int len = (lastLen < MAX_LINE - 1) ? lastLen : MAX_LINE - 1;    //if the match is longer than the buffer, truncate it
        for (int k = 0; k < lastLen; k++) {                            //consume the characters of the match, but don't overflow the buffer
            char c = advance();                                         //consume the character and store it in a string
            if (k < len) buf[k] = c;                                    //store the character in the buffer, but don't overflow it
        }
        buf[len] = '\0';                                                //null terminate the buffer

        //The accepting state decides what to do with it  
        switch (accept[lastAccept].action) {                                       //the accepting state decides what to do with it
            case ACT_SKIP:                                                         //discard the text and look again, so just continue the loop
                continue;                                                          
            case ACT_COMMENT_ERR:                                                        //EXTRA: a /// comment that never closed, so return a TOKEN_UNKNOWN token with the comment and its position
                return make_token(TOKEN_UNKNOWN, "///", startLine, startCol);
            case ACT_WORD:                                                               //a word: the keyword table decides the type, so check if it's a keyword or an identifier
                return make_token(checkKeyword(buf), buf, startLine, startCol);
            default:                                                                     //Default action: Bigay si token with its type, lexeme, and position
                return make_token(accept[lastAccept].type, buf, startLine, startCol);
        }
    }
    
}