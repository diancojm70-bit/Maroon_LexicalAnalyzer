#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

void is_valid_character() {
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
}

void is_valid_name(){
    int counter = 1; //counter for loop control
    char input [20]; //array to store user input

    while(counter > 0){ //loop to keep asking for input until a non-valid character is entered
        printf("Check Name validity!: ");
        fgets(input, sizeof(input), stdin); //fgets to read a line of input from the user and store it in the input array
        input[strcspn(input, "\n")] = '\0'; //remove the newline character from the input string

        bool hasspace = false; //flag to check if the input has a space
        for(int i = 0; input[i] != '\0'; i++){ //loop through each character in the input string
            if(isspace(input[i])){
                hasspace = true; //set the flag to true if a space is found
                break;
            }
        }
        bool haspunct = false; //flag to check if the input has a punctuation
        for(int i = 0; input[i] != '\0'; i++){ //loop through each character in the input string
            if(ispunct(input[i]) && input[i] != '_'){ // check if the character is a punctuation and not an underscore
                haspunct = true; //set the flag to true if a punctuation is found
                break;
            }
        }

        if(isdigit((unsigned char)input[0])){ //check if the first character of the input is a digit, unisgned char cast is used to avoid undefined behavior with negative values
            printf("The name has a digit '%s'. This is not valid in maroon. Exiting...\n", input);
            counter = 0;
        }else if(strcmp(input, "when") == 0 || strcmp(input, "ifnot") == 0 ||  //check if the input matches any of the Maroon's keywords
                 strcmp(input, "instead") == 0 || strcmp(input, "cycle") == 0 || 
                 strcmp(input, "each") == 0 || strcmp(input, "span") == 0 || 
                 strcmp(input, "in") == 0 || strcmp(input, "push") == 0 || 
                 strcmp(input, "pull") == 0 || strcmp(input, "yes") == 0 || 
                 strcmp(input, "no") == 0) {
            printf("The name '%s' is a keyword, not valid in maroon.", input);
            counter = 0;          
        } else if (strcmp(input, "+") == 0 || 
            strcmp(input, "-") == 0 || strcmp(input, "*") == 0 || strcmp(input, "/") == 0 || 
            strcmp(input, "%") == 0 || strcmp(input, "^") == 0 || strcmp(input, "=") == 0 || 
            strcmp(input, ">") == 0 || strcmp(input, "<") == 0 || strcmp(input, "!") == 0 || 
            strcmp(input, "|") == 0 || strcmp(input, "&") == 0 || strcmp(input, ".") == 0 || 
            strcmp(input, ";") == 0 || strcmp(input, "(") == 0 || strcmp(input, ")") == 0 || 
            strcmp(input, "{") == 0 || strcmp(input, "}") == 0 || 
            strcmp(input, "\\") == 0 || strcmp(input, "'") == 0 || strcmp(input, "#") == 0) {
            printf("The character '%s' is a valid operator or punctuation in Maroon. Not Allowed\n", input);
            counter = 0;  
        }else if(haspunct == true){ //check if the input has a punctuation, references haspunct flag set in the previous loop
            printf("The name '%s' has a punctuation. This is not valid in maroon. Exiting...\n", input);
            counter = 0;
        }else if(hasspace == true){ //check if the input has a space, references haspunct flag set in the previous loop
            printf("The name '%s' has a space. This is not valid in maroon. Exiting...\n", input);
            counter = 0;
        }else{ //accepts if the input is a valid name in Maroon, prints a message and continues the loop
            printf("The name '%s' is valid in maroon.\n", input);
        }
    }
}


void is_valid_word() { 
    char typed_word[50]; 
    int counter = 1;

    // Added loop to keep asking for words, just like your character and name checks
    while (counter > 0) {
        printf("Enter a word to classify (or type 'exit' to return to menu): ");
        
        if (scanf("%49s", typed_word) != 1) {
            printf("Error reading input.\n");
            return;
        }

        // Custom escape route so the user can easily go back to the main menu
        if (strcmp(typed_word, "exit") == 0) {
            printf("Returning to main menu...\n");
            counter = 0;
            continue;
        }

        const char *keywords[] = {
            "when", "ifnot", "instead", "cycle", "each", 
            "span", "in", "push", "pull", "yes", "no"
        };
        int total_keywords = 11;
        bool is_keyword = false;

        for (int i = 0; i < total_keywords; i++) {
            if (strcmp(typed_word, keywords[i]) == 0) {
                is_keyword = true;
                break; 
            }
        }

        if (is_keyword) {
            printf("Classification: KEYWORD -> '%s'\n", typed_word);
        } else {
            printf("Classification: IDENTIFIER -> '%s'\n", typed_word);
        }
    }
}


void infer_literal_type() {
    int counter = 1; //counter for loop control
    char input[50];

    while (counter > 0) { //loop to keep asking for input until a non-valid character is entered
        printf("Enter a literal to classify (e.g., 45, -3.9, 'T', yes) or type 'Exit': ");
        if (scanf("%49s", input) != 1) {
            printf("Error reading input.\n");
            return;
        }

        int len = strlen(input);

        // Check if the user wants to leave the loop manually
        if (strcmp(input, "Exit") == 0) {
            printf("Going back to main menu...\n"); // Fixed: removed extra unused argument
            counter = 0; // Update loop control
            return;      // Safely drop back to main loop menu execution
        }

        // 1. Rule: Check for BOOLEAN literals
        if (strcmp(input, "yes") == 0 || strcmp(input, "no") == 0) {
            printf("Inferred Type: BOOLEAN -> '%s'\n", input);
            continue; // CRITICAL FIX: Use continue to loop again instead of returning!
        }

        // 2. Rule: Check for CHARACTER literals
        if (len == 3 && input[0] == '\'' && input[2] == '\'') {
            printf("Inferred Type: CHARACTER -> %s\n", input);
            continue; // CRITICAL FIX: Use continue to loop again
        }

        // 3. Rule: Check for Numeric formats (Supports Negative Numbers)
        int i = 0;

        // Handle optional leading negative or positive sign
        if (input[i] == '-' || input[i] == '+') {
            i++;
        }

        // Ensure there is at least one digit after the sign
        if (input[i] == '\0' || !isdigit((unsigned char)input[i])) {
            printf("Inferred Type: INVALID Maroon literal -> '%s'\n", input);
            continue; // CRITICAL FIX: Use continue to loop again
        }

        bool has_decimal = false;
        bool is_invalid = false;

        // Loop through digits using lookahead strategy
        while (input[i] != '\0') {
            if (isdigit((unsigned char)input[i])) {
                i++;
            } 
            // Lookahead: Check if current char is a decimal point followed immediately by a digit
            else if (input[i] == '.' && !has_decimal) {
                if (isdigit((unsigned char)input[i + 1])) {
                    has_decimal = true;
                    i++; // Safely advance past the decimal point
                } else {
                    printf("Inferred Type: INVALID Maroon literal -> '%s'\n", input);
                    is_invalid = true;
                    break;
                }
            } 
            else {
                printf("Inferred Type: INVALID Maroon literal -> '%s'\n", input);
                is_invalid = true;
                break;
            }
        }

        // If the lookahead parser flagged an early validation issue, restart loop cycle
        if (is_invalid) {
            continue;
        }

        // 4. Output final inferred numerical classification
        if (has_decimal) {
            printf("Inferred Type: FLOAT -> '%s'\n", input);
        } else {
            printf("Inferred Type: INTEGER -> '%s'\n", input);
        }
    }
}

int main() {
 int choice; 
 int counter = 1;

 printf("Choose an option:\n");

 while(counter > 0) {
    printf("1. Check Character Validity\n");
    printf("2. Check Name Validity\n");
    printf("3. Check Word Validity\n");
    printf("4. Infer Literal Type\n");
    printf("Enter your choice (1-4): ");
    if (scanf("%d", &choice) >= 5) {
            printf("Invalid input. Please enter a valid number.\n");
            while (getchar() != '\n'); // Clear text strings out of numerical buffer
            continue;
        }

        // Consume the remaining newline character ('\n') from the menu input 
        // so that fgets inside functions like is_valid_name() isn't bypassed instantly.
        while (getchar() != '\n'); 

    switch (choice) {
        case 1:
            is_valid_character(); //call the function to check character validity
            break;
        case 2:
            is_valid_name(); //call the function to check name validity
            break;
        case 3:
            is_valid_word(); //call the function to check word validity
            break;
        case 4:
            infer_literal_type(); //call the function to infer literal type
            break;
        default:
            printf("Invalid choice. Exiting.\n");
            counter = 0; //exit the loop if the user enters an invalid choice
            break;
    }
  }
}