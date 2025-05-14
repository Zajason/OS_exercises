#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_WRITE_FAILED 4

#define DEFAULT_HOST "os4.cloud.dslab.ece.ntua.gr"
#define DEFAULT_PORT 41312
#define DEFAULT_BUFFER_SIZE 1024

/**
 * printw: formatted output using write()
 * Behaves like printf but writes directly to STDOUT_FILENO.
 */
static void printw(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char outbuf[DEFAULT_BUFFER_SIZE];
    int len = vsnprintf(outbuf, sizeof(outbuf), fmt, args);
    va_end(args);
    if (len > 0) {
        if (write(STDOUT_FILENO, outbuf, len) < 0) {
            perror("write to stdout failed");
            exit(EXIT_WRITE_FAILED);
        }
    }
}

/**
 * is_number: checks if string consists solely of digits
 */
static bool is_number(const char *s) {
    if (s == NULL || *s == '\0') return false;
    for (const char *p = s; *p; ++p) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

int main(int argc, char *argv[]){ 
    char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    bool debug = false;
    
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--host") == 0 && i + 1 < argc){
            host = argv[++i];
        }
        else if(strcmp(argv[i], "--port") == 0 && i + 1 < argc){
            port = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "--debug") == 0){
            debug = true;
        }
        else{
            const char *usage = "Usage: %s [--host HOST] [--port PORT] [--debug]\n";
            char msg[256];
            snprintf(msg, sizeof(msg), usage, argv[0]);
            write(STDERR_FILENO, msg, strlen(msg));
            exit(EXIT_INVALID_ARGS);
        }
    }

    if(debug){
        printw("connecting to %s on port %d\n", host, port);
    }
    
    struct hostent *server = gethostbyname(host);
    if(server == NULL){
        const char *err = "Error resolving host\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(EXIT_INVALID_ARGS);
    }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        exit(EXIT_INVALID_ARGS);
    }
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        perror("Connection to server failed");
        close(sockfd);
        exit(EXIT_INVALID_ARGS);
    }
    if(debug){
        printw("Connected to server\n");
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    while(1){
        char buffer[DEFAULT_BUFFER_SIZE];
        int ret = poll(fds, 2, -1);
        if(ret == -1){
            perror("poll");
            break;
        }

        if(fds[0].revents & POLLIN){
            ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if(count <= 0){
                perror("read");
                break;
            }
            buffer[count] = '\0';

            // exit command
            if(strcmp(buffer, "exit\n") == 0){
                printw("Exiting...\n");
                break;
            }
            // help command
            if(strcmp(buffer, "help\n") == 0){
                printw("Available commands:\n");
                printw("1. help - Show this help message\n");
                printw("2. exit - Exit the program\n");
                printw("3. get - Get data from the server\n");
                printw("4. N name surname reason - Exit request\n");
                continue;
            }

            // strip newline
            buffer[strcspn(buffer, "\n")] = '\0';

            // get command
            if(strcmp(buffer, "get") == 0){
                if(write(sockfd, buffer, strlen(buffer)) < 0){
                    perror("write to server failed");
                    break;
                }
                if(debug) printw("Sent: %s\n", buffer);
                continue;
            }

            // check for N name surname reason format
            char tmp[DEFAULT_BUFFER_SIZE];
            strncpy(tmp, buffer, sizeof(tmp));
            tmp[sizeof(tmp)-1] = '\0';
            char *tokens[4];
            int tcount = 0;
            char *tok = strtok(tmp, " ");
            while(tok != NULL && tcount < 4){
                tokens[tcount++] = tok;
                tok = strtok(NULL, " ");
            }
            if(tcount == 4 && is_number(tokens[0])){
                if(write(sockfd, buffer, strlen(buffer)) < 0){
                    perror("write to server failed");
                    break;
                }
                if(debug) printw("Sent: %s\n", buffer);
            } else {
                printw("try again\n");
            }
            continue;
        }

        if(fds[1].revents & POLLIN){
            int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if(bytes <= 0) {
                if(bytes == 0) printw("Server closed the connection.\n");
                else perror("recv failed");
                break;
            }
            buffer[bytes] = '\0';
            if(debug) printw("Received: %s\n", buffer);

            char copy_buf[DEFAULT_BUFFER_SIZE];
            memcpy(copy_buf, buffer, bytes + 1);

            // parse sensor event
            char *token = strtok(buffer, " ");
            int interval, light;
            float temp;
            time_t timestamp;
            struct tm *tm_info;
            char time_str[64];
            bool is_event = false;

            if(token != NULL) {
                interval = atoi(token);
                token = strtok(NULL, " ");
                if(token != NULL) {
                    light = atoi(token);
                    token = strtok(NULL, " ");
                    if(token != NULL) {
                        temp = atoi(token) / 100.0f;
                        token = strtok(NULL, " ");
                        if(token != NULL) {
                            timestamp = atol(token);
                            tm_info = localtime(&timestamp);
                            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
                            printw("Latest event:\n");
                            printw("Interval (%d)\n", interval);
                            printw("Temperature is: %.2f\n", temp);
                            printw("Light level is: %d\n", light);
                            printw("Timestamp is: %s\n", time_str);
                            is_event = true;
                        }
                    }
                }
            }

            if(!is_event) {
                printw("Server: %s\n", copy_buf);
                if(write(sockfd, copy_buf, strlen(copy_buf)) < 0) {
                    perror("write echo failed");
                    break;
                }
                if(debug) printw("Echoed: %s\n", copy_buf);
                struct pollfd ackfd = { .fd = sockfd, .events = POLLIN };
                if(poll(&ackfd, 1, 3000) > 0 && (ackfd.revents & POLLIN)) {
                    int len = recv(sockfd, copy_buf, sizeof(copy_buf) - 1, 0);
                    if(len > 0) {
                        copy_buf[len] = '\0';
                        printw("ACK from server: %s\n", copy_buf);
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
