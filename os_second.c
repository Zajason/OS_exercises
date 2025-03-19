#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_EXECV_FAILED 4
#define EXIT_WRITE_FAILED 5

int n;
pid_t *children;

void write_message(const char *msg)
{
    ssize_t len = strlen(msg);
    if (write(STDOUT_FILENO, msg, len) != len)
    {
        perror("write failed");
        exit(EXIT_WRITE_FAILED);
    }
}

void handle_child_exit(int sig)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        char buf[100];
        int len = snprintf(buf, sizeof(buf), "Child %d terminated. Restarting...\n", pid);
        if (len < 0 || len >= sizeof(buf))
        {
            write_message("Error formatting message.\n");
        }
        else
        {
            write_message(buf);
        }

        for (int i = 0; i < n; i++)
        {
            if (children[i] == pid)
            {
                pid_t new_pid = fork();
                if (new_pid == 0)
                {
                    char *args[] = {"./child", NULL};
                    execv(args[0], args);
                    perror("execv failed");
                    exit(EXIT_EXECV_FAILED);
                }
                else if (new_pid > 0)
                {
                    children[i] = new_pid;
                }
                else
                {
                    perror("fork failed");
                }
            }
        }
    }

    if (pid == -1 && errno != ECHILD)
    {
        perror("waitpid failed");
    }
}

void handle_sigterm(int sig)
{
    write_message("Received SIGTERM. Terminating...\n");

    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    sigaction(SIGCHLD, &sa_ignore, NULL);

    for (int i = 0; i < n; i++)
    {
        if (children[i] > 0)
        {
            if (kill(children[i], SIGTERM) == -1)
            {
                perror("kill failed");
            }
        }
    }

    while (wait(NULL) > 0)
        ;

    write_message("All children terminated. Exiting...\n");

    free(children);
    exit(EXIT_SUCCESS);
}

void forward_signal(int sig)
{
    for (int i = 0; i < n; i++)
    {
        if (children[i] > 0)
        {
            if (kill(children[i], sig) == -1)
            {
                perror("kill failed");
            }
        }
    }
}

void setup_signal_handler(int sig, void (*handler)(int))
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(sig, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        write(STDERR_FILENO, "Error: Missing or too many arguments.\n", 38);
        write(STDERR_FILENO, "Example: ./parent 5\n", 21);
        exit(EXIT_INVALID_ARGS);
    }

    n = atoi(argv[1]);
    if (n <= 0)
    {
        write(STDERR_FILENO, "N must be a positive integer.\n", 30);
        exit(EXIT_INVALID_ARGS);
    }

    children = malloc(n * sizeof(pid_t));
    if (children == NULL)
    {
        perror("malloc failed");
        exit(EXIT_MALLOC_FAILED);
    }

    setup_signal_handler(SIGCHLD, handle_child_exit);
    setup_signal_handler(SIGTERM, handle_sigterm);
    setup_signal_handler(SIGUSR1, forward_signal);
    setup_signal_handler(SIGUSR2, forward_signal);

    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            char *args[] = {"./child", NULL};
            execv(args[0], args);
            perror("execv failed");
            exit(EXIT_EXECV_FAILED);
        }
        else if (pid > 0)
        {
            children[i] = pid;
        }
        else
        {
            perror("fork failed");
            free(children);
            exit(EXIT_FORK_FAILED);
        }
    }

    while (1)
    {
        int ret = pause();
        if (ret == -1 && errno != EINTR)
        {
            perror("pause failed");
            break;
        }
    }

    free(children);
    return 0;
}
