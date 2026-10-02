#include <stdio.h>
#include <ctype.h>

int main() {
    int counter = 1; 
    char input;

    while (counter > 0) {
        printf("Input a character to check Maroon validity!:");
        scanf(" %c", &input);

        if (isupper(input)) {
            printf("The character '%c' is a valid uppercase letter.\n", input);
        } else if (islower(input)) {
            printf("The character '%c' is a valid lowercase letter.\n", input);
        } else if (isdigit(input)) {
            printf("The character '%c' is a valid digit.\n", input);
        } else if (input == '+' || input == '-' || input == '*' || input == '/' || input == '%' || input == '^' || input == '=' || input == '>' || input == '<' || input == '!' || input == '|' || input == '&' || input == '.' || input == ';' || input == '(' || input == ')' || input == '_' || input == '{' || input == '}' || input == '\\' || input == '\'' || input == '#') {
            printf("The character '%c' is a valid operator or punctuation.\n", input);
        } else {
            printf("The character '%c' is not from Maroon, exiting..", input);
            counter = 0;
        }
    }
    return 0;
}
