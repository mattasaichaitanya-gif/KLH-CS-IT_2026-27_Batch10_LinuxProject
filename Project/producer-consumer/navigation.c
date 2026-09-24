#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

int main() {

    char path[PATH_MAX];
    char current_dir[PATH_MAX];
    char previous_dir[PATH_MAX] = "";

    printf("Simple Directory Navigation Program\n");
    printf("Commands: cd <path>, pwd, previous, exit\n");

    while (1) {

        // Display current directory
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("getcwd");
            break;
        }

        printf("$$$ ");

        if (fgets(path, sizeof(path), stdin) == NULL) {
            break;
        }

        // Remove newline
        path[strcspn(path, "\n")] = '\0';

        // Exit command
        if (strcmp(path, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        // pwd command
        if (strcmp(path, "pwd") == 0) {
            printf("Current Directory: %s\n", current_dir);
            continue;
        }

        // previous command
        if (strcmp(path, "previous") == 0) {

            if (strlen(previous_dir) == 0) {
                printf("No previous directory available.\n");
                continue;
            }

            char temp[PATH_MAX];

            strcpy(temp, current_dir);

            if (chdir(previous_dir) == -1) {
                perror("chdir");
                continue;
            }

            strcpy(previous_dir, temp);

            printf("Changed to previous directory.\n");
            continue;
        }

        // cd command
        if (strncmp(path, "cd ", 3) == 0) {

            char *new_path = path + 3;

            // Check if path exists
            struct stat st;

            if (stat(new_path, &st) == -1) {
                perror("Invalid path");
                continue;
            }

            // Check if it is a directory
            if (!S_ISDIR(st.st_mode)) {
                printf("Error: Path is not a directory.\n");
                continue;
            }

            // Save current directory
            strcpy(previous_dir, current_dir);

            // Change directory
            if (chdir(new_path) == -1) {
                perror("chdir");
                continue;
            }

            if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
                perror("getcwd");
                continue;
            }

            printf("Directory changed successfully.\n");
            printf("New Working Directory: %s\n", current_dir);

        } else {
            printf("Invalid command.\n");
            printf("Use: cd <path>, pwd, previous, or exit\n");
        }
    }

    return 0;
}
