#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/stat.h>

#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE (32*1024)

void sig_handler(int);

int sockfd, connectionfd;
FILE *file;


int main(int argc, char *argv[]) 
{
    const int PORT = 9000;
    //const int BUFFER_SIZE = 32*1024;

    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    char client_ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE] = {0};
    size_t bytes_read;

    int daemon_flag = 0;

    // Open syslog
    openlog("aesdsocket", LOG_PID|LOG_CONS, LOG_USER);

    // Parse command line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "-d") == 0) {
            daemon_flag = 1;
        } else {
            syslog(LOG_ERR, "Invalid argument %s", argv[1]);
            return -1;
        }
    }

    // Set up signal handling
    struct sigaction act = {0};
    act.sa_handler = sig_handler;
    act.sa_flags = 0;
    if(sigaction(SIGINT, &act, NULL) < 0) {
        syslog(LOG_ERR, "Error setting up SIGINT action.");
        return -1;
    }
    if(sigaction(SIGTERM, &act, NULL) < 0) {
        syslog(LOG_ERR, "Error setting up SIGTERM action.");
        return -1;
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Error creating socket.");
        return -1;
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        syslog(LOG_ERR, "Error setting socket options.");
        close(sockfd);
        return -1;
    }

    // Bind socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "Error binding socket.");
        close(sockfd);
        return -1;
    }

    // Daemonize if flag is set
    if(daemon_flag) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_ERR, "Error daemonizing.");
            close(sockfd);
            return -1;
        }

        // Change working directory
        if(chdir("/") < 0) {
            syslog(LOG_ERR, "Error changing working directory.");
            close(sockfd);
            return -1;
        }

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Open log file
        openlog("aesdsocket", LOG_PID|LOG_CONS, LOG_USER);        
    }

    // Listen for incoming connections
    if(listen(sockfd, 5) < 0) {
        syslog(LOG_ERR, "Error listening for incoming connections.");
        close(sockfd);
        return -1;
    }

    syslog(LOG_INFO, "Listening for incoming connections on port %d", PORT);

    for(;;){
        // Accept incoming connection
        connectionfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);

        if(connectionfd < 0) {
            syslog(LOG_ERR, "Error accepting incoming connection.");
            close(sockfd);
            return -1;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted incoming connection from %s", client_ip);

        // Open file to append received data, create if it doesn't exist
        file = fopen(FILEPATH, "a+");
        if(file == NULL) {
            syslog(LOG_ERR, "Error opening file.");
            close(connectionfd);
            close(sockfd);
            return -1;
        }

        // Read data from socket and write to file
        while((bytes_read = read(connectionfd, buffer, BUFFER_SIZE)) > 0) {
            fwrite(buffer, 1, bytes_read, file);

            if(ferror(file)) {
                syslog(LOG_ERR, "Error writing to file.");
                close(connectionfd);
                close(sockfd);
                fclose(file);
                return -1;
            }

            // Check for newline character
            char* nptr = strchr(buffer, '\n');
            if(nptr != NULL) {
                //send content of file to client
                fseek(file, 0, SEEK_SET);
                while((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    write(connectionfd, buffer, bytes_read);
                }

                if(ferror(file)) {
                    syslog(LOG_ERR, "Error sending back from file to client.");
                    close(connectionfd);
                    close(sockfd);
                    fclose(file);
                    return -1;
                }
            }
        }

        // Close file and new socket
        if(file != NULL){
            fclose(file);
            file = NULL;
        }
        if(connectionfd > 0){
            close(connectionfd);
            connectionfd = 0;
        }
    }
    
    // Clean up
    closelog();
    if(sockfd > 0) {
        close(sockfd);
        sockfd = 0;
    }
    if(file != NULL) {
        fclose(file);
        file = NULL;
    }

    remove(FILEPATH);
    
    return 0;
}

void sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Received signal %d, exiting.", signo);

        if(connectionfd > 0) {
            close(connectionfd);
        }

        if(sockfd > 0) {
            close(sockfd);
        }

        if(file != NULL) {
            fclose(file);
            file = NULL;
        }
        
        remove(FILEPATH);
        closelog();
        exit(0);
    }
}