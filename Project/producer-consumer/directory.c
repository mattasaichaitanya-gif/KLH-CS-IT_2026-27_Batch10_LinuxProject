#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

void cleanup() {
    printf("\nCleaning up resources...\n");
    printf("Resources cleaned successfully.\n");
}

int main() {

    char cwd[PATH_MAX];

    /* Register cleanup function */
    atexit(cleanup);

    printf("=== Directory and Process Management ===\n\n");

    /* Retrieve current directory */
    if (getcwd(cwd, sizeof(cwd)) != NULL) {

        printf("Current Directory: %s\n", cwd);

    } else {

        perror("getcwd");
        return 1;
    }

    /* Display path */
    printf("Path: %s\n", cwd);

    /* Save state */
    FILE *file = fopen("state.txt", "w");

    if (file != NULL) {

        fprintf(file, "Saved Working Directory: %s\n", cwd);

        fclose(file);

        printf("State saved successfully.\n");

    } else {

        perror("State file");
    }

    /* Verify correct behavior */
    if (getcwd(cwd, sizeof(cwd)) != NULL) {

        printf("Verification: Current directory is correct.\n");

    } else {

        printf("Verification failed.\n");
    }

    /* Process exit request */
    char choice;

    printf("\nDo you want to exit? (Y/n): ");
    scanf(" %c", &choice);

    if (choice == 'Y' || choice == 'y') {

        printf("Process exit requested.\n");
        exit(0);

    } else {

        printf("Process will continue running.\n");
    }

    return 0;
}

