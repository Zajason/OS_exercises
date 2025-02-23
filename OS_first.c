#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_OPEN_FAILED 4
#define EXIT_WRITE_FAILED 5
#define EXIT_READ_FAILED 6

int is_valid_integer(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (!isdigit(str[i]))
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        write(STDERR_FILENO, "Error: Missing or too many arguments.\n", 38);
        write(STDERR_FILENO, "Example: ./OS_first.c 5\n", 23);
        exit(EXIT_INVALID_ARGS);
    }
    if (!is_valid_integer(argv[1]))
    {
        write(STDERR_FILENO, "N must be a positive integer.\n", 30);
        exit(EXIT_INVALID_ARGS);
    }

    int N = atoi(argv[1]);
    if (N <= 0)
    {
        write(STDERR_FILENO, "N must be a positive integer.\n", 30);
        exit(EXIT_INVALID_ARGS);
    }

    pid_t *child_pids = (pid_t *)malloc(N * sizeof(pid_t));
    if (child_pids == NULL)
    {
        perror("malloc failed for child_pids");
        exit(EXIT_MALLOC_FAILED);
    }

    for (int i = 0; i < N; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            free(child_pids);
            exit(EXIT_FORK_FAILED);
        }

        if (pid == 0)
        {

            char *msg = (char *)malloc(50 * sizeof(char));
            if (msg == NULL)
            {
                perror("malloc failed for msg");
                exit(EXIT_MALLOC_FAILED);
            }
            int msg_len = snprintf(msg, 50, "Child process created: PID = %d\n", getpid());
            write(STDOUT_FILENO, msg, msg_len);
            free(msg);

            char *filename = (char *)malloc(50 * sizeof(char));
            if (filename == NULL)
            {
                perror("malloc failed for filename");
                exit(EXIT_MALLOC_FAILED);
            }
            snprintf(filename, 50, "output_%d.txt", getpid());

            int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0)
            {
                perror("open failed");
                free(filename);
                exit(EXIT_OPEN_FAILED);
            }

            char *buffer = (char *)malloc(100 * sizeof(char));
            if (buffer == NULL)
            {
                perror("malloc failed for buffer");
                close(fd);
                free(filename);
                exit(EXIT_MALLOC_FAILED);
            }

            int len = snprintf(buffer, 100, "Child PID: %d, Parent PID: %d\n", getpid(), getppid());
            if (write(fd, buffer, len) < 0)
            {
                perror("write failed");
                free(buffer);
                close(fd);
                free(filename);
                exit(EXIT_WRITE_FAILED);
            }

            close(fd);
            free(buffer);
            free(filename);
            exit(0);
        }
        else
        {
            child_pids[i] = pid;
        }
    }

    for (int i = 0; i < N; i++)
    {
        wait(NULL);
    }

    write(STDOUT_FILENO, "\nParent reading file contents:\n", 31);

    for (int i = 0; i < N; i++)
    {

        char *filename = (char *)malloc(50 * sizeof(char));
        if (filename == NULL)
        {
            perror("malloc failed for filename in parent");
            continue;
        }

        snprintf(filename, 50, "output_%d.txt", child_pids[i]);

        int fd = open(filename, O_RDONLY);
        if (fd < 0)
        {
            perror("Error opening child file");
            free(filename);
            continue;
        }

        off_t file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        char *buffer = (char *)malloc((file_size + 1) * sizeof(char));
        if (buffer == NULL)
        {
            perror("malloc failed for buffer in parent");
            close(fd);
            free(filename);
            continue;
        }

        memset(buffer, 0, file_size + 1);
        ssize_t bytes_read = read(fd, buffer, file_size);
        if (bytes_read < 0)
        {
            perror("Error reading file");
            close(fd);
            free(buffer);
            free(filename);
            continue;
        }

        buffer[bytes_read] = '\0';

        write(STDOUT_FILENO, buffer, bytes_read);
        write(STDOUT_FILENO, "\n", 1);

        close(fd);
        free(buffer);
        free(filename);
    }

    write(STDOUT_FILENO, "Parent Process: Finished reading all child files.\n", 50);
    free(child_pids);
    return 0;
}
