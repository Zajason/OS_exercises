#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

volatile sig_atomic_t var = 0;
time_t start_time;

void write_message(const char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_signal(int sig)
{
    if (sig == SIGUSR1)
    {
        var++;
    }
    else if (sig == SIGUSR2)
    {
        var--;
    }
    else if (sig == SIGTERM)
    {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "Child %d terminating...\n", getpid());
        write(STDOUT_FILENO, buf, len);
        _exit(0);
    }
}

void report_status(int sig)
{
    time_t now = time(NULL);
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "Parent PID: %d Child PID: %d, var: %d, Running Time: %ld sec\n", getppid(), getpid(), var, now - start_time);
    write(STDOUT_FILENO, buf, len);
    alarm(10);
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

int main()
{
    start_time = time(NULL);

    setup_signal_handler(SIGUSR1, handle_signal);
    setup_signal_handler(SIGUSR2, handle_signal);
    setup_signal_handler(SIGTERM, handle_signal);
    setup_signal_handler(SIGALRM, report_status);

    alarm(10);

    while (1)
    {
        pause();
    }

    return 0;
}
