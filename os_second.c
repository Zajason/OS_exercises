/*2 vdomades
argc = 2
N = atoi(argv[1])
2 terminal
pinaka apo paidia me malloc
for loop me fork
perimeno 10 sec me signals
sigalarm
xrhsimopoiw global variable
kathe paidi lambanei 4 signal
waitpid anti gia wait gia na katalavaineis to status
waitpid(pid, &status, 0)
prosoxi gia loop oti an termatisiei apo ton patera den to xanadimiourgo mono an to termatisi kapios apo xristis
kill(pid, SIGNAL)
while(true) sta paidia
an termatiso en paidi kai to xanaftiaxei o pateras tha prepi na ektiposo to neo pid
to neo paidi den tha simpipti sto countdown me ta palia
stin child.c tha valome ton kodika toy pediou tha to kanoume compile kai me tin execv trexoyme to ektelesimo mesa ston kodika
giata 10 sec tha valoyme allo ena alarm ston handler toy sigalarm kai ena sto idio to paidi
kai etsi ginetai mia loupa
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_EXECV_FAILED 4
#define EXIT_WRITE_FAILED 5

int n;
pid_t *children;

void write_message(const char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_child_exit(int sig)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        char buf[100];
        int len = snprintf(buf, 100, "Child %d terminated. Restarting...\n", pid);
        write(STDOUT_FILENO, buf, len);

        for (int i = 0; i < n; i++)
        {
            if (children[i] == pid)
            {
                pid_t new_pid = fork();
                if (new_pid == 0)
                {
                    char *args[] = {"./child", NULL};
                    execv(args[0], args);
                    exit(EXIT_EXECV_FAILED);
                }
                else
                {
                    children[i] = new_pid;
                }
            }
        }
    }
}

void handle_sigterm(int sig)
{
    write_message("Received SIGTERM. Terminating...\n");

    // Ignore SIGCHLD temporarily to prevent respawning children
    signal(SIGCHLD, SIG_IGN);

    for (int i = 0; i < n; i++)
    {
        if (children[i] > 0)
        {
            kill(children[i], SIGTERM);
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
            kill(children[i], sig);
        }
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
        perror("malloc");
        exit(EXIT_MALLOC_FAILED);
    }

    signal(SIGCHLD, handle_child_exit);
    signal(SIGTERM, handle_sigterm);
    signal(SIGUSR1, forward_signal);
    signal(SIGUSR2, forward_signal);

    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            char *args[] = {"./child", NULL};
            execv(args[0], args);
            exit(EXIT_EXECV_FAILED);
        }
        else if (pid > 0)
        {
            children[i] = pid;
        }
        else
        {
            perror("fork");
            free(children);
            exit(EXIT_FORK_FAILED);
        }
    }

    while (1)
    {
        pause();
    }
    return 0;
}
