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

#define EXIT_INVALID_ARGS 1
#define EXIT_MALLOC_FAILED 2
#define EXIT_FORK_FAILED 3
#define EXIT_WRITE_FAILED 4

#define DEFAULT_HOST "os4.cloud.dslab.ece.ntua.gr"
#define DEFAULT_PORT 41312
#define DEFAULT_BUFFER_SIZE 1024

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
            fprintf(stderr, "Usage: %s [--host HOST] [--port PORT] [--debug]\n", argv[0]);
            exit(EXIT_INVALID_ARGS);
        }
    }

    if(debug){
        printf("connecting to %s on port %d\n", host, port);
    }
    
    struct hostent *server = gethostbyname(host);
    if(server == NULL){
        fprintf(stderr, "Error resolving host\n");
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
        printf("Connected to server\n");
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
            if(strcmp(buffer, "exit\n") == 0){
                printf("Exiting...\n");
                break;
            }
            if(strcmp(buffer, "help\n") == 0){
                printf("Available commands: \n");
                printf("1. help - Show this help message\n");
                printf("2. exit - Exit the program\n");
                printf("3. get - Get data from the server\n");
                printf("4. N name surname reason - Exit request\n");
                continue;
            }
            buffer[strcspn(buffer, "\n")] = '\0';    
            if(write(sockfd, buffer, strlen(buffer)) < 0){
                perror("write to server failed");
                break;
            }
            if (debug) printf("Sent: %s\n", buffer);
        }
        if(fds[1].revents & POLLIN){
            int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                if (bytes == 0) printf("Server closed the connection.\n");
                else perror("recv failed");
                break;
            }
            buffer[bytes] = '\0';
            if (debug) printf("Received: %s\n", buffer);

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

            if (token != NULL) {
                interval = atoi(token);
                token = strtok(NULL, " ");
                if (token != NULL) {
                    light = atoi(token);
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        temp = atoi(token) / 100.0f;
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                            timestamp = atol(token);
                            tm_info = localtime(&timestamp);
                            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
                            printf("Latest event:\n");
                            printf("Interval (%d)\n", interval);
                            printf("Temperature is: %.2f\n", temp);
                            printf("Light level is: %d\n", light);
                            printf("Timestamp is: %s\n", time_str);
                            is_event = true;
                        }
                    }
                }
            }

            if (!is_event) {
                printf("Server: %s\n", copy_buf);
                if (write(sockfd, copy_buf, strlen(copy_buf)) < 0) {
                    perror("write echo failed");
                    break;
                }
                if (debug) printf("Echoed: %s\n", copy_buf);
                struct pollfd ackfd = { .fd = sockfd, .events = POLLIN };
                if (poll(&ackfd, 1, 3000) > 0 && (ackfd.revents & POLLIN)) {
                    int len = recv(sockfd, copy_buf, sizeof(copy_buf) - 1, 0);
                    if (len > 0) {
                        copy_buf[len] = '\0';
                        printf("ACK from server: %s\n", copy_buf);
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
