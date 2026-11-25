#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

#define REPORT_FILE "Parse_Tree.txt"
#define FULL_PARSE_TREE 1 
#define SHOW_TOKEN_FEED 0

// Output part: prints on the screen and saves to the Parse_Tree.txt file hehe

static FILE *report = NULL;
static void emit(const char *text)
{
    fputs(text, stdout);
    if (report != NULL) fputs(text, report);
}

// copies string sa box na may limited size para di mag overflow

static void copy_text(char *dest, const char *src, int size)
{
    int i = 0;
    while (src[i] != '\0' && i < size - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int text_length(const char *text)
{
    int n = 0;
    while (text[n] != '\0') n++;
    return n;
} 

// Comparing two strings letter by letter 

static int same_text(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

// Parse Tree: 

typedef struct ParseNode {
    char label[32]; // grammar rule name, like "assignment_stmt" (empty for leaves)
    char text[100]; // the lexeme, only used by leaves, parang "score"
    struct ParseNode *first_child;
    struct ParseNode *last_child;
    struct ParseNode *next_sibling;
    struct ParseNode *alloc_next; // bpara ma free yung nodes at the end
} ParseNode;

static ParseNode *all_nodes = NULL; // list of every node ever created

static ParseNode *new_node(const char *label)
{
    ParseNode *n = calloc(1, sizeof(ParseNode)); // calloc = allocate and fill with zeros
    if (n == NULL) {
        printf("Out of memory while building the parse tree.\n");
        exit(1);
    }
    copy_text(n->label, label, (int)sizeof n->label);
    n->alloc_next = all_nodes;
    all_nodes = n;
    return n;
}

static ParseNode *new_leaf(const char *text)
{
    ParseNode *n = new_node("");
    copy_text(n->text, text, (int)sizeof n->text);
    return n;
}

static void add_child(ParseNode *parent, ParseNode *child)
{
    if (child == NULL) return;
    if (parent->last_child == NULL) parent->first_child = child;
    else                            parent->last_child->next_sibling = child;
    parent->last_child = child;
}

static void free_all_nodes(void)
{
    while (all_nodes != NULL) {
        ParseNode *next = all_nodes->alloc_next;
        free(all_nodes);
        all_nodes = next;
    }
}

//if one child lang, mag pass through siya tapos kapag FULL_PARSE_TREE ay 0, skip yung layers
static ParseNode *collapse(ParseNode *n)
{
    if (!FULL_PARSE_TREE && n->first_child != NULL && n->first_child == n->last_child)
        return n->first_child;
    return n;
}


static ParseNode *wrap(const char *label, ParseNode *child)
{
    ParseNode *n = new_node(label);
    add_child(n, child);
    return n;
}

//print parse tree: 
static void print_tree(const ParseNode *n, const char *prefix, int is_last, int is_root)
{
    char name[140];
    char line[1300];
    char child_prefix[1024];
    const ParseNode *child;

    if (n->label[0] != '\0') snprintf(name, sizeof name, "<%s>", n->label);
    else                     snprintf(name, sizeof name, "%s", n->text);

    if (is_root) snprintf(line, sizeof line, "%s\n", name);
    else         snprintf(line, sizeof line, "%s%s%s\n", prefix, is_last ? "`-- " : "|-- ", name);
    emit(line);

    if (is_root) snprintf(child_prefix, sizeof child_prefix, "%s", prefix);
    else         snprintf(child_prefix, sizeof child_prefix, "%s%s", prefix, is_last ? "    " : "|   ");

    for (child = n->first_child; child != NULL; child = child->next_sibling)
        print_tree(child, child_prefix, child->next_sibling == NULL, 0);
}


// Symbol Table and Current Token na (ITEM 1 sa rubric hehe)

static Token *symbol_table = NULL;
static int symbol_count = 0; // number ng tokens sa table
static int symbol_capacity = 0; // capacity ng table before siya mag grow
static int symbol_pos = 0; // index of the next token to hand out

static Token cur; //current token na tinitignan rn ng parser
static int prev_line = 1;  

static void load_symbol_table(const char *source)
{
    Token t;

    symbol_count = 0;
    initLexer(source); // start ng lexer
    do {
        t = getNextToken(); // para makuha next token
        if (t.type == TOKEN_COMMENT) continue; // mag skip ng comments tapos dun na mag continue sa while check

        if (symbol_count == symbol_capacity) { // pag full na table, palalakihin lang 
            int new_capacity = (symbol_capacity == 0) ? 256 : symbol_capacity * 2;
            Token *bigger = realloc(symbol_table, (size_t)new_capacity * sizeof(Token));
            if (bigger == NULL) {
                printf("Out of memory while loading the symbol table.\n");
                exit(1);
            }
            symbol_table = bigger;
            symbol_capacity = new_capacity;
        }
        symbol_table[symbol_count++] = t; // store the token (last lagi ang EOF)
    } while (t.type != TOKEN_EOF);
}

static const char *type_name(TokenType type)
{
    switch (type) {
        case TOKEN_INTEGER:     return "INTEGER";
        case TOKEN_FLOAT:       return "FLOAT";
        case TOKEN_CHARACTER:   return "CHARACTER";
        case TOKEN_IDENTIFIER:  return "IDENTIFIER";
        case TOKEN_OPERATOR:    return "OPERATOR";
        case TOKEN_KEYWORD:     return "KEYWORD";
        case TOKEN_RESERVED:    return "RESERVED";
        case TOKEN_NOISE:       return "NOISE";
        case TOKEN_COMMENT:     return "COMMENT";
        case TOKEN_LEFT_PAREN:  return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE:  return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_SEMICOLON:   return "SEMICOLON";
        case TOKEN_EOF:         return "EOF";
        default:                return "UNKNOWN";
    }
}

//magbibigay ng isang token from symbol table to parser, after last token, magbibigay ulit EOF para di siya lumagpas sa table

static void get_next_symbol(void)
{
    char line[200];

    prev_line = cur.line;
    if (symbol_pos < symbol_count) cur = symbol_table[symbol_pos++];
    else                           cur = symbol_table[symbol_count - 1];

    if (SHOW_TOKEN_FEED) {
        snprintf(line, sizeof line, "  [symbol table -> parser] line %d  %-11s %s\n",
                 cur.line, type_name(cur.type), cur.lexeme);
        emit(line);
    }
}

//para malaman current position ng current sa symbol table
static int cur_index(void){return symbol_pos - 1;}

// checking if current token is this keyword: (when, push, ...)
static int cur_is_word(const char *word)
{return cur.type == TOKEN_KEYWORD && same_text(cur.lexeme, word);}

// chinecheck if current token ba ay this operator: (+, ==, ...)
static int cur_is_op(const char *op)      
{return cur.type == TOKEN_OPERATOR && same_text(cur.lexeme, op);}

// ERROR RECOVERY PART NAMAN (RUBRIC ITEM 11 at 12 hehe)

static int error_count = 0;
static int recoveries = 0; // number of times nag recover ang panic mode
static int panic = 0;

//Print ng one error message: "Syntax error on line N: <reason>"
static void report_error_at(int line, const char *message){
    char text[400];
    snprintf(text, sizeof text, "Syntax error on line %d: %s\n", line, message);
    emit(text);
    error_count++;
}

//Same lang pero para naman sa problem sa current token
static void report_error(const char *message)
{report_error_at(cur.line, message);}


//"expected <what> before <current token>", then PANIC
//Pagkatapos ma call eto, yung fucntion na nag call will return agad
//line = the line number to show in the message.
static void fail_expected_at(int line, const char *what){
    char found[128];
    char message[320];

    if (panic) return; //return agad if nagpapanic na pala

    if (cur.type == TOKEN_EOF) snprintf(found, sizeof found, "end of file");
    else                       snprintf(found, sizeof found, "'%s'", cur.lexeme);

    snprintf(message, sizeof message, "expected %s before %s%s", what, found,
             cur.type == TOKEN_UNKNOWN ? " (not a valid Maroon token)" : "");
    report_error_at(line, message);
    panic = 1;
}

static void fail_expected(const char *what)
{fail_expected_at(cur.line, what);}


//Panic mode recovery:
//After panic, mag throw siya ng tokens until maka reach ng safe to start ulit
// ';' - the broken statement is over
// '}' - stop, wag i use kasi need ng block before this
// when/cycle/each/push - stop tapos start ng new statement
// '{' - skip whole block { ... } broken statement na nasa loob
// end of file - stop
// Di siya mag stop sa identifiers, kasi baka mag start sa gitna ng broken statement tas magbigay ulit ng fake error yung "x = 1;"
static void skip_block(void)
{
    int depth = 0;
    do {
        if (cur.type == TOKEN_LEFT_BRACE)       depth++;
        else if (cur.type == TOKEN_RIGHT_BRACE) depth--;
        get_next_symbol();
    } while (depth > 0 && cur.type != TOKEN_EOF);
}

static void synchronize(void){
    panic = 0; //since recovering na, panic is over
    recoveries++;

    while (cur.type != TOKEN_EOF) {
        if (cur.type == TOKEN_SEMICOLON) {
            get_next_symbol();
            return; }
        if (cur.type == TOKEN_RIGHT_BRACE) return;
        if (cur_is_word("when") || cur_is_word("cycle") ||
            cur_is_word("each") || cur_is_word("push"))
            return;
        if (cur.type == TOKEN_LEFT_BRACE) {
            skip_block();
            // Exception: kung may kasunod na ifnot/instead, parte pa rin siya ng same when, so skip din
            if (cur_is_word("ifnot") || cur_is_word("instead")) continue;
            return; }
        get_next_symbol();
    }
}


// HELPERS para sa matching tokens
// Returns 1 pag matched, 0 if hindi (tapos panic is set)
static void take(ParseNode *parent) {
    add_child(parent, new_leaf(cur.lexeme));
    get_next_symbol(); }

// check ng grammar rule
static const char *literal_rule(const Token *t) {
    if (t->type == TOKEN_INTEGER) return "integer";
    if (t->type == TOKEN_FLOAT) return "float";
    if (t->type == TOKEN_RESERVED) return "boolean";
    if (t->type == TOKEN_CHARACTER) return text_length(t->lexeme) > 3 ? "string" : "character";
    return "identifier"; }

static ParseNode *value_node(const Token *t) {
    return wrap(literal_rule(t), new_leaf(t->lexeme)); }

/* Wraps the current token in a rule node, for example <identifier> -> score */
static void take_as(ParseNode *parent, const char *rule_name) {
    add_child(parent, wrap(rule_name, new_leaf(cur.lexeme)));
    get_next_symbol(); }

static int expect_type(ParseNode *parent, TokenType type, const char *what) {
    if (cur.type != type) {
        int line = cur.line;
        // A missing ';' ')' or '}' belongs to the END of the previous line, so rereport lang
        if ((type == TOKEN_SEMICOLON || type == TOKEN_RIGHT_PAREN || type == TOKEN_RIGHT_BRACE)
            && cur.line > prev_line)
            line = prev_line;
        fail_expected_at(line, what);
        return 0;
    }
    take(parent);
    return 1; }

static int expect_op(ParseNode *parent, const char *op) {
    if (!cur_is_op(op)) {
        char what[16];
        snprintf(what, sizeof what, "'%s'", op);
        fail_expected(what);
        return 0;
    }
    take(parent);
    return 1; }


//GRAMMAR RULES: (one function per rule)
//PAKIBASA PO PLS WITH EBNF SA DOCUMENT NAMIN HEHE
//Whenever a function calls something that can fail, it checks `panic` afterwards and quits agad if it is set.

static ParseNode *parse_statement(void); //function prototypes lang since mamaya call nila each other
static ParseNode *parse_block(void);
static ParseNode *parse_expression(void);

//A. Expressions: 
//Bakit maraming layers? this decide the order ng operations 
//LAST binds TIGHTEST so 2 + 3 * 4 becomes 2 + (3 * 4) since mas malalim '*' than '+'
//<primary_expr> ::= <identifier> | <integer> | <float> | <character> | <string>| <boolean> | '(' <expression> ')'

static ParseNode *parse_primary(void) {
    ParseNode *n = new_node("primary_expr");

    if (cur.type == TOKEN_IDENTIFIER ||
        cur.type == TOKEN_INTEGER    ||
        cur.type == TOKEN_FLOAT      ||
        cur.type == TOKEN_CHARACTER  ||
        cur.type == TOKEN_RESERVED) { // yes/no
        take_as(n, literal_rule(&cur)); }
    else if (cur.type == TOKEN_LEFT_PAREN) {
        take(n); 
        add_child(n, parse_expression());
        if (panic) return n;
        expect_type(n, TOKEN_RIGHT_PAREN, "')'");}
    else {
        fail_expected("an expression"); }
    return n;
}

//<unary_expr> ::= '!' <unary_expr> | <primary_expr> 
static ParseNode *parse_unary(void) {
    ParseNode *n = new_node("unary_expr");

    if (cur_is_op("!")) {
        take(n);
        add_child(n, parse_unary()); } 
    else { add_child(n, parse_primary());}
    return collapse(n);
}

//<exponent_expr> ::= <unary_expr> [ '^' <exponent_expr> ]
static ParseNode *parse_exponent(void) {
    ParseNode *n = new_node("exponent_expr");

    add_child(n, parse_unary());
    if (panic) return n;

    if (cur_is_op("^")) {                  // [ ... ] is optional, so it is an if
        take(n);
        add_child(n, parse_exponent());    // calls itself, so 2^3^2 groups as 2^(3^2)
    }
    return collapse(n); }

//<multiplicative_expr> ::= <exponent_expr> { ( '*' | '/' | '%' | '//' ) <exponent_expr> }
static ParseNode *parse_multiplicative(void) {
    ParseNode *n = new_node("multiplicative_expr");

    add_child(n, parse_exponent());
    if (panic) return n;

    while (cur_is_op("*") || cur_is_op("/") || cur_is_op("%") || cur_is_op("//")) {
        take(n);                           // the operator
        add_child(n, parse_exponent());    // the operand after it
        if (panic) return n; }
    return collapse(n);
}

//<additive_expr> ::= <multiplicative_expr> { ( '+' | '-' ) <multiplicative_expr> } 
static ParseNode *parse_additive(void) {
    ParseNode *n = new_node("additive_expr");

    add_child(n, parse_multiplicative());
    if (panic) return n;

    while (cur_is_op("+") || cur_is_op("-")) {
        take(n);
        add_child(n, parse_multiplicative());
        if (panic) return n; }
    return collapse(n);
}

//<relational_expr> ::= <additive_expr> { ( '>' | '<' | '>=' | '<=' ) <additive_expr> }
static ParseNode *parse_relational(void) {
    ParseNode *n = new_node("relational_expr");

    add_child(n, parse_additive());
    if (panic) return n;

    while (cur_is_op(">") || cur_is_op("<") || cur_is_op(">=") || cur_is_op("<=")) {
        take(n);
        add_child(n, parse_additive());
        if (panic) return n;
    }
    return collapse(n);
}

//<equality_expr> ::= <relational_expr> { ( '==' | '!=' ) <relational_expr> }
static ParseNode *parse_equality(void) {
    ParseNode *n = new_node("equality_expr");

    add_child(n, parse_relational());
    if (panic) return n;

    while (cur_is_op("==") || cur_is_op("!=")) {
        take(n);
        add_child(n, parse_relational());
        if (panic) return n; }
    return collapse(n);
}

//<logical_and_expr> ::= <equality_expr> { '&&' <equality_expr> } 
static ParseNode *parse_logical_and(void)
{
    ParseNode *n = new_node("logical_and_expr");

    add_child(n, parse_equality());
    if (panic) return n;

    while (cur_is_op("&&")) {
        take(n);
        add_child(n, parse_equality());
        if (panic) return n;
    }
    return collapse(n);
}

//<logical_or_expr> ::= <logical_and_expr> { '||' <logical_and_expr> }
static ParseNode *parse_logical_or(void) {
    ParseNode *n = new_node("logical_or_expr");

    add_child(n, parse_logical_and());
    if (panic) return n;

    while (cur_is_op("||")) {
        take(n);
        add_child(n, parse_logical_and());
        if (panic) return n; }
    return collapse(n);
}

//<expression> ::= <logical_or_expr>
static ParseNode *parse_expression(void) {
    ParseNode *n = new_node("expression");
    add_child(n, parse_logical_or());
    return n;
}

//B. INPUT AND ASSIGNMENT STATEMENTS
//<input_stmt> ::= <identifier> '=' 'pull' '(' ')' ';'
//<assignment_stmt> ::= <assign_var_stmt> | <assign_const_stmt> | <assign_expr_stmt>
//<assign_var_stmt> ::= <identifier> '=' <identifier> ';' a = b;
//<assign_const_stmt> ::= <identifier> '=' <constant> ';' a = 1; a = 'T';
//<assign_expr_stmt> ::= <identifier> '=' <expression> ';' a = 3 + 2 * 5 / 5 + c;
//<constant> ::= <integer> | <float> | <character> | <string> | <boolean>

//All four forms start with identifier '='. 
//We cannot tell them apart from the first token, 
//so we parse the shared start first and decide later. 
//This is called LEFT FACTORING:
// -if the next token is 'pull'   -> input statement
// -otherwise we parse the expression, then COUNT how many tokens it used.
// Only 1 token: an identifier (variable = variable) or a constant (variable = constant).
// More than 1 token: a real expression.

static int is_constant_type(TokenType type) {
    return type == TOKEN_INTEGER || type == TOKEN_FLOAT ||
           type == TOKEN_CHARACTER || type == TOKEN_RESERVED;
}

static ParseNode *parse_assign_or_input(void) {
    ParseNode *n = new_node("assign_expr_stmt");
    ParseNode *expr;
    Token first;
    int start, length;

    take_as(n, "identifier");
    if (!expect_op(n, "=")) return n;

    if (cur_is_word("pull")) { //input statement
        copy_text(n->label, "input_stmt", (int)sizeof n->label);
        take(n);                                   // pull
        if (!expect_type(n, TOKEN_LEFT_PAREN, "'('")) return n;
        if (!expect_type(n, TOKEN_RIGHT_PAREN, "')'")) return n;
        expect_type(n, TOKEN_SEMICOLON, "';'");
        return n; }

    first = cur;
    start = cur_index();
    expr = parse_expression();
    if (panic) {
        add_child(n, expr);
        return n; }
    length = cur_index() - start; 

    if (length == 1 && first.type == TOKEN_IDENTIFIER) {
        copy_text(n->label, "assign_var_stmt", (int)sizeof n->label);
        add_child(n, value_node(&first)); } 
    else if (length == 1 && is_constant_type(first.type)) {
        copy_text(n->label, "assign_const_stmt", (int)sizeof n->label);
        add_child(n, wrap("constant", value_node(&first))); } 
    else {
        add_child(n, expr); }

    if (!expect_type(n, TOKEN_SEMICOLON, "';'")) return n;

    return wrap("assignment_stmt", n);
}

//C. OUTPUT STATEMENTS NAMAN
//<output_stmt> ::= <output_string_stmt> | <output_var_stmt> | <output_mixed_stmt> | <output_expr_stmt>
//<output_string_stmt> ::= 'push' '(' <string> ')' ';' push('Hello');
//<output_var_stmt>    ::= 'push' '(' <identifier> ')' ';' push(total);
//<output_mixed_stmt>  ::= 'push' '(' <string> '+' <identifier> ')' ';' push('Total: ' + total); | 
// 'push' '(' <identifier> '+' <string> ')' ';' push(total + ' pts');
//<output_expr_stmt>   ::= 'push' '(' <expression> ')' ';' push(2 + 3 * 4);

//Same trick used sa assignment: parse the expression first, then look at WHICH tokens it
//used (they are still in the symbol table at positions start, start+1, start+2).

static ParseNode *parse_output(void) {
    ParseNode *n = new_node("output_expr_stmt"); 
    ParseNode *expr;
    int start, length;

    take(n);
    if (!expect_type(n, TOKEN_LEFT_PAREN, "'('")) return n;

    start = cur_index();
    expr = parse_expression();
    if (panic) {
        add_child(n, expr);
        return n;
    }
    length = cur_index() - start;

    if (length == 1 && symbol_table[start].type == TOKEN_CHARACTER) {
        copy_text(n->label, "output_string_stmt", (int)sizeof n->label);
        add_child(n, value_node(&symbol_table[start])); }
    else if (length == 1 && symbol_table[start].type == TOKEN_IDENTIFIER) {
        copy_text(n->label, "output_var_stmt", (int)sizeof n->label);
        add_child(n, value_node(&symbol_table[start])); }
    else if (length == 3 &&
             symbol_table[start + 1].type == TOKEN_OPERATOR &&
             same_text(symbol_table[start + 1].lexeme, "+") &&
             ((symbol_table[start].type == TOKEN_CHARACTER &&
               symbol_table[start + 2].type == TOKEN_IDENTIFIER) ||
              (symbol_table[start].type == TOKEN_IDENTIFIER &&
               symbol_table[start + 2].type == TOKEN_CHARACTER))) {
        copy_text(n->label, "output_mixed_stmt", (int)sizeof n->label);
        add_child(n, value_node(&symbol_table[start]));
        add_child(n, new_leaf("+"));
        add_child(n, value_node(&symbol_table[start + 2])); }
    else {
        add_child(n, expr);}

    if (!expect_type(n, TOKEN_RIGHT_PAREN, "')'")) return n;
    if (!expect_type(n, TOKEN_SEMICOLON, "';'")) return n;
    return wrap("output_stmt", n);
}

//D. BLOCKS, CONDITIONALS, LOOPS
//<block> ::= '{' { <statement> } '}'
//<conditional_stmt> ::= <when_stmt> | <when_instead_stmt> | <when_ifnot_stmt>
//<when_stmt> ::= <when_clause> (if)
//<when_instead_stmt> ::= <when_clause> <instead_clause> (if else)
//<when_ifnot_stmt> ::= <when_clause> <ifnot_clause> { <ifnot_clause> }
// [ <instead_clause> ] (if elseif elseif else)
//<when_clause> ::= 'when' '(' <expression> ')' <block>
//<ifnot_clause> ::= 'ifnot' '(' <expression> ')' <block>
//<instead_clause> ::= 'instead' <block>

//<loop_stmt> ::= <cycle_stmt> | <nested_cycle_stmt> | <each_stmt> | <nested_each_stmt>
//<cycle_stmt> ::= 'cycle' '(' <expression> ')' <block>
//<nested_cycle_stmt>::= 'cycle' '(' <expression> ')' <block that has another loop inside>
//<each_stmt> ::= 'each' <identifier> [ in ] <span_expr> <block>
//<nested_each_stmt> ::= 'each' <identifier> [ in ] <span_expr> <block that has another loop inside>
//<span_expr> ::= 'span' '(' <expression> ')'

static ParseNode *parse_block(void) {
    ParseNode *n = new_node("block");

    if (!expect_type(n, TOKEN_LEFT_BRACE, "'{'")) return n;

    while (cur.type != TOKEN_RIGHT_BRACE && cur.type != TOKEN_EOF) //{<statement>}
        add_child(n, parse_statement());

    expect_type(n, TOKEN_RIGHT_BRACE, "'}'");
    return n;
}

//'(' <expression> ')' <block> (same for when, ifnot and cycle)
static void parse_condition_and_block(ParseNode *n) {
    if (!expect_type(n, TOKEN_LEFT_PAREN, "'('")) return;
    add_child(n, parse_expression());
    if (panic) return;
    if (!expect_type(n, TOKEN_RIGHT_PAREN, "')'")) return;
    add_child(n, parse_block());
}

static ParseNode *parse_when_clause(void) {
    ParseNode *n = new_node("when_clause");
    take(n); 
    parse_condition_and_block(n);
    return n;
}

static ParseNode *parse_ifnot_clause(void) {
    ParseNode *n = new_node("ifnot_clause");
    take(n); // ifnot
    parse_condition_and_block(n);
    return n;
}

static ParseNode *parse_instead_clause(void) {
    ParseNode *n = new_node("instead_clause");
    take(n); //instead
    add_child(n, parse_block());
    return n;
}

static ParseNode *parse_conditional(void) {
    ParseNode *n = new_node("when_stmt");
    int ifnot_count = 0;
    int has_instead = 0;

    add_child(n, parse_when_clause());
    if (panic) return n;

    while (cur_is_word("ifnot")) { //{<ifnot_clause>}
        add_child(n, parse_ifnot_clause());
        ifnot_count++;
        if (panic) return n; }
    if (cur_is_word("instead")) { //[<instead_clause>]
        add_child(n, parse_instead_clause());
        has_instead = 1; }

    //So ngayon alam na which of the three forms yung binasa
    if (ifnot_count > 0) copy_text(n->label, "when_ifnot_stmt", (int)sizeof n->label);
    else if (has_instead) copy_text(n->label, "when_instead_stmt", (int)sizeof n->label);
    return wrap("conditional_stmt", n);
}

//Binibilang ilang loops na yung na complete, tapos if yung number goes up while nagbabasa nung body niya, may loop sa loob so nested loop siya
static int loops_finished = 0;

static ParseNode *parse_cycle(void) {
    ParseNode *n = new_node("cycle_stmt");
    int before = loops_finished;

    take(n);                               // cycle
    parse_condition_and_block(n);
    if (panic) return n;

    if (loops_finished > before) copy_text(n->label, "nested_cycle_stmt", (int)sizeof n->label);
    loops_finished++;
    return wrap("loop_stmt", n);
}

static ParseNode *parse_span_expr(void) {
    ParseNode *n = new_node("span_expr");

    if (!cur_is_word("span")) {
        fail_expected("'span'");
        return n; }
    take(n); 
    if (!expect_type(n, TOKEN_LEFT_PAREN, "'('")) return n;
    add_child(n, parse_expression());
    if (panic) return n;
    expect_type(n, TOKEN_RIGHT_PAREN, "')'");
    return n;
}

static ParseNode *parse_each(void) {
    ParseNode *n = new_node("each_stmt");
    int before = loops_finished;

    take(n); 
    if (cur.type != TOKEN_IDENTIFIER) {
        fail_expected("an identifier");
        return n; }
    take_as(n, "identifier");

    if (cur.type == TOKEN_NOISE) take(n);  //[in] (noise word, optional)

    add_child(n, parse_span_expr());
    if (panic) return n;
    add_child(n, parse_block());
    if (panic) return n;

    if (loops_finished > before) copy_text(n->label, "nested_each_stmt", (int)sizeof n->label);
    loops_finished++;
    return wrap("loop_stmt", n);
}

//E. STATEMENT AND PROGRAM
//<program>   ::= { <statement> }
//<statement> ::= <assignment_stmt> | <input_stmt> | <output_stmt> | <conditional_stmt> | <loop_stmt>
//The "|" in <statement> is decided by the FIRST token of the statement:
//identifier - assignment or input push - output
//when - conditional cycle/each - loop

static ParseNode *parse_statement_body(void)
{
    if (cur.type == TOKEN_IDENTIFIER) return parse_assign_or_input();

    if (cur.type == TOKEN_KEYWORD) {
        if (cur_is_word("push"))  return parse_output();
        if (cur_is_word("when"))  return parse_conditional();
        if (cur_is_word("cycle")) return parse_cycle();
        if (cur_is_word("each"))  return parse_each();

        if (cur_is_word("ifnot")) {
            report_error("'ifnot' has no matching 'when' before it");
            return parse_ifnot_clause(); }
        if (cur_is_word("instead")) {
            report_error("'instead' has no matching 'when' before it");
            return parse_instead_clause(); }
    }

    fail_expected("a statement");
    return NULL;
}

static ParseNode *parse_statement(void) {
    ParseNode *stmt = new_node("statement");

    add_child(stmt, parse_statement_body());
    if (panic) {
        add_child(stmt, new_node("error"));
        synchronize(); }
    return stmt;
}

//<program> ::= {<statement>} 
static ParseNode *parse_program(void) {
    ParseNode *n = new_node("program");

    while (cur.type != TOKEN_EOF) {
        if (cur.type == TOKEN_RIGHT_BRACE) { 
            report_error("unexpected '}' with no matching '{'");
            get_next_symbol();
            continue; }
        add_child(n, parse_statement());
    }
    return n;
}


//Entry point
int parseSource(const char *source) {
    ParseNode *root;
    char line[200];
    int errors;

    error_count = 0;
    recoveries = 0;
    panic = 0;
    loops_finished = 0;
    symbol_pos = 0;
    prev_line = 1;
    cur.line = 1;
    report = fopen(REPORT_FILE, "w");//na ooverwrite every run, if mag fail, mag skip lang

    emit("\n=== Maroon Syntax Analyzer ===\n");

    load_symbol_table(source); //lalagyan ng tokens yung table
    snprintf(line, sizeof line, "Symbol table loaded: %d token(s). The parser reads them one at a time.\n",
             symbol_count - 1); //-1 since yung last entry ay EOF
    emit(line);

    get_next_symbol(); //i load yung first token sa `cur`
    root = parse_program();

    if (error_count == 0) {
        emit("No syntax errors found. Parse tree:\n\n");
        print_tree(root, "", 1, 1); } 
    else {
        snprintf(line, sizeof line, "\nParse tree not generated: %d syntax error(s) found.\n", error_count);
        emit(line);
        snprintf(line, sizeof line, "Error recovery method: panic mode (recovered %d time(s), parsing continued).\n",
                 recoveries);
        emit(line); }

    errors = error_count;
    free_all_nodes();
    free(symbol_table);
    symbol_table = NULL;
    symbol_capacity = 0;
    symbol_count = 0;

    if (report != NULL) {
        fclose(report);
        report = NULL;
        printf("\nResults saved to %s\n", REPORT_FILE); }

    return errors;
}
