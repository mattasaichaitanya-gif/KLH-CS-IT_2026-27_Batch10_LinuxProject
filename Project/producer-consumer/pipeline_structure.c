#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMANDS 10
#define MAX_LENGTH 100

char commands[MAX_COMMANDS][MAX_LENGTH];
int count = 0;

void createPipeline(char input[]) {
    char *token;

    count = 0;

    token = strtok(input, "|");

    while (token != NULL && count < MAX_COMMANDS) {

        // Remove leading spaces
        while (*token == ' ')
            token++;

        // Remove trailing newline
        token[strcspn(token, "\n")] = '\0';

        strcpy(commands[count], token);
        count++;

        token = strtok(NULL, "|");
    }
}

void displayPipeline() {

    printf("\nPipeline Structure:\n");

    for (int i = 0; i < count; i++) {
        printf("%d. %s\n", i + 1, commands[i]);
    }
}

void displayExecutionOrder() {

    printf("\nExecution Order:\n");

    for (int i = 0; i < count; i++) {
        printf("%d -> %s\n", i + 1, commands[i]);
    }
}

int main() {

    char input[MAX_LENGTH * MAX_COMMANDS];

    printf("Enter pipeline command separated by |:\n");
    printf("Example: ls -l | grep txt | wc -l\n");

    printf("Pipeline: ");

    fgets(input, sizeof(input), stdin);

    createPipeline(input);

    if (count == 0) {
        printf("Invalid pipeline.\n");
        return 1;
    }

    displayPipeline();

    displayExecutionOrder();

    printf("\nPipeline execution completed successfully.\n");

    return 0;
}
