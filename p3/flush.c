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
#define MAX_ARGS 10

#define DONT_USE_PIPE 0
#define WRITE_TO_PIPE 1
#define READ_FROM_PIPE 2
#define READ_AND_WRITE_TO_PIPE 3

void inputPrompt(void);
void handleInput(char input[MAX_ARGS][MAX_PATH]);
void clearInputBuffer(void);
int handleCommand(char *command[MAX_ARGS], int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH], int usePipe, int readPipeFd[2], int writePipeFd[2]);

extern int errno;

void pwd(char arg[MAX_PATH]);
void clear(char arg[MAX_PATH]);
void help(char arg[MAX_PATH]);
void jobs(char arg[MAX_PATH]);

typedef struct {
    char *name;
    void (*func) (char argument[MAX_PATH]);
} builtin_func;

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

//TODO: Locking/semaphores
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

void printArgs(char input[MAX_ARGS][MAX_PATH]) {
    printf("[%s", input[0]);
    for (int i = 1; i < MAX_ARGS; i++) {
        //print only non-empty strings in input
        if (strlen(input[i]) > 0) {
            printf(" %s", input[i]);
        }
    }
    printf("]");
}

int isSpecial(char string[MAX_PATH]) {
    if (strcmp(string, ";") == 0) return 1;
    if (strcmp(string, "&") == 0) return 1;
    if (strcmp(string, "|") == 0) return 1;
    if (strcmp(string, ">") == 0) return 1;
    if (strcmp(string, "<") == 0) return 1;
    return 0;
}

char currentDirectory[MAX_PATH] = "";
process_list processes = {NULL, &processes.head};

int main(void) {
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

void pwd(char arg[MAX_PATH]) {
    printf("%s\n", currentDirectory);
}

void clear(char arg[MAX_PATH]) {
    printf("\e[1;1H\e[2J"); //regex is faster than system("clear"), credits to geekforgeeks
}

void help(char arg[MAX_PATH]) {
    printf("[help] Available commands:\n");
    printf("[cd] Change directory\n");
    printf("[pwd] Print current directory\n");
    printf("[clear] Clear screen\n");
    printf("[jobs] List all running background jobs\n");
    printf("[exit] Exit program\n");
}

void jobs(char arg[MAX_PATH]) {
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

    char *args[MAX_ARGS];
    memset(&args, 0, sizeof(args));
    
    short exec = 0, nowait = 0;
    int pipes[2][2];
    unsigned short pipeIndex = 0, usePipe = DONT_USE_PIPE;

    // allocate strings for io redirection (they are also used as flags, which is fine in this small program)
    char inStreamStr[MAX_PATH];
    char outStreamStr[MAX_PATH];
    memset(&inStreamStr, 0, sizeof(inStreamStr));
    memset(&outStreamStr, 0, sizeof(outStreamStr));

    for (int i = 0, j = 0; i < MAX_ARGS && strlen(input[i]) > 0; i++, j++) {
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

int handleCommand(char *command[MAX_ARGS], int nowait, char inStreamStr[MAX_PATH], char outStreamStr[MAX_PATH], int usePipe, int readPipeFd[2], int writePipeFd[2]) {
    if (&command[0][0] == NULL) {
        return 1;
    }
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
        //convert *char[] to char[][] for easier printing bc pointers are scary
        //(env vars are stored right after program args f.ex.)
        char args[MAX_ARGS][MAX_PATH];
        memset(&args, 0, sizeof(args));
        for (int i = 0; i < MAX_ARGS && command[i] != NULL; i++) {
            strcpy(args[i], command[i]);
        }
        printArgs(&args);
        
        if (!nowait) {
            int status;
            waitpid(pid, &status, 0);
            // close pipes
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
    return 0;
}
