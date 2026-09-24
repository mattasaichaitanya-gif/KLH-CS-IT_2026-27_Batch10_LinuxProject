#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {

    int fd[2];
    pid_t p1, p2;

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    printf("pipe created successfully.\n");

    p1 = fork();

    if (p1 < 0) {
        perror("fork");
        return 1;
    }

    if (p1 == 0) {

        // First command: ls
        close(fd[0]);

        dup2(fd[1], STDOUT_FILENO);

        close(fd[1]);

        execlp("ls", "ls", NULL);

        perror("execlp");
        exit(1);
    }

    p2 = fork();

    if (p2 < 0) {
        perror("fork");
        return 1;
    }

    if (p2 == 0) {

        // Second command: wc -l
        close(fd[1]);

        dup2(fd[0], STDIN_FILENO);

        close(fd[0]);

        execlp("wc", "wc", "-l", NULL);

        perror("execlp");
        exit(1);
    }

    // Parent process
    close(fd[0]);
    close(fd[1]);

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);

    printf("data flow completed successfully.\n");

    return 0;
}

