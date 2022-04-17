#include <fcntl.h>
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
#define MAX_ARGS 20 

#define DONT_USE_PIPE 0
#define WRITE_TO_PIPE 1
#define READ_FROM_PIPE 2
#define READ_AND_WRITE_TO_PIPE 3

void inputPrompt(void);
void handleInput(char input[MAX_ARGS][MAX_PATH]);
void clearInputBuffer(void);
int handleCommand(char *command[MAX_ARGS], int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH], int usePipe, int readPipeFd[2], int writePipeFd[2]);

extern int errno;

void pwd(void);
void clear(void);
void help(void);
void jobs(void);

typedef struct {
    char *name;
    void (*func) (void);
} builtin_func;

// array with builtin commands makes loop easier (and adding new commands is easier)
builtin_func builtins[] = {
    {"pwd", &pwd},
    {"clear", &clear},
    {"cls", &clear},
    {"help", &help},
    {"jobs", &jobs},
    {NULL, NULL}
};

typedef struct process {
    pid_t pid;
    char command[MAX_ARGS][MAX_PATH];
    struct process *next;
} process;

typedef struct process_list {
    process *head;
    process **tail;
} process_list;

void enqueue(process_list *list, process *p) {
    p->next = NULL;
    *list->tail = p;
    list->tail = &p->next;
}

void dequeue(process_list *list, process *p) {
    if (list->head == p) {
        list->head = p->next;
    }
    if (list->tail == &p->next) {
        list->tail = &list->head;
    }
}

int isAmpOrPipe(char string[MAX_PATH]) {
    if (strcmp(string, "&") == 0) return 1;
    if (strcmp(string, "|") == 0) return 1;
    return 0;
}

void printArgs(char input[MAX_ARGS][MAX_PATH]) {
    printf("[%s", input[0]);
    int i = 1;
    while (*input[i] != NULL && strlen(input[i]) > 0 && !isAmpOrPipe(input[i])) {
        printf(" %s", input[i++]);
    }
    printf("]");
}

char currentDirectory[MAX_PATH] = "";
process_list processes = {NULL, &processes.head};

int main(void) {
    //Generated with cowsay
    printf(" ___________________\n< Welcome to Flush >\n-------------------\n        \\   ^__^\n         \\  (oo)\\_______\n            (__)\\       )\\/\\ \n                ||----w |\n                ||     ||\n");
    memset(&currentDirectory, 0, sizeof(currentDirectory));

    while(1) {
        // kill all processes that have exited
        process *p = processes.head;
        while(p) {
            int status;
            if(p->pid == waitpid(p->pid, &status, WNOHANG)) {
                printf("Command ");
                printArgs(p->command);
                printf(" with pid %d has exited with status %d\n", p->pid, status);
                process *next = p->next;
                free(p);
                dequeue(&processes, p);
                p = next;
            } else {
                p = p->next;
            }
        }
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

void pwd(void) {
    printf("%s\n", currentDirectory);
}

void clear(void) {
    printf("\e[1;1H\e[2J"); //regex is faster than system("clear"), credits to geekforgeeks
}

void help(void) {
    printf("[help] Available commands:\n");
    printf("[cd] Change directory\n");
    printf("[pwd] Print current directory\n");
    printf("[clear] Clear screen\n");
    printf("[jobs] List all running background jobs\n");
    printf("[exit] Exit program\n");
}

void jobs(void) {
    process *p = processes.head;
    if (p == NULL) {
        printf("[jobs] No jobs running\n");
        return;
    }
    while(p) {
        printf("[%d] ", p->pid);
        printArgs(p->command);
        printf("\n");
        p = p->next;
    }
}

void handleInput(char input[MAX_ARGS][MAX_PATH]) {
    printf("handling the following input: ");
    printArgs(input);
    printf("\n");

    char *args[MAX_PATH];
    
    short exec = 1; // set to 1 to memset arrays before first iteration
    short nowait = 0;
    int pipes[2][2];
    unsigned short pipeIndex = 0, usePipe = DONT_USE_PIPE;

    // allocate strings for io redirection (they are also used as flags, which is fine in this small program)
    char inStreamStr[MAX_PATH];
    char outStreamStr[MAX_PATH];

    for (int i = 0, j = 0; i < MAX_ARGS && strlen(input[i]) > 0; i++, j++) {
        // reset vars after execution
        if (exec) {
            if (usePipe == WRITE_TO_PIPE || usePipe == READ_AND_WRITE_TO_PIPE) {
                usePipe = READ_FROM_PIPE;
                pipeIndex = !pipeIndex;
            } else if (usePipe == READ_FROM_PIPE)
                usePipe = DONT_USE_PIPE;
            j = 0;
            exec = 0;
            nowait = 0;
            memset(&args, 0, sizeof(args));
            memset(&inStreamStr, 0, sizeof(inStreamStr));
            memset(&outStreamStr, 0, sizeof(outStreamStr));
        }

        if (strcmp(input[i], "<") == 0) {
            strcpy(inStreamStr, input[++i]);
            if (input[i + 1][0] != '>') {
                exec = 1;
            }
        } else if (strcmp(input[i], ">") == 0) {
            strcpy(outStreamStr, input[++i]);
            if (input[i + 1][0] != '<') {
                exec = 1;
            }
        } else if (strcmp(input[i], "|") == 0) {
            if (pipe(pipes[pipeIndex])) {
                printf("[pipe] Unable to create pipe\n");
                return;
            }
            usePipe = usePipe | WRITE_TO_PIPE;
            exec = 1;
        } else if (strcmp(input[i], "&") == 0) {
            exec = 1;
            nowait = 1;
        } else {
            args[j] = input[i];
        }
        if (exec || (i == MAX_ARGS - 1 || strlen(input[i + 1]) == 0)) {
            handleCommand(args, nowait, inStreamStr, outStreamStr, usePipe, pipes[!pipeIndex], pipes[pipeIndex]);
        }
    }
    return;
}

int handleCommand(char **command, int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH], int usePipe, int readPipeFd[2], int writePipeFd[2]) {
    if (&command[0][0] == NULL) {
        return 1;
    }
    command[MAX_ARGS - 1] = NULL;

    //built-in commands that must be run in this process
    if (strcmp(command[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(EXIT_SUCCESS);
    } else if (strcmp(command[0], "cd") == 0) {
        cd(command[1]);
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            if (inStreamStr[0] != '\0') {
                int inStream = open(inStreamStr, O_RDONLY);
                dup2(inStream, STDIN_FILENO);
            }
            if (outStreamStr[0] != '\0') {
                int outStream = open(outStreamStr, O_WRONLY | O_CREAT);
                dup2(outStream, STDOUT_FILENO);
            }
            if (usePipe & WRITE_TO_PIPE) {
                close(writePipeFd[0]);
                dup2(writePipeFd[1], STDOUT_FILENO);
                close(writePipeFd[1]);
            } 
            if ((usePipe & READ_FROM_PIPE) == READ_FROM_PIPE) {
                close(readPipeFd[1]);
                dup2(readPipeFd[0], STDIN_FILENO);
                close(writePipeFd[0]);
            }

            // loop over builtins
            for (int i = 0; builtins[i].name != NULL; i++) {
                if (strcmp(command[0], builtins[i].name) == 0) {
                    builtins[i].func();
                    exit(0);
                }
            }
            if (execvp(command[0], command) == -1) {
                printf("[%s] %s\n", command[0], strerror(errno));
                if (errno == 13) {
                    printf("[%s] (File probably doesn't exist)\n", command[0]);
                }
                exit(1);
            }
        } else if (pid > 0) {
            printArgs(*command);
            
            if (!nowait) {
                int status;
                waitpid(pid, &status, 0);
                if (usePipe & WRITE_TO_PIPE) {
                    close(writePipeFd[1]);
                }
                if ((usePipe & READ_FROM_PIPE) == READ_FROM_PIPE) {
                    close(readPipeFd[0]);
                }

                if (WIFEXITED(status)) {
                    printf(" exited with status %d\n", WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf(" terminated by signal %d\n", WTERMSIG(status));
                }
            }
            else {
                printf(" [%d] started in background\n", pid);
                process *p = malloc(sizeof(process));
                p->pid = pid;
                for (int i = 0; i < MAX_ARGS && command[i] != NULL; i++) {
                    strcpy(p->command[i], command[i]);
                }
                enqueue(&processes, p);
            }
        } else {
            printf("[%s] fork failed\n", command[0]);
        }
    }
}
