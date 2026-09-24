#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 200
#define MAX_NAME 100
#define MAX_VALUE 100

/* Validate environment variable name */
int valid_name(char *name)
{
    if (name == NULL || name[0] == '\0')
        return 0;

    /* First character must be a letter or underscore */
    if (!isalpha(name[0]) && name[0] != '_')
        return 0;

    /* Remaining characters */
    for (int i = 1; name[i] != '\0'; i++)
    {
        if (!isalnum(name[i]) && name[i] != '_')
            return 0;
    }

    return 1;
}

/* Process export command */
void export_variable(char *input)
{
    char name[MAX_NAME];
    char value[MAX_VALUE];

    /* Find '=' */
    char *equal = strchr(input, '=');

    if (equal == NULL)
    {
        printf("Error: Invalid export syntax.\n");
        printf("Usage: export NAME=VALUE\n");
        return;
    }

    /* Extract variable name */
    int name_length = equal - input;

    if (name_length <= 0 || name_length >= MAX_NAME)
    {
        printf("Error: Invalid variable name.\n");
        return;
    }

    strncpy(name, input, name_length);
    name[name_length] = '\0';

    /* Extract value */
    strncpy(value, equal + 1, MAX_VALUE - 1);
    value[MAX_VALUE - 1] = '\0';

    /* Validate variable name */
    if (!valid_name(name))
    {
        printf("Error: Invalid variable name: %s\n", name);
        return;
    }

    /* Check existing variable */
    char *old_value = getenv(name);

    if (old_value != NULL)
    {
        printf("Existing variable found: %s=%s\n",
               name, old_value);
    }

    /* Set environment variable */
    if (setenv(name, value, 1) == -1)
    {
        perror("setenv");
        return;
    }

    printf("Export successful: %s=%s\n", name, value);
}

/* Test child process */
void test_child_process()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        /* Child process */

        printf("\nChild Process:\n");

        char *value = getenv("MYVAR");

        if (value != NULL)
        {
            printf("MYVAR=%s\n", value);
        }
        else
        {
            printf("MYVAR is not set.\n");
        }

        exit(0);
    }
    else
    {
        /* Parent process */

        wait(NULL);

        printf("Child process completed.\n");
    }
}

int main()
{
    char input[MAX_INPUT];

    printf("========================================\n");
    printf("          EXPORT COMMAND TESTER\n");
    printf("========================================\n");

    while (1)
    {
        printf("\nshell> ");

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        /* Remove newline */
        input[strcspn(input, "\n")] = '\0';

        /* Exit command */
        if (strcmp(input, "exit") == 0)
        {
            printf("Exiting...\n");
            break;
        }

        /* Export command */
        if (strncmp(input, "export ", 7) == 0)
        {
            export_variable(input + 7);
        }

        /* Test command */
        else if (strcmp(input, "test") == 0)
        {
            test_child_process();
        }

        /* Print variable */
        else if (strncmp(input, "print ", 6) == 0)
        {
            char *name = input + 6;
            char *value = getenv(name);

            if (value != NULL)
            {
                printf("%s=%s\n", name, value);
            }
            else
            {
                printf("%s is not set.\n", name);
            }
        }

        /* Unknown command */
        else
        {
            printf("Unknown command.\n");
        }
    }

    return 0;
}
