#include <stdio.h>
#include <syslog.h>

//2 arguments
//first argument is path to the file
//second argument is the string to write to the file

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <file> <string>\n", argv[0]);
        return 1;
    }

    openlog("coursera-writer", LOG_PID, LOG_USER);
    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);


    FILE *file;
    file = fopen(argv[1], "w");
    if (file == NULL) {
        perror("fopen");
        syslog(LOG_ERR, "Failed to open file %s", argv[1]);
        return 1;
    }

    if (fputs(argv[2], file) == EOF) {
        perror("fputs");
        syslog(LOG_ERR, "Failed to write to file %s", argv[1]);
        return 1;
    }

    fclose(file);
    return 0;
}

