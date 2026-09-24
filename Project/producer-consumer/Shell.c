#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 100
#define MAX_ARGS 20

/* Function declarations */
void command_pwd(char **args);
void command_cd(char **args);
void command_help(char **args);
void command_exit(char **args);

/* Dispatch table */
typedef struct {
    char *name;
    void (*function)(char **args);
} Command;

/* Built-in command table */
Command dispatch_table[] = {
    {"pwd", command_pwd},
    {"cd", command_cd},
    {"help", command_help},
    {"exit", command_exit}
};

int command_count =
    sizeof(dispatch_table) / sizeof(dispatch_table[0]);

/* pwd command */
void command_pwd(char **args) {

    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

/* cd command */
void command_cd(char **args) {

    if (args[1] == NULL) {
        printf("cd: missing argument\n");
        return;
    }

    if (chdir(args[1]) != 0) {
        perror("cd");
    }
}

/* help command */
void command_help(char **args) {

    printf("\nAvailable commands:\n");
    printf("  pwd       - Show current directory\n");
    printf("  cd <dir>  - Change directory\n");
    printf("  help      - Show available commands\n");
    printf("  exit      - Exit shell\n");
    printf("  Other commands are executed as processes.\n\n");
}

/* exit command */
void command_exit(char **args) {

    printf("Exiting shell...\n");
    exit(0);
}

/* Execute external process */
void execute_process(char **args) {

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {

        if (execvp(args[0], args) == -1) {
            perror("Command execution failed");
            exit(1);
        }

    } else {

        /* Parent process */
        waitpid(pid, NULL, 0);
    }
}

/* Check dispatch table */
int execute_builtin(char **args) {

    for (int i = 0; i < command_count; i++) {

        if (strcmp(args[0], dispatch_table[i].name) == 0) {

            dispatch_table[i].function(args);

            return 1;
        }
    }

    return 0;
}

/* Convert input into arguments */
int parse_input(char *input, char **args) {

    int count = 0;

    char *token = strtok(input, " \t\n");

    while (token != NULL && count < MAX_ARGS - 1) {

        args[count++] = token;

        token = strtok(NULL, " \t\n");
    }

    args[count] = NULL;

    return count;
}

int main() {

    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    printf("====================================\n");
    printf("        SIMPLE COMMAND SHELL        \n");
    printf("====================================\n");

    while (1) {

        printf("myshell> ");

        /* Read command */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        /* Remove newline */
        input[strcspn(input, "\n")] = '\0';

        /* Ignore empty input */
        if (strlen(input) == 0) {
            continue;
        }

        /* Convert input into arguments */
        parse_input(input, args);

        /* Check built-in commands */
        if (execute_builtin(args)) {
            continue;
        }

        /* Execute external command */
        execute_process(args);
    }

    return 0;
}

