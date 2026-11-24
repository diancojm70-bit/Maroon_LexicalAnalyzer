#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

#define REPORT_FILE "Parse_Tree.txt"
#define FULL_PARSE_TREE 1 

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

static ParseNode *collapse(ParseNode *n)
{
    if (!FULL_PARSE_TREE && n->first_child != NULL && n->first_child == n->last_child)
        return n->first_child;
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




