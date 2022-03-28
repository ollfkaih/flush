#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <dirent.h>

#define MAX_INPUT 128
#define MAX_PATH 1024
#define MAX_ARGS 10

void inputPrompt(void);
void handleInput(char input[MAX_ARGS][MAX_PATH]);

char currentDirectory[MAX_PATH] = "";

int main(void) {
    printf("Welcome to flush\n");
    memset(&currentDirectory, 0, sizeof(currentDirectory));

    while(1) {
        getwd(currentDirectory);
        inputPrompt();
    }
}

void inputPrompt(void) {
    printf("[Flush] %s: ", currentDirectory);
    char buffer[MAX_INPUT];
    fgets(buffer, MAX_INPUT, stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    char input[MAX_ARGS][MAX_PATH];
    memset(&input, 0, sizeof(input));

    char delim[] = " "; // TODO: add tabs
    
    char *ptr = strtok(buffer, delim);

    for (int i = 0; ptr != NULL && i < MAX_ARGS; i++) {
        strcpy(&input[i][0], ptr);
        ptr = strtok(NULL, delim);
    }

    handleInput(input);
}

void handleInput(char input[MAX_ARGS][MAX_PATH]) {
    printf("handling the following input: [%s", input[0]);
    for (int i = 1; i < MAX_ARGS; i++) {
        //print only non-empty strings in input
        if (strlen(input[i]) > 0) {
            printf(", %s", input[i]);
        } 
    }
    printf("]\n");

    if (strcmp(input[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(0);
    } 

    pid_t pid = fork();
    int status;

    if (pid < 0) {
        printf("Error: unable to fork\n");
    }
    if (pid == 0) {
        if (strcmp(input[0], "ls") == 0) {
                DIR *directory;
                struct dirent *ep;
                if (input[1][0] == '\0') {
                    // ls was not passed any args
                    directory = opendir("./");
                } else {
                    directory = opendir(&input[1][0]);
                }

                if (directory != NULL) {
                    while ((ep = readdir (directory))) {
                        puts (ep->d_name);
                    }
                    (void) closedir(directory);
                } else {
                    printf("Unable to open dir\n");
                }
                exit(0);
                // _exit(EXIT_SUCCESS);
            }
         else if (strcmp(input[0], "cd") == 0) {
            if (strlen(input[1]) > 0) {
                if (chdir(&input[1][0]) == 0) {
                    printf("Changed directory to %s\n", &input[1][0]);
                    exit(0);
                } else {
                    printf("Unable to change directory to %s\n", &input[1][0]);
                    exit(1);
                }
            } else {
                // change to home dir (not required)
                if (chdir(getenv("HOME")) == 0) {
                    printf("Changed directory to %s\n", getenv("HOME"));
                    exit(0);
                }
            }
        } else if (strcmp(input[0], "pwd") == 0) {
            printf("%s\n", currentDirectory);
            exit(0);
        } else if (strcmp(input[0], "clear") == 0 || strcmp(input[0], "cls") == 0) {
            printf("\033[2J\033[1;1H");
            exit(0);
        }
        else if (strcmp(input[0], "help") == 0) {
            printf("ls - list files in current directory\n");
            printf("cd - change directory\n");
            printf("pwd - print current directory\n");
            printf("clear - clear screen\n");
            printf("exit - exit program\n");
        } /*else {
          printf("Unrecognized command\n");
        }*/
    } else if (pid > 0) {
        waitpid(-1, &status, 0);

        if ( WIFEXITED(status) ) {
            int es = WEXITSTATUS(status);
            printf("Exit status [%s ...] was %d\n", input[0], es);
        }
    }
}
