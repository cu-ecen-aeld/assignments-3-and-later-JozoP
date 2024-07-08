#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include "queue.h"
#include <sys/types.h>
#include <netdb.h>
#include <stdbool.h>
#include <syslog.h>
#include <fcntl.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define USE_AESD_CHAR_DEVICE 1

// Define the data file path
#if USE_AESD_CHAR_DEVICE
    #define DATA_FILE "/dev/aesdchar"
#else
    #define DATA_FILE "/var/tmp/aesdsocketdata"
#endif

// Declare global variables
int serverSocket;
FILE *filePointer;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t timestampThread;
struct aesd_seekto seekto;
int fd;

// Define the thread information structure
struct ThreadInfo
{
    pthread_t threadId;
    int clientSocket;
    bool threadComplete;
    SLIST_ENTRY( ThreadInfo ) entries;
};

// Define the thread list head
SLIST_HEAD( ThreadHead, ThreadInfo ) threadHead;

// Signal handler function
void signalHandler ( int sig )
{
    //  Check if the signal is SIGINT or SIGTERM
    if ( sig == SIGINT || sig == SIGTERM )
    {
        //  Log a message indicating the caught signal
        syslog( LOG_INFO, "Caught signal, exiting" );

        if ( serverSocket != -1 )
        {
            shutdown( serverSocket, SHUT_RDWR );
        }

        closelog();
        pthread_cancel( timestampThread );

        // Join all threads
        struct ThreadInfo *currentThread, *nextThread;
        SLIST_FOREACH_SAFE( currentThread, &threadHead, entries, nextThread )
        {
            pthread_join( currentThread->threadId, NULL );
            SLIST_REMOVE( &threadHead, currentThread, ThreadInfo, entries );
            free( currentThread );
        }
        exit( 0 );
    }
}

// Function to handle client connections
void *handleClient ( void *arg )
{
    struct ThreadInfo *threadInfo = ( struct ThreadInfo * ) arg;
    int clientSocket = threadInfo->clientSocket;
    struct sockaddr_in clientAddr;
    socklen_t addrSize = sizeof( clientAddr );
    
    if ( getpeername( clientSocket, ( struct sockaddr * )&clientAddr, &addrSize ) == -1 )
    {
        syslog( LOG_ERR, "Failed to get client address: %s", strerror( errno ) );
        close( clientSocket );
        threadInfo->threadComplete = true;
        pthread_exit( NULL );
    }

    char ipAddress[INET_ADDRSTRLEN];

    inet_ntop( AF_INET, &clientAddr.sin_addr, ipAddress, sizeof( ipAddress ) );

    syslog( LOG_INFO, "Accepted connection from %s", ipAddress );

    // Declare buffer to store received data
    char buffer[1024];
    ssize_t bytesReceived;
    bool cmd_found = false;
    fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0744);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file %s: %s", DATA_FILE, strerror(errno));
        pthread_mutex_unlock(&mutex);
        close(clientSocket);
        threadInfo->threadComplete = true;
        pthread_exit(NULL);
    }
    //  Receive data from the client
    while ( ( bytesReceived = recv( clientSocket, buffer, sizeof( buffer ), 0 ) ) > 0 )
    {
        
        char str_compare[] = "AESDCHAR_IOCSEEKTO:";
        cmd_found = false;
        unsigned int x = 0;
        unsigned int y = 0;
        if(strncmp(str_compare, buffer, 19) == 0)
        {
            int buf_idx;
            int y_idx;
            for(buf_idx = 19; (buffer[buf_idx] != '\0') && (buf_idx < sizeof(buffer)); buf_idx++)
            {
                cmd_found = false;
                if(buffer[buf_idx] >= '0' && buffer[buf_idx] <= '9')
                {
                    x = (buffer[buf_idx] - '0');
                }
                else if(buffer[buf_idx] == ',')
                {
                    cmd_found = true;
                    y_idx = buf_idx + 1;
                    break;
                }
                else
                {
                    break;
                }
                
            }
            if(cmd_found)
            {
                for(buf_idx = y_idx; (buffer[buf_idx] != '\0') && (buf_idx < sizeof(buffer)); buf_idx++)
                {
                    if(buffer[buf_idx] >= '0' && buffer[buf_idx] <= '9')
                    {
                        y = (buffer[buf_idx] - '0');
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        pthread_mutex_lock(&mutex);
        // Check if the received data is a command
        if(cmd_found)
        {
            
            seekto.write_cmd = x;
            seekto.write_cmd_offset = y;
            long result_ret = ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto);
            if(result_ret != 0)
            {
                syslog(LOG_ERR, "Failed IOCTL Result = %ld", result_ret);
                syslog( LOG_ERR, "Failed to perform seek operation: %s", strerror(errno));
            }

        }
        else
        {
            if ( write( fd, buffer, bytesReceived ) == -1 )
            {
                syslog( LOG_ERR, "Failed to write to file %s: %s", DATA_FILE, strerror( errno ) );
                close( fd );
                pthread_mutex_unlock( &mutex );
                close( clientSocket );
                threadInfo->threadComplete = true;
                pthread_exit( NULL );
            }
            close( fd );
        }
        pthread_mutex_unlock( &mutex );

        // Check if the received data contains a newline character
        if ( memchr( buffer, '\n', bytesReceived ) != NULL )
        {
            break;
        }            
        
    }

    pthread_mutex_lock( &mutex );
    // Open the file in read mode to send the content back to the client
    if(!cmd_found)
    {
        fd = open(DATA_FILE, O_RDWR);
        if (fd == -1) {
            // Handle error
            syslog(LOG_ERR, "Failed to open file %s: %s", DATA_FILE, strerror(errno));
            pthread_mutex_unlock(&mutex);
            close(clientSocket);
            threadInfo->threadComplete = true;
            pthread_exit(NULL);
        }
    }

    while ( ( bytesReceived = read( fd, buffer, sizeof( buffer ) ) ) > 0 )
    {
        send( clientSocket, buffer, bytesReceived, 0 );
    }
    close(fd);
       
    
    pthread_mutex_unlock( &mutex );

    close( clientSocket );

    syslog( LOG_INFO, "Closed connection from %s", ipAddress );
    threadInfo->threadComplete = true;
    pthread_exit( NULL );
}

void *appendTimestamp ( void *arg )
{
    struct timespec currentTime;
    struct tm timeInfo;
    char timestamp[128];

    while ( 1 )
    {
        // Get the current time
        if ( clock_gettime( CLOCK_REALTIME, &currentTime ) == -1 )
        {
            syslog( LOG_ERR, "Failed to get current time: %s", strerror( errno ) );
            pthread_exit( NULL );
        }

        if ( localtime_r( &currentTime.tv_sec, &timeInfo ) == NULL )
        {
            syslog( LOG_ERR, "Failed to convert time: %s", strerror( errno ) );
            pthread_exit( NULL );
        }

        strftime( timestamp, sizeof( timestamp ), "timestamp:%a, %d %b %Y %T %z", &timeInfo );

        pthread_mutex_lock( &mutex );
        filePointer = fopen( DATA_FILE, "a" );
        if ( filePointer == NULL )
        {
            syslog( LOG_ERR, "Failed to open file %s: %s", DATA_FILE, strerror( errno ) );
            pthread_mutex_unlock( &mutex );
            pthread_exit( NULL );
        }

        // Write timestamp to file
        size_t len = strlen( timestamp );
        if ( fwrite( timestamp, 1, len, filePointer ) != len )
        {
            syslog( LOG_ERR, "Failed to write timestamp to file" );
            fclose( filePointer );
            pthread_mutex_unlock( &mutex );
            pthread_exit( NULL );
        }

        if ( fwrite( "\n", 1, 1, filePointer ) != 1 )
        {
            syslog( LOG_ERR, "Failed to write newline character to file" );
            fclose( filePointer );
            pthread_mutex_unlock( &mutex );
            pthread_exit( NULL );
        }

        fclose( filePointer );
        pthread_mutex_unlock( &mutex );

        // Sleep for 10 seconds
        struct timespec sleepTime = {10, 0};
        nanosleep( &sleepTime, NULL );
    }
}

int main ( int argc, char *argv[] )
{
    openlog( "aesdsocket", LOG_PID | LOG_CONS, LOG_USER );

#if (USE_AESD_CHAR_DEVICE == 0)
    // Remove the data file if it exists
    if ( remove( DATA_FILE ) == -1 && errno != ENOENT )
    {
        syslog( LOG_ERR, "Failed to remove file %s: %s", DATA_FILE, strerror( errno ) );
        closelog();
        exit( -1 );
    }
#endif
    bool isDaemonMode = false;

    if ( argc > 1 && strcmp( argv[1], "-d" ) == 0 )
    {
        isDaemonMode = true;
    }

    if ( signal( SIGINT, signalHandler ) == SIG_ERR || signal( SIGTERM, signalHandler ) == SIG_ERR )
    {
        syslog( LOG_ERR, "Failed to register signal handler: %s", strerror( errno ) );
        closelog();
        exit( -1 );
    }

    SLIST_INIT( &threadHead );

    struct addrinfo hints, *serviceAddr, *p;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ( ( status = getaddrinfo( NULL, "9000", &hints, &serviceAddr ) ) != 0 )
    {
        syslog( LOG_ERR, "Failed to get address info: %s", strerror( errno ) );
        closelog();
        exit( -1 );
    }

    bool isBindingSuccessful = false;

    for ( p = serviceAddr; p != NULL; p = p->ai_next )
    {
        // Create a socket
        serverSocket = socket( p->ai_family, p->ai_socktype, p->ai_protocol );
        if ( serverSocket == -1 )
        {
            syslog( LOG_ERR, "Failed to create socket: %s", strerror( errno ) );
            continue;
        }

        if ( setsockopt( serverSocket, SOL_SOCKET, SO_REUSEADDR, &( int ){1}, sizeof( int ) ) == -1 )
        {
            syslog( LOG_ERR, "Failed to set socket options: %s", strerror( errno ) );
            close( serverSocket );
            continue;
        }

        if ( bind( serverSocket, p->ai_addr, p->ai_addrlen ) == -1 )
        {
            syslog( LOG_ERR, "Failed to bind: %s", strerror( errno ) );
            close( serverSocket );
            continue;
        }

        isBindingSuccessful = true;
        break;
    }

    freeaddrinfo( serviceAddr );

    if ( !isBindingSuccessful )
    {
        syslog( LOG_ERR, "Failed to bind to any address" );
        close( serverSocket );
        closelog();
        exit( -1 );
    }

    if ( isDaemonMode )
    {
        pid_t pid = fork();
        if ( pid == -1 )
        {
            close( serverSocket );
            closelog();
            exit( -1 );
        }
        else if ( pid != 0 )
        {
            close( serverSocket );
            closelog();
            exit( 0 );
        }
    }

    if ( listen( serverSocket, 10 ) == -1 )
    {
        syslog( LOG_ERR, "Failed to listen: %s", strerror( errno ) );
        closelog();
        exit( -1 );
    }

#if (USE_AESD_CHAR_DEVICE == 0)
    if ( pthread_create( &timestampThread, NULL, appendTimestamp, NULL ) != 0 )
    {
        syslog( LOG_ERR, "Failed to create timestamp thread" );
        closelog(  );
        exit( -1 );
    }
#endif

    while ( 1 )
    {
        // Accept client connection
        struct sockaddr_in clientAddr;
        socklen_t addrSize = sizeof( clientAddr );

        int clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientAddr, &addrSize );
        if ( clientSocket == -1 )
        {
            syslog( LOG_ERR, "Failed to accept: %s", strerror( errno ) );
            continue;
        }

        struct ThreadInfo *threadInfo = ( struct ThreadInfo * )malloc( sizeof( struct ThreadInfo ) );
        if ( threadInfo == NULL )
        {
            syslog( LOG_ERR, "Failed to allocate memory" );
            close( clientSocket );
            continue;
        }

        threadInfo->clientSocket = clientSocket;
        threadInfo->threadComplete = false;
        // Create thread to handle client
        if ( pthread_create( &threadInfo->threadId, NULL, handleClient, threadInfo ) != 0 )
        {
            syslog( LOG_ERR, "Failed to create client handling thread" );
            close( clientSocket );
            free( threadInfo );
            continue;
        }

        // Insert the thread info structure into the list
        SLIST_INSERT_HEAD( &threadHead, threadInfo, entries );

        // Join complete threads
        struct ThreadInfo *currentThread, *nextThread;
        SLIST_FOREACH_SAFE( currentThread, &threadHead, entries, nextThread )
        {
            if ( currentThread->threadComplete )
            {
                pthread_join( currentThread->threadId, NULL );
                SLIST_REMOVE( &threadHead, currentThread, ThreadInfo, entries );
                free( currentThread );
            }
        }
    }
}