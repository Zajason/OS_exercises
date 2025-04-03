#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_WRITE_FAILED 4

int main(int argc, char *argv[]){
    if(argc < 2 || argc > 3){
        write(STDERR_FILENO, "Error: Missing or too many arguments.\n", 38);
        write(STDERR_FILENO, "Example: ./parent 5 [--round-robin|--random]\n", 45);
        exit(EXIT_INVALID_ARGS);
    }
    int n = atoi(argv[1]);
    if(n <= 0){
        write(STDERR_FILENO, "N must be a positive integer.\n", 30);
        exit(EXIT_INVALID_ARGS);
    }
    int mode = 0; // default: round-robin
    if(argc == 3){
        if(strcmp(argv[2], "--round-robin") == 0){
            mode = 0;
        }
        else if(strcmp(argv[2], "--random") == 0){
            mode = 1;
        }
        else{
            write(STDERR_FILENO, "Error: Invalid mode. The second argument should be --round-robin or --random.\n", 102);
            exit(EXIT_INVALID_ARGS);
        }
    }
    
    int *children = malloc(n * sizeof(int));
    if(children == NULL){
        perror("malloc failed");
        exit(EXIT_MALLOC_FAILED);
    }
    
    // Allocate two arrays for the pipes:
    // pipe_pc: pipes from Parent to Child (for sending jobs)
    // pipe_cp: pipes from Child to Parent (for receiving results)
    int (*pipe_pc)[2] = malloc(n * sizeof(int[2]));
    int (*pipe_cp)[2] = malloc(n * sizeof(int[2]));
    if(pipe_pc == NULL || pipe_cp == NULL){
        perror("malloc failed for pipes");
        free(children);
        exit(EXIT_MALLOC_FAILED);
    }
    
    // Initialize random seed if in random mode.
    if(mode == 1) {
         srand(time(NULL));
    }
    
    // Create the pipes and fork each child.
    for(int i = 0; i < n; i++){
        if(pipe(pipe_pc[i]) == -1 || pipe(pipe_cp[i]) == -1){
            perror("pipe failed");
            free(children);
            free(pipe_pc);
            free(pipe_cp);
            exit(EXIT_WRITE_FAILED);
        }
        
        pid_t pid = fork();
        if(pid == 0){
            // Child process:
            close(pipe_pc[i][1]); // Child only reads from parent-to-child.
            close(pipe_cp[i][0]); // Child only writes to child-to-parent.
            int num;
            while(1){
                ssize_t r = read(pipe_pc[i][0], &num, sizeof(num));
                if(r <= 0){
                    // If pipe is closed or error occurs, exit the loop.
                    break;
                }
                // Process the job (e.g., double the number), wait 5 seconds...
                num *= 2;
                sleep(5);
                if(write(pipe_cp[i][1], &num, sizeof(num)) == -1){
                    perror("child write failed");
                    break;
                }
            }
            close(pipe_pc[i][0]);
            close(pipe_cp[i][1]);
            exit(0);
        }
        else if(pid > 0){
            // Parent process:
            children[i] = pid;
            close(pipe_pc[i][0]); // Parent writes to this pipe.
            close(pipe_cp[i][1]); // Parent reads from this pipe.
        }
        else{
            perror("fork failed");
            free(children);
            free(pipe_pc);
            free(pipe_cp);
            exit(EXIT_FORK_FAILED);
        }
    }
    
    // Parent main loop: use select() to monitor terminal input and children responses.
    while(1){
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        // Add STDIN.
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = STDIN_FILENO;
        
        // Add all child-to-parent pipe read ends.
        for(int i = 0; i < n; i++){
            FD_SET(pipe_cp[i][0], &read_fds);
            if(pipe_cp[i][0] > max_fd)
                max_fd = pipe_cp[i][0];
        }
        
        // Block until one or more file descriptors are ready.
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if(ready == -1){
            if(errno == EINTR)
                continue;
            perror("select failed");
            break;
        }
        
        // Check for terminal input.
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char input[128];
            ssize_t count = read(STDIN_FILENO, input, sizeof(input) - 1);
            if(count <= 0){
                continue;
            }
            input[count] = '\0';
            // Remove trailing newline.
            input[strcspn(input, "\n")] = '\0';
            
            if(strcmp(input, "help") == 0){
                write(STDOUT_FILENO, "Type a number to send job to a child!\n", 38);
            }
            else if(strcmp(input, "exit") == 0){
                write(STDOUT_FILENO, "All children terminated.\n", 25);
                // Optionally send termination signals to children.
                for(int i = 0; i < n; i++){
                    kill(children[i], SIGTERM);
                    close(pipe_pc[i][1]);
                    close(pipe_cp[i][0]);
                }
                break;
            }
            else{
                // Check if input is a number.
                int job = atoi(input);
                if(job == 0 && strcmp(input, "0") != 0){
                    write(STDOUT_FILENO, "Type a number to send job to a child!\n", 38);
                } else {
                    static int rr_index = 0;
                    int chosen;
                    if(mode == 0){
                        // Round-robin: assign job sequentially.
                        chosen = rr_index;
                        rr_index = (rr_index + 1) % n;
                    } else {
                        // Random: choose a random child.
                        chosen = rand() % n;
                    }
                    if(write(pipe_pc[chosen][1], &job, sizeof(job)) == -1){
                        perror("write to child failed");
                    } else {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "[Parent] Assigned %d to child %d\n", job, children[chosen]);
                        write(STDOUT_FILENO, msg, strlen(msg));
                    }
                }
            }
        }
        
        // Check if any child has sent back a result.
        for(int i = 0; i < n; i++){
            if(FD_ISSET(pipe_cp[i][0], &read_fds)){
                int result;
                ssize_t r = read(pipe_cp[i][0], &result, sizeof(result));
                if(r > 0){
                    char msg[128];
                    snprintf(msg, sizeof(msg), "[Parent] Received result %d from child %d\n", result, children[i]);
                    write(STDOUT_FILENO, msg, strlen(msg));
                }
            }
        }
    }
    
    free(children);
    free(pipe_pc);
    free(pipe_cp);
    return 0;