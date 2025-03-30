/*
2 vdomades 9/4
pipes
to pipe einai oura FIFO
oti diavazoume fevgei apo to pipe
to pipe einai blocking , an den exei tipota grammeno perimenoume
den mporeis na exasfaliseis poios kanei read afou kaneis write 
entoli pipe, epistrefei perigrafes ton dio akron 
mporo na epistrepso 2 pragmata se ena function me dikti (call by reference)
int pd[2]; pipe(pd);
pd[0] proto akro , pd[1] deytero akro
prepei pipe prin to fork
kleinoume to akro poy den xrisimopio
2n dioxeteuseis , mia gia paidi -> pateras , pateras -> paidi 
disdiastato pinaka me 2n me malloc gia tis dioxeteuseis anti git n pinakes ton 2
mazi me pliktrologio o pateras perimenei input apo n+1 kai prepei na mi mplokarei
ayto linetai me select() , tis dinetai ena sinolo apo perigrafites kai leei ston parent pou na kanei
random me rand()
*/
#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_WRITE_FAILED 4



#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



int main(int argc ,char  *argv[]){
    if(argc <2 || argc >3){
        write(STDERR_FILENO, "Error: Missing or too many arguments.\n", 38);
        write(STDERR_FILENO, "Example: ./parent 5\n", 21);
        exit(EXIT_INVALID_ARGS);
    }
    int n = atoi(argv[1]);
    if (n <= 0)
    {
        write(STDERR_FILENO, "N must be a positive integer.\n", 30);
        exit(EXIT_INVALID_ARGS);
    }
    int *children = malloc(n * sizeof(int));
    if (children == NULL)
    {
        perror("malloc failed");
        exit(EXIT_MALLOC_FAILED);
    }
    int mode = 0;
    if(argc == 3){
        if(strcmp(argv[2], "--round-robin")==0){
            mode =0;
        }
        else if(strcmp(argv[2], "--random")==0){
            mode = 1;
        }
        else{
            write(STDERR_FILENO, "Error: Invalid mode.The second argument should be --round-robin , --random or left blank\n", 21);
            exit(EXIT_INVALID_ARGS);
        }
    }
for(int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
         
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
    while(1){
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