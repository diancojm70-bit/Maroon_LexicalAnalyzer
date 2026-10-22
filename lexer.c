/* lexer.c  (table-driven Maroon lexical analyzer)

   The transition table follows the group's DFA diagram. The states are
   named Q0 ... Q37, exactly like the circles in the diagram, so any row of
   the table can be checked against the picture.

   How to read it:
     1. CharClass  - every character belongs to one class (a table column)
     2. State      - Q0 ... Q37 from the diagram            (a table row)
     3. table      - table[state][class] = next state
     4. accept     - which states can end a token, and what that token is
     5. getNextToken() - follows the table, remembers the LAST accepting
                         state (longest match), then builds the token

   Anything the diagram has no arrow for is an error: the offending
   character becomes one TOKEN_UNKNOWN and scanning continues after it.

   Three small additions that the diagram does not draw (marked EXTRA below):
     - a /// comment that never reaches its closing delimiter is reported
       as one TOKEN_UNKNOWN instead of being read as // followed by /
     - a # comment on the very last line (no newline after it) is accepted
     - a number glued to letters, like 3twobscs, is reported as one
       TOKEN_UNKNOWN (state Q38), because an identifier can never begin
       with a digit
*/
#include <string.h>
#include "lexer.h"

#define MAX_LINE 256

/* =====================================
   LEXER STATE
   ===================================== */
static const char *src = NULL;   /* the source code text */
static int pos  = 0;             /* current position in the text */
static int line = 1;             /* current line number */
static int col  = 1;             /* current column number */

/* =====================================
   HELPERS
   ===================================== */

/* Consume the current character and update line/column */
static char advance(void)
{
    char c = src[pos];
    if (c == '\0') return '\0';      /* never move past the end */
    pos++;
    if (c == '\n') { line++; col = 1; }
    else           { col++; }
    return c;
}

/* Build a token and return it */
static Token make_token(TokenType type, const char *text, int tline, int tcol)
{
    Token t;
    t.type = type;
    strncpy(t.lexeme, text, sizeof(t.lexeme) - 1);
    t.lexeme[sizeof(t.lexeme) - 1] = '\0';   /* lexeme holds only 100 chars */
    t.line   = tline;
    t.column = tcol;
    return t;
}

/* =====================================
   1. CHARACTER CLASSES  (the table columns)
   One class for every different label on an arrow in the diagram.
   ===================================== */
typedef enum {
    C_LETTER,    /* a-z A-Z _                                  */
    C_DIGIT,     /* 0-9                                        */
    C_DOT,       /* .                                          */
    C_QUOTE,     /* '                                          */
    C_HASH,      /* #                                          */
    C_SLASH,     /* /                                          */
    C_BSLASH,    /* backslash                                  */
    C_EQ,        /* =                                          */
    C_BANG,      /* !                                          */
    C_LT,        /* <                                          */
    C_GT,        /* >                                          */
    C_AMP,       /* &                                          */
    C_PIPE,      /* |                                          */
    C_PLUS,      /* +                                          */
    C_MINUS,     /* -                                          */
    C_STAR,      /* *                                          */
    C_PERCENT,   /* %                                          */
    C_CARET,     /* ^                                          */
    C_LPAREN,    /* (                                          */
    C_RPAREN,    /* )                                          */
    C_LBRACE,    /* {                                          */
    C_RBRACE,    /* }                                          */
    C_SEMI,      /* ;                                          */
    C_SPACE,     /* the space character                        */
    C_TABCR,     /* tab and carriage return                    */
    C_NEWLINE,   /* line feed                                  */
    C_PRINT,     /* printable ASCII outside the Maroon set     */
    C_OTHER,     /* control characters and non-ASCII bytes     */
    C_EOF,       /* the end of the text                        */
    NUM_CLASSES
} CharClass;

static CharClass classify(char ch)
{
    unsigned char c = (unsigned char)ch;

    switch (c) {
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

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        return C_LETTER;
    if (c >= '0' && c <= '9')
        return C_DIGIT;
    if (c >= 32 && c <= 126)
        return C_PRINT;          /* printable, but not part of Maroon */
    return C_OTHER;
}

/* =====================================
   2. STATES  (the table rows)
   Numbered exactly like the diagram. Q_DEAD is not drawn: it means
   "no arrow for this character".
   ===================================== */
typedef enum {
    Q0,    /* start; loops on whitespace                              */
    Q1,    /* identifier / keyword                                    */
    Q2,    /* integer                                                 */
    Q3,    /* digits then "."                                         */
    Q4,    /* float                                                   */
    Q5,    /* opening ' of a character                                */
    Q6,    /* one allowed character read                              */
    Q7,    /* character                                               */
    Q8,    /* div                                                     */
    Q9,    /* integer division                                        */
    Q10,   /* inside a /// comment                                    */
    Q11,   /* comment, saw 1 backslash                                */
    Q12,   /* comment, saw 2 backslashes                              */
    Q13,   /* discard (comment closed by 3 backslashes)               */
    Q14,   /* inside a # comment                                      */
    Q15,   /* discard (# comment ended by newline)                    */
    Q16,   /* assign                =                                 */
    Q17,   /* equal to              ==                                */
    Q18,   /* not                   !                                 */
    Q19,   /* not equal to          !=                                */
    Q20,   /* less than             <                                 */
    Q21,   /* less than or equal    <=                                */
    Q22,   /* greater than          >                                 */
    Q23,   /* greater or equal      >=                                */
    Q24,   /* one &                                                   */
    Q25,   /* and                   &&                                */
    Q26,   /* one |                                                   */
    Q27,   /* or                    ||                                */
    Q28,   /* add                   +                                 */
    Q29,   /* sub                   -                                 */
    Q30,   /* mul                   *                                 */
    Q31,   /* mod                   %                                 */
    Q32,   /* exp                   ^                                 */
    Q33,   /* Lparen                (                                 */
    Q34,   /* Rparen                )                                 */
    Q35,   /* Lbrace                {                                 */
    Q36,   /* Rbrace                }                                 */
    Q37,   /* semicolon             ;                                 */
    Q38,   /* EXTRA: number glued to letters, like 3twobscs (error)   */
    Q_DEAD,
    NUM_STATES
} State;

/* =====================================
   3. THE TRANSITION TABLE
   table[state][class] -> next state.
   Every cell starts as Q_DEAD; build_table() then adds one rule per
   arrow in the diagram.
   ===================================== */
static State table[NUM_STATES][NUM_CLASSES];
static int   table_ready = 0;

static void rule(State from, CharClass on, State to)
{
    table[from][on] = to;
}

/* "any character" rule; specific rules written after it override it */
static void rule_all(State from, State to)
{
    for (int c = 0; c < NUM_CLASSES; c++) table[from][c] = to;
}

static void build_table(void)
{
    int s, c;

    for (s = 0; s < NUM_STATES; s++)
        for (c = 0; c < NUM_CLASSES; c++)
            table[s][c] = Q_DEAD;

    /* ---- q0: the first character picks the path ---- */
    rule(Q0, C_SPACE,   Q0);        /* whitespace loops on q0 */
    rule(Q0, C_TABCR,   Q0);
    rule(Q0, C_NEWLINE, Q0);
    rule(Q0, C_LETTER,  Q1);        /* letter or _   */
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

    /* ---- q1: identifier / keyword ---- */
    rule(Q1, C_LETTER, Q1);
    rule(Q1, C_DIGIT,  Q1);

    /* ---- q2, q3, q4: integer and float ---- */
    rule(Q2, C_DIGIT, Q2);
    rule(Q2, C_DOT,   Q3);
    rule(Q3, C_DIGIT, Q4);
    rule(Q4, C_DIGIT, Q4);

    /* ---- q38 (EXTRA): a number followed by letters, like 3twobscs ---- */
    rule(Q2,  C_LETTER, Q38);
    rule(Q4,  C_LETTER, Q38);
    rule(Q38, C_LETTER, Q38);
    rule(Q38, C_DIGIT,  Q38);

    /* ---- q5, q6, q7: character literal 'A' ----
       "any allowed char" = the Maroon character set (letters, digits, the
       symbols of the proposal) plus the space, but not the quote itself */
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

    /* ---- q8, q9, q10 ... q13: / and // and the /// comment ---- */
    rule(Q8, C_SLASH, Q9);          /* //  integer division          */
    rule(Q9, C_SLASH, Q10);         /* /// opens the comment         */

    rule_all(Q10, Q10);             /* any except \                  */
    rule(Q10, C_BSLASH, Q11);
    rule(Q10, C_EOF,    Q_DEAD);

    rule_all(Q11, Q10);             /* any other char goes back      */
    rule(Q11, C_BSLASH, Q12);
    rule(Q11, C_EOF,    Q_DEAD);

    rule_all(Q12, Q10);             /* any other char goes back      */
    rule(Q12, C_BSLASH, Q13);       /* \\\ closes it                 */
    rule(Q12, C_EOF,    Q_DEAD);

    /* ---- q14, q15: # comment, discarded at the newline ---- */
    rule_all(Q14, Q14);             /* any except newline            */
    rule(Q14, C_NEWLINE, Q15);
    rule(Q14, C_EOF,     Q_DEAD);

    /* ---- q16 ... q23: = == ! != < <= > >= ---- */
    rule(Q16, C_EQ, Q17);
    rule(Q18, C_EQ, Q19);
    rule(Q20, C_EQ, Q21);
    rule(Q22, C_EQ, Q23);

    /* ---- q24 ... q27: && and || ---- */
    rule(Q24, C_AMP,  Q25);
    rule(Q26, C_PIPE, Q27);

    table_ready = 1;
}

/* =====================================
   4. ACCEPTING STATES  (the double circles)
   If the walk stops in one of these, a complete token (or a discard) was
   found. States not listed are not accepting.
   ===================================== */
typedef enum {
    ACT_NONE,         /* not an accepting state                          */
    ACT_SKIP,         /* discard: throw the text away and look again     */
    ACT_WORD,         /* a word: the keyword table decides the type      */
    ACT_COMMENT_ERR,  /* EXTRA: a /// comment that never closed          */
    ACT_TOKEN         /* emit a token of the given type                  */
} Action;

typedef struct {
    Action    action;
    TokenType type;
} Accept;

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

    [Q14] = { ACT_SKIP,        TOKEN_UNKNOWN     },   /* EXTRA: comment at the end of the text */
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

/* =====================================
   PUBLIC FUNCTIONS
   ===================================== */

/* Start (or restart) scanning a new source text */
void initLexer(const char *source)
{
    src  = source;
    pos  = 0;
    line = 1;
    col  = 1;
    if (!table_ready) build_table();
}

/* Look the word up in the keyword table (defined in main.c).
   Returns the keyword's token type, or TOKEN_IDENTIFIER if not found. */
TokenType checkKeyword(const char *word)
{
    for (int i = 0; i < keywordCount; i++) {
        if (strcmp(word, keywords[i].word) == 0)
            return keywords[i].type;
    }
    return TOKEN_IDENTIFIER;
}

/* Follow the table from q0 and return the next token. The two discard
   states (comments) are consumed and the loop starts over. */
Token getNextToken(void)
{
    for (;;) {
        char  buf[MAX_LINE];

        /* q0 loops on whitespace: skip it before a token starts */
        while (table[Q0][classify(src[pos])] == Q0)
            advance();

        if (src[pos] == '\0')
            return make_token(TOKEN_EOF, "EOF", line, col);

        int   startLine = line;
        int   startCol  = col;

        State state      = Q0;
        State lastAccept = Q_DEAD;
        int   i          = 0;      /* how far ahead we have looked          */
        int   lastLen    = 0;      /* length of the longest match so far    */

        /* Follow arrows until there is none for the next character */
        for (;;) {
            state = table[state][classify(src[pos + i])];
            if (state == Q_DEAD) break;
            i++;
            if (accept[state].action != ACT_NONE) {
                lastAccept = state;
                lastLen    = i;
            }
        }

        /* No double circle was ever reached: this character cannot start
           a token, so report it as unknown and move on */
        if (lastLen == 0) {
            char c[2] = { advance(), '\0' };
            return make_token(TOKEN_UNKNOWN, c, startLine, startCol);
        }

        /* Consume the characters of the match */
        int len = (lastLen < MAX_LINE - 1) ? lastLen : MAX_LINE - 1;
        for (int k = 0; k < lastLen; k++) {
            char c = advance();
            if (k < len) buf[k] = c;
        }
        buf[len] = '\0';

        /* The accepting state decides what to do with it */
        switch (accept[lastAccept].action) {
            case ACT_SKIP:
                continue;                                   /* look again */
            case ACT_COMMENT_ERR:
                return make_token(TOKEN_UNKNOWN, "///", startLine, startCol);
            case ACT_WORD:
                return make_token(checkKeyword(buf), buf, startLine, startCol);
            default:
                return make_token(accept[lastAccept].type, buf, startLine, startCol);
        }
    }
}