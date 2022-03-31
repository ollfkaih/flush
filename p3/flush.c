#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#define MAX_INPUT_LENGTH 128
#define MAX_PATH 1024
#define MAX_ARGS 10

void inputPrompt(void);
void handleInput(char input[MAX_ARGS][MAX_PATH]);
void clearInputBuffer(void);

char currentDirectory[MAX_PATH] = "";

int main(void) {
    printf(" ___________________\n< Welcome to Flush >\n-------------------\n        \\   ^__^\n         \\  (oo)\\_______\n            (__)\\       )\\/\\ \n                ||----w |\n                ||     ||\n");
    memset(&currentDirectory, 0, sizeof(currentDirectory));

    while(1) {
        getcwd(currentDirectory, sizeof(currentDirectory));
        inputPrompt();
    }
}

void inputPrompt(void) {
    printf("[Flush] %s: ", currentDirectory);
    char buffer[MAX_INPUT_LENGTH];
    fgets(buffer, MAX_INPUT_LENGTH, stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    char input[MAX_ARGS][MAX_PATH];
    memset(&input, 0, sizeof(input));

    char delim[] = " \t";    
    char *ptr = strtok(buffer, delim);

    for (int i = 0; ptr != NULL && i < MAX_ARGS; i++) {
        strcpy(&input[i][0], ptr);
        ptr = strtok(NULL, delim);
    }

    handleInput(input);
}

void printArgs(char input[MAX_ARGS][MAX_PATH]) {
    printf("%s", input[0]);
    for (int i = 1; i < MAX_ARGS; i++) {
        //print only non-empty strings in input
        if (strlen(input[i]) > 0) {
            printf(", %s", input[i]);
        }
    }
}

void ls(char dir[MAX_PATH]) {
    DIR *directory;
    struct dirent *ep;
    if (dir[0] == '\0') {
        // ls was not passed any args
        directory = opendir("./");
    } else {
        directory = opendir(&dir[0]);
    }

    if (directory != NULL) {
        while ((ep = readdir (directory))) {
            puts (ep->d_name);
        }
        (void) closedir(directory);
    } else {
        printf("[ls] Unable to open dir\n");
    }
}

void cd(char dir[MAX_PATH]) {
    if (strlen(dir) > 0) {
        if (chdir(&dir[0]) != 0) {
            printf("[cd] Unable to change directory to %s\n", &dir[0]);
        } 
    } else {
        // change to home dir (not required)
        chdir(getenv("HOME"));
    }
}

void forkAndExec(char command[MAX_ARGS][MAX_PATH]) {
    // get stdin descriptor


    pid_t pid = fork();
    int status;

    if (pid < 0) {
        printf("Error: unable to fork\n");
    }
    if (pid == 0) {
        if (strcmp(command[0], "ls") == 0) {
            ls(command[1]);
            exit(0);
        } else if (strcmp(command[0], "pwd") == 0) {
            printf("%s\n", currentDirectory);
            exit(0);
        } else if (strcmp(command[0], "clear") == 0 || strcmp(command[0], "cls") == 0) {
            printf("\033[2J\033[1;1H");
            exit(0);
        } else if (strcmp(command[0], "help") == 0) {
            printf("ls - list files in current directory\n");
            printf("cd - change directory\n");
            printf("pwd - print current directory\n");
            printf("clear - clear screen\n");
            printf("jobs - prints all running background processes\n");
            printf("exit - exit program\n");
            exit(0);
        } else if (strcmp(command[0],"cd") != 0) {
            char *args[MAX_ARGS];
            args[0] = command[0];
            for (int i = 1; i < MAX_ARGS; i++) {
                if (strlen(command[i]) > 0) {
                    args[i] = command[i];
                } else {
                    args[i] = NULL;
                }
            }
            args[MAX_ARGS - 1] = NULL;
            execvp(args[0], args);

            // if execvp fails, print error
            printf("Error: unable to execute %s\n", args[0]);
            exit(0);
        }
    } else if (pid > 0) {
        printf("%d", pid);
        waitpid(-1, &status, 0);

        if ( WIFEXITED(status) ) {
            // set stdin and stdout to tty
            freopen("/dev/tty", "r", stdin);
            freopen("/dev/tty", "w", stdout);     

            // close stdin
            //close(STDIN_FILENO);
            // close stdout
            //close(STDOUT_FILENO);

            int es = WEXITSTATUS(status);
            printf("Exit status [", es);
            printArgs(command);
            printf("] was %d\n", es);
        }
    }
}

void handleInput(char input[MAX_ARGS][MAX_PATH]) {
    printf("handling the following input: [");
    printArgs(input);
    printf("]\n");

    if (strcmp(input[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(0);
    } else if (strcmp(input[0], "cd") == 0) {
        cd(input[1]);
    }

    freopen("/dev/tty", "r", stdin);
    freopen("/dev/tty", "w", stdout);

    // loop over input and check for &, ;, <, >, |
    // if &, fork and exec
    // if ;, exec
    // if <, redirect stdin
    // if >, redirect stdout
    // if |, fork and exec
    // if no redirection, exec
    int execFlag = 0;
    char command[MAX_ARGS][MAX_PATH];
    memset(&command, 0, sizeof(command));
    int commandIndex = 0;
    
    for (int i = 0; i < MAX_ARGS && strlen(input[i]) > 0; i++) {
        if (strcmp(input[i], "&") == 0) {
            printf("Found &\n");
            execFlag = 1;
            // fork and exec
        } else if (strcmp(input[i], ";") == 0) {
            printf("Found ;\n");
            execFlag = 1;
            // exec
        } else if (strcmp(input[i], "<") == 0) {
            printf("Found <\n");
            // redirect stdin
            for (int j = i + 1; j < MAX_ARGS; j++) {
                if (strlen(input[j]) > 0) {
                    execFlag = 1;
                    freopen(input[j], "r", stdin);
                    break;
                }
            }
        } else if (strcmp(input[i], ">") == 0) {
            printf("Found >\n");
            // redirect stdout
            for (int j = i + 1; j < MAX_ARGS; j++) {
                if (strlen(input[j]) > 0) {
                    execFlag = 1;
                    freopen(input[j], "w", stdout);
                    break;
                }
            }
        } else if (strcmp(input[i], "|") == 0) {
            execFlag = 1;
            printf("Found |\n");
            // fork and exec
        } else {
            if (execFlag) {
                commandIndex = 0;
                execFlag = 0;
            }
            strcpy(&command[commandIndex][0], input[i]);
            commandIndex++;
        }

        if (execFlag)
            forkAndExec(command);
    }
    // exec last command entered
    forkAndExec(command);
}
