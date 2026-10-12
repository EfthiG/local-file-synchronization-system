#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define MAX_CMD_LEN 256
#define PATH_MAX 4096

//FIFO file paths for communication with the manager
#define FIFO_IN "fss_in"
#define FIFO_OUT "fss_out"

//Logs a command to the console logfile with a timestamp
void command_to_logfile(FILE *log_fp, const char *cmd_line) {
    time_t current_time = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", localtime(&current_time));

    //Parse the command and its arguments
    char cmd[32] = "", arg1[PATH_MAX] = "", arg2[PATH_MAX] = "";
    sscanf(cmd_line, "%31s %4095s %4095[^\n]", cmd, arg1, arg2);

    //Log according to command type
    if (strcmp(cmd, "add") == 0 && *arg1 && *arg2) {
        fprintf(log_fp, "%s Command add %s -> %s\n", timestamp, arg1, arg2);
    } else if ((strcmp(cmd, "status") == 0 || strcmp(cmd, "cancel") == 0 || strcmp(cmd, "sync") == 0) && *arg1) {
        fprintf(log_fp, "%s Command %s %s\n", timestamp, cmd, arg1);
    } else if (strcmp(cmd, "shutdown") == 0) {
        fprintf(log_fp, "%s Command shutdown\n", timestamp);
    }

    fflush(log_fp); //Log has to be written to disk immediately
}

//Sends a command to the manager and captures its response
void communicate(const char *cmd, char *reply, int reply_size, FILE *log_fp) {
    //Open FIFO_IN for writing commands to the manager
    int fd_out = open(FIFO_IN, O_WRONLY);
    if (fd_out < 0) {
        perror("open FIFO_IN");
        return;
    }

    write(fd_out, cmd, strlen(cmd));
    close(fd_out);

    //Open FIFO_OUT to read the manager’s response
    int fd_in = open(FIFO_OUT, O_RDONLY);
    if (fd_in < 0) {
        perror("open FIFO_OUT");
        return;
    }

    //Read the response and store it in the buffer
    int n = read(fd_in, reply, reply_size - 1);
    if (n > 0) {
        reply[n] = '\0'; //Null-terminate the response string
    } else {
        strcpy(reply, "Manager gave no response");
    }

    close(fd_in);

    //Log the response with a timestamp
    time_t current_time = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", localtime(&current_time));
    fprintf(log_fp, "%s Response: %s\n", timestamp, reply);
    fflush(log_fp);
}

int main(int argc, char *argv[]) {
    //Check if the arguments are valid: expecting "-l <logfile>"
    if (argc != 3 || strcmp(argv[1], "-l") != 0) {
        fprintf(stderr, "Usage: %s -l <console-logfile>\n", argv[0]);
        return 1;
    }

    //Open the logfile in append mode
    FILE *log_fp = fopen(argv[2], "a");
    if (!log_fp) {
        perror("Log file");
        return 1;
    }

    char input[MAX_CMD_LEN], response[4096];

    //Interactive command loop
    while (printf("> "), fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\n")] = '\0';  //Strip trailing newline

        if (strlen(input) == 0){
            continue; //Ignore empty input
        }

        //Log the command
        command_to_logfile(log_fp, input);

        //Send command to manager and print the response
        communicate(input, response, sizeof(response), log_fp);
        printf("%s\n", response);

        //Exit if the user requested shutdown
        if (strncmp(input, "shutdown", 8) == 0) {
            break;
        }
    }

    fclose(log_fp); //Clean up
    return 0;
}