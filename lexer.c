//lexer.c is the files function storage for the project, uses a DFA diagram to simulate states, whether a passed lexeme is accepted as a token or not
//Removed string.h! Identifier/keyword logic is now pure DFA.
#include <stddef.h>
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

// Build a token and return it (Manually copies lexeme to avoid using string.h)
static Token make_token(TokenType type, const char *text, int tline, int tcol)
{
    Token t;
    t.type = type;                                   //stores the token type
    
    int i = 0;
    while (text[i] != '\0' && i < (int)(sizeof(t.lexeme) - 1)) {
        t.lexeme[i] = text[i];                       //copies the lexeme character by character
        i++;
    }
    t.lexeme[i] = '\0';                              //stores the null terminator/EOF marker at the edge
    
    t.line   = tline;                                //copies start line
    t.column = tcol;                                 //copies start col
    return t;                                        //return the finished token
}

// One class per kind of character the DFA diagram distinguishes (the table's columns)
// Added specific letter classes used in keywords to isolate their paths
typedef enum {
    C_A, C_C, C_D, C_E, C_F, C_H, C_I, C_L, C_N, C_O, C_P, C_S, C_T, C_U, C_W, C_Y, // Keyword letters
    C_LETTER,    // other letters (b, g, j, k, m, q, r, v, x, z, A-Z, _)
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
        // Specific keyword letters (lowercase only)
        case 'a': return C_A; case 'c': return C_C; case 'd': return C_D;
        case 'e': return C_E; case 'f': return C_F; case 'h': return C_H;
        case 'i': return C_I; case 'l': return C_L; case 'n': return C_N;
        case 'o': return C_O; case 'p': return C_P; case 's': return C_S;
        case 't': return C_T; case 'u': return C_U; case 'w': return C_W;
        case 'y': return C_Y;
    }
   //handles other letters, uppercase letters, and underscore
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
    Q1,        //identifier
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

    // KEYWORD SPECIFIC PATHS (Each step of a keyword is a state!)
    K_W1, K_W2, K_W3, K_W4,               // w-h-e-n
    K_I1, K_I2, K_I3, K_I4, K_I5,         // i-f-n-o-t
    K_IN1, K_IN2, K_IN3, K_IN4, K_IN5, K_IN6, // i-n-s-t-e-a-d (shares 'in')
    K_C1, K_C2, K_C3, K_C4, K_C5,         // c-y-c-l-e
    K_E1, K_E2, K_E3, K_E4,               // e-a-c-h
    K_S1, K_S2, K_S3, K_S4,               // s-p-a-n
    K_P1, K_PU1, K_PUS1, K_PUS2,          // p-u-s-h
    K_PUL1, K_PUL2,                       // p-u-l-l
    K_Y1, K_Y2, K_Y3,                     // y-e-s
    K_N1, K_N2,                           // n-o

    Q_DEAD,    //dead state
    NUM_STATES //state counter
} State;

static State table[NUM_STATES][NUM_CLASSES]; //creates a transition table using both counters
static int   table_ready = 0;                //tracks if the the table is already built

static void rule(State from, CharClass on, State to)
{
    table[from][on] = to; //rule declaration means when in the first indicated state, and a specific class was given, then proceed to this state
}

// "any character" rule; specific rules written after it override it 
static void rule_all(State from, State to)
{
    for (int c = 0; c < NUM_CLASSES; c++) table[from][c] = to;
    //for rule_all declarations within the same state, all inputs remains within this state
}

// Helper: If a keyword is interrupted by another letter/digit, it defaults to identifier (Q1)
static void rule_ident(State from)
{
    // C_A up through C_DIGIT encompasses all valid identifier characters in our enum
    for (int c = C_A; c <= C_DIGIT; c++) {
        rule(from, c, Q1);
    }
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
    
    // Any letter generally starts an identifier
    for (int ch = C_A; ch <= C_LETTER; ch++) rule(Q0, ch, Q1);

    // Override specific starting letters to begin keyword paths
    rule(Q0, C_W, K_W1);
    rule(Q0, C_I, K_I1);
    rule(Q0, C_C, K_C1);
    rule(Q0, C_E, K_E1);
    rule(Q0, C_S, K_S1);
    rule(Q0, C_P, K_P1);
    rule(Q0, C_Y, K_Y1);
    rule(Q0, C_N, K_N1);

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

    // q1: generic identifier loops on any letter/digit
    rule_ident(Q1);

    // KEYWORD PATHS: Default to Q1 if character breaks the exact keyword spelling
    // "when"
    rule_ident(K_W1); rule(K_W1, C_H, K_W2);
    rule_ident(K_W2); rule(K_W2, C_E, K_W3);
    rule_ident(K_W3); rule(K_W3, C_N, K_W4);
    rule_ident(K_W4);

    // "ifnot"
    rule_ident(K_I1); rule(K_I1, C_F, K_I2); rule(K_I1, C_N, K_IN1); // K_IN1 covers 'in'
    rule_ident(K_I2); rule(K_I2, C_N, K_I3);
    rule_ident(K_I3); rule(K_I3, C_O, K_I4);
    rule_ident(K_I4); rule(K_I4, C_T, K_I5);
    rule_ident(K_I5);

    // "instead" (shares 'in' with K_IN1)
    rule_ident(K_IN1); rule(K_IN1, C_S, K_IN2);
    rule_ident(K_IN2); rule(K_IN2, C_T, K_IN3);
    rule_ident(K_IN3); rule(K_IN3, C_E, K_IN4);
    rule_ident(K_IN4); rule(K_IN4, C_A, K_IN5);
    rule_ident(K_IN5); rule(K_IN5, C_D, K_IN6);
    rule_ident(K_IN6);

    // "cycle"
    rule_ident(K_C1); rule(K_C1, C_Y, K_C2);
    rule_ident(K_C2); rule(K_C2, C_C, K_C3);
    rule_ident(K_C3); rule(K_C3, C_L, K_C4);
    rule_ident(K_C4); rule(K_C4, C_E, K_C5);
    rule_ident(K_C5);

    // "each"
    rule_ident(K_E1); rule(K_E1, C_A, K_E2);
    rule_ident(K_E2); rule(K_E2, C_C, K_E3);
    rule_ident(K_E3); rule(K_E3, C_H, K_E4);
    rule_ident(K_E4);

    // "span"
    rule_ident(K_S1); rule(K_S1, C_P, K_S2);
    rule_ident(K_S2); rule(K_S2, C_A, K_S3);
    rule_ident(K_S3); rule(K_S3, C_N, K_S4);
    rule_ident(K_S4);

    // "push" & "pull"
    rule_ident(K_P1); rule(K_P1, C_U, K_PU1);
    rule_ident(K_PU1); rule(K_PU1, C_S, K_PUS1); rule(K_PU1, C_L, K_PUL1);
    
    rule_ident(K_PUS1); rule(K_PUS1, C_H, K_PUS2);
    rule_ident(K_PUS2); // push accepted

    rule_ident(K_PUL1); rule(K_PUL1, C_L, K_PUL2);
    rule_ident(K_PUL2); // pull accepted

    // "yes"
    rule_ident(K_Y1); rule(K_Y1, C_E, K_Y2);
    rule_ident(K_Y2); rule(K_Y2, C_S, K_Y3);
    rule_ident(K_Y3);

    // "no"
    rule_ident(K_N1); rule(K_N1, C_O, K_N2);
    rule_ident(K_N2);


    // q2, q3, q4: integer and float
    rule(Q2, C_DIGIT, Q2);
    rule(Q2, C_DOT,   Q3);
    rule(Q3, C_DIGIT, Q4);
    rule(Q4, C_DIGIT, Q4);

    // q38: invalid format (e.g., 3twobscs) - covers all letters including new keyword classes
    for (int ch = C_A; ch <= C_LETTER; ch++) {
        rule(Q2, ch, Q38);
        rule(Q4, ch, Q38);
        rule(Q38, ch, Q38);
    }
    rule(Q38, C_DIGIT,  Q38);

    // q5, q6, q7: char literal 'A'
    {
        static const CharClass allowed[] = {
            C_A, C_C, C_D, C_E, C_F, C_H, C_I, C_L, C_N, C_O, C_P, C_S, C_T, C_U, C_W, C_Y, // Keyword classes
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

//defining the action depending on the state arrived after the DFA processing
//Removed ACT_WORD! Keywords are directly provided a TOKEN_KEYWORD type inherently inside Accept.
typedef enum {
    ACT_NONE,         /* not an accepting state                          */
    ACT_SKIP,         /* discard: throw the text away and look again     */
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
    [Q1]  = { ACT_TOKEN,       TOKEN_IDENTIFIER  },
    [Q2]  = { ACT_TOKEN,       TOKEN_INTEGER     },
    [Q4]  = { ACT_TOKEN,       TOKEN_FLOAT       },
    [Q7]  = { ACT_TOKEN,       TOKEN_CHARACTER   },

    [Q8]  = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q9]  = { ACT_TOKEN,       TOKEN_OPERATOR    },
    [Q10] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },  //EXTRA: a /// comment that never closed
    [Q11] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },  
    [Q12] = { ACT_COMMENT_ERR, TOKEN_UNKNOWN     },
    [Q13] = { ACT_SKIP,        TOKEN_UNKNOWN     },  //EXTRA: 3 backslashes closes the comment

    [Q14] = { ACT_SKIP,        TOKEN_UNKNOWN     },  // # comment, discard it
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

    [Q38] = { ACT_TOKEN,       TOKEN_UNKNOWN     },  //EXTRA: number glued to letters

    // KEYWORD ACCEPTANCE:
    // Notice that intermediate partial keywords are still valid IDENTIFIERS if they stop midway!
    [K_W1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_W2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_W3] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_W4] = { ACT_TOKEN, TOKEN_KEYWORD }, // "when"
    
    [K_I1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_I2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_I3] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_I4] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_I5] = { ACT_TOKEN, TOKEN_KEYWORD }, // "ifnot"
    
    [K_IN1] = { ACT_TOKEN, TOKEN_KEYWORD }, // "in"
    [K_IN2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_IN3] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_IN4] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_IN5] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_IN6] = { ACT_TOKEN, TOKEN_KEYWORD }, // "instead"
    
    [K_C1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_C2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_C3] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_C4] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_C5] = { ACT_TOKEN, TOKEN_KEYWORD }, // "cycle"
    
    [K_E1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_E2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_E3] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_E4] = { ACT_TOKEN, TOKEN_KEYWORD }, // "each"
    
    [K_S1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_S2] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_S3] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_S4] = { ACT_TOKEN, TOKEN_KEYWORD }, // "span"
    
    [K_P1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_PU1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_PUS1] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_PUS2]= { ACT_TOKEN, TOKEN_KEYWORD }, // "push"
    
    [K_PUL1]= { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_PUL2]= { ACT_TOKEN, TOKEN_KEYWORD }, // "pull"
    
    [K_Y1] = { ACT_TOKEN, TOKEN_IDENTIFIER }, [K_Y2] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_Y3] = { ACT_TOKEN, TOKEN_BOOLEAN }, // "yes"
    
    [K_N1] = { ACT_TOKEN, TOKEN_IDENTIFIER },
    [K_N2] = { ACT_TOKEN, TOKEN_BOOLEAN }  // "no"
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

Token getNextToken(void)   //returns the next token, or TOKEN_EOF if there is no more text to scan
{
    for (;;) {                                                          //infinite loop, doesen't stop until TOKEN_EOF is passed
        char  buf[MAX_LINE];

                                                                        // q0 loops on whitespace: skip it before a token starts
        while (table[Q0][classify(src[pos])] == Q0)                     // while the next character is whitespace, consume it and move on
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
        for (;;) {                                                    //infinite loop, doesen't stop until TOKEN_EOF is passed
            state = table[state][classify(src[pos + i])];             //lookahead, checks the next character in the text, but doesn't consume it yet
            if (state == Q_DEAD) break;                               //no arrow for the next character, so stop looking ahead
            i++;                                                      //advance the lookahead counter
            if (accept[state].action != ACT_NONE) {                   //if the state reached is an accepting state, then store it as the lastAccept state
                lastAccept = state;                                   //store the last accepting state
                lastLen    = i;                                       //store how many characters were consumed to reach the last accepting state
            }
        }

        //No double circle was ever reached: this character cannot start a token, so report it as unknown and move on 
        if (lastLen == 0) {                                                //no accepting state was reached, so the character is not valid
            char c[2] = { advance(), '\0' };                               //consume the character and store it in a string
            return make_token(TOKEN_UNKNOWN, c, startLine, startCol);   //return a TOKEN_UNKNOWN token with the character and its position
        }

        // Consume the characters of the match 
        int len = (lastLen < MAX_LINE - 1) ? lastLen : MAX_LINE - 1;    //if the match is longer than the buffer, truncate it
        for (int k = 0; k < lastLen; k++) {                             //consume the characters of the match, but don't overflow the buffer
            char c = advance();                                         //consume the character and store it in a string
            if (k < len) buf[k] = c;                                    //store the character in the buffer, but don't overflow it
        }
        buf[len] = '\0';                                                //null terminate the buffer

        //The accepting state decides what to do with it  
        switch (accept[lastAccept].action) {                                       //the accepting state decides what to do with it
            case ACT_SKIP:                                                         //discard the text and look again, so just continue the loop
                continue;                                                          
            case ACT_COMMENT_ERR:                                                  //EXTRA: a /// comment that never closed, so return a TOKEN_UNKNOWN token with the comment and its position
                return make_token(TOKEN_UNKNOWN, "///", startLine, startCol);
            default:                                                               //Default action: Bigay si token with its type natively from the DFA, lexeme, and position
                return make_token(accept[lastAccept].type, buf, startLine, startCol);
        }
    }
}