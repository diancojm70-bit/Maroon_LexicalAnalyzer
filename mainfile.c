//Main file | here the program starts

//includes necessary libraries
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <direct.h> //for _getcwd() to get the current working directory

#define MAX_SOURCE 10000
#define MAX_LINE   256

#include "lexer.h"

// Keyword table

Keyword keywords[] = {//used to identify per keyword, pero inisa-isa na lang lol
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

// Size/Number of keywords
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

// Type code at the keyboard. Finish by typing :end on a line by itself. 
static int read_from_keyboard(char *buffer)
{
    char line[MAX_LINE];
    int  used = 0;

    printf("\nType your Maroon code below.\n");
    printf("When you are finished, type  :end  on a line by itself.\n\n");

    buffer[0] = '\0';
    while (fgets(line, sizeof line, stdin) != NULL) {
        //':end' must be alone on its line (so ':endless' is not a command) 
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, ":end") == 0) break;

        int len = (int)strlen(line);
        if (used + len + 1 >= MAX_SOURCE - 1) {      //+1 for the '\n' we add back
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

static int read_from_file(const char *path, char *buffer) //reads the file and stores it in the buffer, returns -1 if error, else returns length of the file
{
    // Shows exact characters and string length received HAYS
    printf("DEBUG: Raw path = \"%s\" (length: %zu)\n", path, strlen(path)); 

    if (!has_maroon_extension(path)) {                //checks if the file has the correct extension, if not, it will return -1
        printf("\"%s\" is not a Maroon file. Only %s files are accepted.\n",
               path, MAROON_EXT);
        return -1;
    }

    FILE *fp = fopen(path, "rb");   
    if (fp == NULL) {                                 //checks if the file can be opened, if not, it will return -1
        printf("Could not open \"%s\". Check the file name and try again.\n", path);
        return -1;
    }

    size_t n = fread(buffer, 1, MAX_SOURCE - 1, fp);
    buffer[n] = '\0';

    int too_long = (fgetc(fp) != EOF);               //checks if the file is too long, if so, it will return -1
    fclose(fp);

    if (n == 0) {
        printf("\"%s\" is empty. Nothing to analyze.\n", path);
        return -1;
    }
    if (memchr(buffer, '\0', n) != NULL) {   /* a NUL byte inside = not a text file ? */
        printf("\"%s\" does not look like a text file.\n", path);
        return -1;
    }
    if (too_long)
        printf("Warning: file is longer than %d characters; it was cut off.\n",
               MAX_SOURCE);

    return normalize_source(buffer, (int)n); //returns the length of the file after normalization
}

int main(void)
{
    static char source[MAX_SOURCE];     //buffer for the source code, either typed or read from a file
    char choice[MAX_LINE];             //buffer for the user's menu choice
    char path[MAX_LINE];               //buffer for the file path
 
    char cwd[MAX_LINE];
if (_getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("Current Working Directory: %s\n", cwd); //Display the current working directory GRRRRRRR
}

    static const char *names[] = {
        [TOKEN_INTEGER]     = "INTEGER",
        [TOKEN_FLOAT]       = "FLOAT",
        [TOKEN_CHARACTER]   = "CHARACTER",
        [TOKEN_IDENTIFIER]  = "IDENTIFIER",
        [TOKEN_OPERATOR]    = "OPERATOR",
        [TOKEN_KEYWORD]     = "KEYWORD",
        [TOKEN_LEFT_PAREN]  = "LPAREN",
        [TOKEN_RIGHT_PAREN] = "RPAREN",
        [TOKEN_LEFT_BRACE]  = "LBRACE",
        [TOKEN_RIGHT_BRACE] = "RBRACE",
        [TOKEN_SEMICOLON]   = "SEMICOLON",
        [TOKEN_EOF]         = "EOF",
        [TOKEN_UNKNOWN]     = "UNKNOWN"
    };
 
    printf("=== Maroon Lexical Analyzer ===\n");
 
    for (;;) {
        const char *text = NULL;        //pointer to the source code, either typed or read from a file 
 
        printf("\n[1] Type code\n");
        printf("[2] Load a %s file\n", MAROON_EXT);
        printf("[3] Run the sample program\n");
        printf("[4] Quit\n");
        printf("Choose: ");
 
        if (fgets(choice, sizeof choice, stdin) == NULL) break;   //end of input 
 
        switch (choice[0]) { 
            case '1':
                if (read_from_keyboard(source) == 0) printf("Nothing was typed.\n");
                else                                 text = source;
                break;
            case '2':
                printf("File name: ");
                if (fgets(path, sizeof path, stdin) == NULL) return 0;
                path[strcspn(path, "\r\n")] = '\0';
                if (read_from_file(path, source) >= 0) text = source;
        
                break;
            case '3':
                printf("\n%s", SAMPLE_PROGRAM);
                text = SAMPLE_PROGRAM;
                break;
            case '4':
                printf("Goodbye!\n");
                return 0;
            default:
                printf("Please type 1, 2, 3, or 4.\n");
        }
 
        //Runs thee lexer and print em tokens
        if (text != NULL) {
            
            FILE *fp = fopen("Symbol_Table.txt", "a");                 //append mode to avoid overwriting previous results
            if (fp == NULL) {                                         //checks if the file can be opened, if not, it will continue to the next iteration of the loop
                printf("Could not open Symbol_Table.txt for writing");
                continue;
            }


            int total  = 0;
            int errors = 0;
 
            initLexer(text);
 
            printf("\n%-5s %-4s  %-11s %s\n", "LINE", "COL", "TOKEN", "LEXEME"); 
            printf("---------------------------------------------\n");

            fprintf(fp, "\n%-5s %-4s  %-11s %s\n", "LINE", "COL", "TOKEN", "LEXEME"); //write the header to the file
            fprintf(fp, "---------------------------------------------\n");
 
            for (;;) {  
                Token t = getNextToken();
                if (t.type == TOKEN_EOF) break;
 
                printf("%-5d %-4d  %-11s %s", t.line, t.column,
                       names[t.type], t.lexeme);
                if (t.type == TOKEN_UNKNOWN) {
                    printf("   <-- not valid Maroon");
                    errors++;
                }
                printf("\n");

                fprintf(fp, "%-5d %-4d  %-11s %s \n", t.line, t.column,      //duplicate the output to the file
                       names[t.type], t.lexeme);
                if (t.type == TOKEN_UNKNOWN) {
                    fprintf(fp, "   <-- not valid Maroon");
                    errors++;
                }
                printf("\n");

                total++;
            }
 
            printf("---------------------------------------------\n");
            printf("%d token(s), %d error(s)\n", total, errors);

            fprintf(fp, "---------------------------------------------\n");      //duplicate the output to the file
            fprintf(fp, "%d token(s), %d error(s)\n\n", total, errors);

            fclose(fp); // Flush and close file handle
            printf("Results successfully saved to Symbol_Table.txt\n");
        }
    } 
    return 0;
}
