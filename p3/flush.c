#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

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
int handleCommand(char *command[MAX_ARGS], int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH]);

extern int errno;

void ls(char dir[MAX_PATH]);
void pwd(char arg[MAX_PATH]);
void clear(char arg[MAX_PATH]);
void help(char arg[MAX_PATH]);

typedef struct {
    char *name;
    void (*func) (char argument[MAX_PATH]);
} builtin_func;

builtin_func builtins[] = {
    {"ls", &ls},
    {"pwd", &pwd},
    {"clear", &clear},
    {"cls", &clear},
    {"help", &help},
    {NULL, NULL}
};

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
    memset(&buffer, 0, sizeof(buffer));
    
    // read input char by char
    int c;
    while((c = getchar()) != '\n') {
        buffer[strlen(buffer)] = c;
        if (c == EOF) exit(0);
    }
    if (strlen(buffer) == 0) {
        return;
    }    

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
    printf("[%s", input[0]);
    for (int i = 1; i < MAX_ARGS; i++) {
        //print only non-empty strings in input
        if (strlen(input[i]) > 0) {
            printf(", %s", input[i]);
        }
    }
    printf("]");
}

void ls(char dir[MAX_PATH]) {
    DIR *directory;
    if (dir == NULL || dir[0] == '\0') {
        // ls was not passed any args
        directory = opendir("./");
    } else {
        directory = opendir(&dir[0]);
    }

    if (directory != NULL) {
        struct dirent *ep;
        while ((ep = readdir (directory))) {
            puts (ep->d_name);
        }
        (void) closedir(directory);
    } else {
        printf("[ls] Unable to open dir\n");
    }
}

void cd(char dir[MAX_PATH]) {
    if (dir == NULL || dir[0] == '\0') {
        // cd was not passed any args
        chdir(getenv("HOME"));
    } else {
        if (chdir(&dir[0]) != 0) {
            printf("[cd] Unable to change directory to %s\n", &dir[0]);
        }
    }
}

void pwd(char arg[MAX_PATH]) {
    printf("%s\n", currentDirectory);
}

void clear(char arg[MAX_PATH]) {
    printf("\e[1;1H\e[2J"); //regex is faster than system("clear"), credits to geekforgeeks
}

void help(char arg[MAX_PATH]) {
    printf("[help] Available commands:\n");
    printf("[ls] List files in current directory\n");
    printf("[cd] Change directory\n");
    printf("[pwd] Print current directory\n");
    printf("[clear] Clear screen\n");
    printf("[exit] Exit program\n");
}

void handleInput(char input[MAX_ARGS][MAX_PATH]) {
    printf("handling the following input: ");
    printArgs(input);
    printf("\n");

    char *args[MAX_ARGS];
    memset(&args, 0, sizeof(args));
    int exec = 0;
    int nowait = 0;
    char inStreamStr[MAX_PATH];
    char outStreamStr[MAX_PATH];
    memset(&inStreamStr, 0, sizeof(inStreamStr));
    memset(&outStreamStr, 0, sizeof(outStreamStr));

    for (int i = 0, j = 0; i < MAX_ARGS && strlen(input[i]) > 0; i++, j++) {
        if (exec) {
            exec = 0;
            nowait = 0;
            memset(&args, 0, sizeof(args));
            j = 0;
        }

        if (strcmp(input[i], "<") == 0) {
            // redirect stdin
            strcpy(inStreamStr, input[i-1]);
            //clear args bc only file name is in it
            memset(&args, 0, sizeof(args));
            j = -1;
        } else if (strcmp(input[i], ">") == 0) {
            // redirect stdout
            strcpy(outStreamStr, input[i+1]);
            exec = 1;
            i++;
        } else if (strcmp(input[i], "|") == 0) {
            // pipe
            // fork
            // exec
            exec = 1;
        } else if (strcmp(input[i], "&") == 0) {
            // background
            exec = 1;
            nowait = 1;
        } else {
            args[j] = input[i];
        }
        if (exec) {
            handleCommand(args, nowait, inStreamStr, outStreamStr);
        }
    }
    handleCommand(args, nowait, inStreamStr, outStreamStr);
    return;
}

int handleCommand(char *command[MAX_ARGS], int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH]) {
    //built-in commands that must be run in this process
    if (strcmp(command[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(0);
    } else if (strcmp(command[0], "cd") == 0) {
        cd(command[1]);
    } 
    
    pid_t pid = fork();
    if (pid == 0) {
        if (inStreamStr[0] != '\0') {
            freopen(inStreamStr, "r", stdin);
        }
        if (outStreamStr[0] != '\0') {
            freopen(outStreamStr, "w", stdout);
        }
        // loop over builtins
        int external = 1; // assume external command
        for (int i = 0; builtins[i].name != NULL; i++) {
            if (strcmp(command[0], builtins[i].name) == 0) {
                external = 0;
                if (command[1] != NULL && strlen(command[1]) > 0) {
                    builtins[i].func(command[1]);
                } else {
                    builtins[i].func(NULL);
                }
                exit(0);
            }
        }
        if (external && strcmp(command[0],"cd") != 0) {
            command[MAX_ARGS - 1] = NULL;
            if (execvp(command[0], command) == -1) {
                printf("[%s] %s\n", command[0], strerror(errno)); // TODO these messages are a bit greek, maybe change to custom error messages?
                if (errno == 13) {
                    printf("[%s] (File probably doesn't exist)\n", command[0]);
                }
                exit(1);
            }
        }
    } else if (pid > 0) {
        //parent
        if (!nowait) {
            int status;
            waitpid(pid, &status, 0);

            //convert *char[] to char[][] for easier printing bc pointers are scary
            //(env vars are stored right after program args f.ex.)
            char args[MAX_ARGS][MAX_PATH];
            memset(&args, 0, sizeof(args));
            for (int i = 0; i < MAX_ARGS && command[i] != NULL; i++) {
                strcpy(args[i], command[i]);
            }
            printArgs(&args);
            if (WIFEXITED(status)) {
                printf(" exited with status %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf(" terminated by signal %d\n", WTERMSIG(status));
            }
        }
    } else {
        printf("[%s] fork failed\n", command[0]);
    }
    return 0;
}
