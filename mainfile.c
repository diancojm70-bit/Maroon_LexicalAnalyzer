//Main file | here the program starts, Check the vibe file for the main rules and division of rules with examples
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_SOURCE 10000
#define MAX_LINE   256

#include "lexer.h"

/* =====================================
   KEYWORD TABLE
   Stores all reserved words in Maroon
   ===================================== */

Keyword keywords[] = {//Changed to simplify identification
    {"when",    TOKEN_KEYWORD},
    {"ifnot",   TOKEN_KEYWORD},
    {"instead", TOKEN_KEYWORD},
    {"cycle",   TOKEN_KEYWORD},
    {"each",    TOKEN_KEYWORD},
    {"span",    TOKEN_KEYWORD},
    {"in",      TOKEN_KEYWORD},
    {"push",    TOKEN_KEYWORD},
    {"pull",    TOKEN_KEYWORD},
    {"yes",     TOKEN_KEYWORD},
    {"no",      TOKEN_KEYWORD}
};

/* Number of keywords in the table */
const int keywordCount =
    sizeof(keywords) / sizeof(keywords[0]);

// Reader
#define MAROON_EXT ".mrn"

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

/* Returns 1 if 'path' ends with .mrn (case-insensitive), otherwise 0. */
static int has_maroon_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t ext_len = strlen(MAROON_EXT);

    if (dot == NULL || strlen(dot) != ext_len) return 0;
    for (size_t i = 0; i < ext_len; i++) {
        if (tolower((unsigned char)dot[i]) != MAROON_EXT[i]) return 0;
    }
    return 1;
}

/* Cleans up raw text so the Lexer's line/column counting is accurate:
 *   - removes a UTF-8 BOM (invisible bytes some editors add at the start)
 *   - turns Windows line endings (\r\n) into plain \n
 * Returns the new length. */
static int normalize_source(char *buffer, int len)
{
    int start = 0, out = 0;

    if (len >= 3 && (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF)
        start = 3;

    for (int i = start; i < len; i++) {
        if (buffer[i] == '\r' && i + 1 < len && buffer[i + 1] == '\n')
            continue;                       /* drop the \r of a \r\n pair */
        buffer[out++] = buffer[i];
    }
    buffer[out] = '\0';
    return out;
}

/* Type code at the keyboard. Finish by typing :end on a line by itself. */
static int read_from_keyboard(char *buffer)
{
    char line[MAX_LINE];
    int  used = 0;

    printf("\nType your Maroon code below.\n");
    printf("When you are finished, type  :end  on a line by itself.\n\n");

    buffer[0] = '\0';
    while (fgets(line, sizeof line, stdin) != NULL) {
        /* ':end' must be alone on its line (so ':endless' is not a command) */
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, ":end") == 0) break;

        int len = (int)strlen(line);
        if (used + len + 1 >= MAX_SOURCE - 1) {      /* +1 for the '\n' we add back */
            printf("Input too long (limit is %d characters). Extra input ignored.\n",
                   MAX_SOURCE);
            break;
        }
        memcpy(buffer + used, line, len);
        used += len;
        buffer[used++] = '\n';
        buffer[used] = '\0';
    }
    return used;
}

/* Load code from a .mrn file. Returns the number of characters read, or -1. */
static int read_from_file(const char *path, char *buffer)
{
    if (!has_maroon_extension(path)) {
        printf("\"%s\" is not a Maroon file. Only %s files are accepted.\n",
               path, MAROON_EXT);
        return -1;
    }

    FILE *fp = fopen(path, "rb");   /* binary: we handle line endings ourselves */
    if (fp == NULL) {
        printf("Could not open \"%s\". Check the file name and try again.\n", path);
        return -1;
    }

    size_t n = fread(buffer, 1, MAX_SOURCE - 1, fp);
    buffer[n] = '\0';

    int too_long = (fgetc(fp) != EOF);   /* anything left over? then it was cut off */
    fclose(fp);

    if (n == 0) {
        printf("\"%s\" is empty. Nothing to analyze.\n", path);
        return -1;
    }
    if (memchr(buffer, '\0', n) != NULL) {   /* a NUL byte inside = not a text file */
        printf("\"%s\" does not look like a text file.\n", path);
        return -1;
    }
    if (too_long)
        printf("Warning: file is longer than %d characters; it was cut off.\n",
               MAX_SOURCE);

    return normalize_source(buffer, (int)n);
}

int main(){

    printf("Pinky UP");
    
}