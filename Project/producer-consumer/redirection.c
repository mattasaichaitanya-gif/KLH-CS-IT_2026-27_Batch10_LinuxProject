#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main() {

    pid_t pid;
    int fd;

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {

        // Open output file
        fd = open("output.txt",
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

        if (fd < 0) {
            perror("open");
            exit(1);
        }

        // Redirect stdout to file
        dup2(fd, STDOUT_FILENO);

        close(fd);

        // Execute command
        execlp("ls", "ls", NULL);

        perror("execlp");
        exit(1);
    }

    // Parent process
    wait(NULL);

    printf("Output redirected to output.txt\n");

    return 0;
}

