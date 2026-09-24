#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMANDS 5
#define MAX_LENGTH 100

char history[MAX_COMMANDS][MAX_LENGTH];
int count = 0;

void storeCommand(char command[]) {
    if (count < MAX_COMMANDS) {
        strcpy(history[count], command);
        count++;
    } else {
        // Remove oldest command
        for (int i = 0; i < MAX_COMMANDS - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }

        strcpy(history[MAX_COMMANDS - 1], command);
    }
}

void displayHistory() {
    if (count == 0) {
        printf("History is empty.\n");
        return;
    }

    printf("\nCommand History:\n");

    for (int i = 0; i < count; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

void retrieveCommand(int number) {
    if (number < 1 || number > count) {
        printf("Invalid history number.\n");
        return;
    }

    printf("Command: %s\n", history[number - 1]);
}

int main() {
    char command[MAX_LENGTH];
    int choice, number;

    while (1) {
        printf("\n--- COMMAND HISTORY ---\n");
        printf("1. Store Command\n");
        printf("2. Display History\n");
        printf("3. Retrieve Command\n");
        printf("4. Exit\n");
        printf("Enter choice: ");

        scanf("%d", &choice);
        getchar();

        switch (choice) {

            case 1:
                printf("Enter command: ");

                fgets(command, MAX_LENGTH, stdin);

                command[strcspn(command, "\n")] = '\0';

                storeCommand(command);

                printf("Command stored.\n");
                break;

            case 2:
                displayHistory();
                break;

            case 3:
                printf("Enter history number: ");
                scanf("%d", &number);

                retrieveCommand(number);
                break;

            case 4:
                exit(0);

            default:
                printf("Invalid choice.\n");
        }
    }

    return 0;
}
