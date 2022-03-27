#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

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
    printf("Welcome to Cterm\n");
    memset(&currentDirectory, 0, sizeof(currentDirectory));

    while(1) {
        getwd(currentDirectory);
        inputPrompt();
    }
}

void inputPrompt(void) {
    printf("[C_TERM] %s: ", currentDirectory);
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
    printf("handling the following input: [");
    for (int i = 0; i < MAX_ARGS; i++) {
        printf("%s, ", input[i]);
    }
    printf("]\n");

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
    }

    if (strcmp(input[0], "cd") == 0) {
        printf("cd todo\n");

    }
}