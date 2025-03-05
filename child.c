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
    int len = snprintf(buf, sizeof(buf), "Child PID: %d, var: %d, Running Time: %ld sec\n", getpid(), var, now - start_time);
    write(STDOUT_FILENO, buf, len);
    alarm(10);
}

int main()
{
    start_time = time(NULL);

    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGALRM, report_status);

    alarm(10);

    while (1)
    {
        pause();
    }

    return 0;
}
