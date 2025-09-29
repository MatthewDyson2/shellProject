/*
project: 01
author: Matthew Dyson
description: A simple linux shell designed to perform basic Linux commands
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include "utils.h"

#define EXIT "exit"
#define PROC "/proc"
#define HISTORY "history"
#define MAXARGS 100

// Prototypes
int user_prompt_loop();
char **get_user_command();
char **parse_command(char *inputLine);
int execute_command(char **commands); //Only for basic commands

// Helper Commands
int execute_proc(char **args);
int execute_exit(char **args);
int execute_history(char **args);
int write_history(char **args);
void delete_history();

int main(int argc, char **argv){
    /*
    Write the main function that checks the number of argument passed to ensure
    no command-line arguments are passed; if the number of argument is greater
    than 1 then exit the shell with a message to stderr and return value of 1
    otherwise call the user_prompt_loop() function to get user input repeatedly
    until the user enters the "exit" command.
    */
    int returnCode = 0;
    if (argc <= 1){
        returnCode = user_prompt_loop();
    }
    else{
        returnCode = 1;
        fprintf(stderr, "Invalid syntax. Too many arguments in function call!\n");
    }
    delete_history();
    return returnCode;
}

/*
user_prompt_loop():
Get the user input using a loop until the user exits, prompting the user for a command.
Gets command and sends it to a parser, then compares the first element to the two
different commands ("/proc", and "exit"). If it's none of the commands,
send it to the execute_command() function. If the user decides to exit, then exit 0 or exit
with the user given value.
*/

int user_prompt_loop(){
    int run = 1;
    int returnCode = 0;
    do{
        printf("bash >> ");
        char **command = get_user_command();

        // String compare
        if (strcmp(command[0], EXIT) == 0){
            returnCode = execute_exit(command);
            if(returnCode == 0 ){
                run = 0;
            }
        }
        else if (strcmp(command[0], PROC) == 0){
            returnCode = execute_proc(command);
        }
        else if (strcmp(command[0], HISTORY) == 0){
            returnCode = execute_history(command);
        }
        else if(strlen(command[0]) == 0){} //Empty case
        else{
            returnCode = execute_command(command);
        }
        write_history(command);
        int index = 0;
        while(command[index] != NULL){
            free(command[index]);
            index++;
        }
        free(command[index]);
        free(command);
    } while (run);

    
    return returnCode;
}

// Take input from user & return list of commands
char **get_user_command(){
    char *buffer = malloc(10);
    size_t size = 10;
    getline(&buffer, &size, stdin); // Getline automatically resizes memory

    //Remove trailing newline
    if(buffer[strlen(buffer)-1] == '\n'){
        buffer[strlen(buffer)-1] = '\0';
    }

    //Return parsed array
    return parse_command(buffer);
}

// Parse through input line, split by spaces, and return as an array
char **parse_command(char *inputLine){
    // Include support for up to 100 arguments by default
    int maxArgs = MAXARGS;
    int index = 0;
    char **commandArgs = malloc(maxArgs * sizeof(char *));
    char *line = unescape(inputLine, stderr);
    int space = first_unquoted_space(line);
    

    // Tokenize input based on spaces in line
    if (space > 0){
        char *spacePntr = strtok(line, " "); // TODO: Replace with first_unquoted_space() later on

        // Parse loop
        while (spacePntr != NULL){
            commandArgs[index] = malloc(sizeof(spacePntr) + 1);
            strcpy(commandArgs[index], spacePntr);             // Store command in array
            //printf("%s", commandArgs[index]);                // TEST PRINT
            spacePntr = strtok(NULL, " ");
            index++;

            // If more than 100 args, realloc() for 100 more
            if (index >= maxArgs){
                maxArgs += MAXARGS;
                commandArgs = realloc(commandArgs, sizeof(char *) * maxArgs);
            }
        }
    }
    // If no unquoted spaces, then just store the line
    else{
        commandArgs[0] = malloc(sizeof(line));
        strcpy(commandArgs[0], line);
        index = 1; //Prepare for NULL assignment
    }
    commandArgs[index] = NULL; // Set final pointer to null
    commandArgs = realloc(commandArgs, sizeof(char *) * (index+1));
    free(inputLine); // Free buffer from getline()
    free(line);      // Free line since all string data has been copied over
    return commandArgs;
}

/*
execute_command():
Execute the parsed command if the commands are neither /proc nor exit;
fork a process and execute the parsed command inside the child process
*/
int execute_command(char **args){
    // Set up execution variables
    int success = 0;
    pid_t pid;

    pid = fork();

    switch (pid){
    // Parent Process: Return error if fork() failed
    case -1:
        fprintf(stderr, "Command could not be executed. Failed fork() call.\n");
        return -1;
        break;

    // Child Process: Run exec command if child process
    case 0:
        success = execvp(args[0], args);
        //Free memory & terminate process
        int i = 0;
        while(args[i] != NULL){
            free(args[i]);
            i++;
        }
        free(args);
        if (success == -1){
            fprintf(stderr, "Command could not be executed. Invalid syntax.\n");
        }
        kill(getpid(), SIGKILL);
        break;

    // Parent Process: Wait for child process to finish & return
    default:
        waitpid(pid, NULL, 0);
        // Debug printf
        // printf("Successful command.");
        return 0;
    }
    return -1;
}

// Helper functions
int execute_proc(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "Invalid syntax. Missing file destination for /proc");
        return -1;
    }
    //Executing proc file operations
    else if(args[2] == NULL){
        //Prepare full proc file name
        char * fullProc = malloc(sizeof(args[0]) + sizeof(args[1]));
        strcpy(fullProc,args[0]);
        strcat(fullProc,"/");
        strcat(fullProc,args[1]);

        //Cat command implementation instead of file opening
        char ** command = malloc(sizeof(char*) *2);
        command[0] = malloc(sizeof("cat")+1);
        strcpy(command[0],"cat");
        command[1] = fullProc;
        execute_command(command);

        //Free Memory created in this function
        free(command[0]);
        free(fullProc);
        free(command);
    }
    else{
        fprintf(stderr, "Invalid syntax. Too many arguments.\n");
        return -1;
    }
    return 0;
}

//Exit and refuse exit if more than one argument
int execute_exit(char **args){
    if (args[1] != NULL){
        return -1;
    }
    else{
        return 0;
    }
}

//Display history or run previous command if line number is specified
int execute_history(char **args){
    char* filename = ".421sh";
    FILE *histFile = fopen(filename,"r");
    if(histFile == NULL){
        fprintf(stderr, "No history to display.\n");
        return -1;
    }

    //No line number -> display history
    if (args[1] == NULL){       
        char *buffer = malloc(10);
        size_t size = 10;
        while(getline(&buffer,&size,histFile) != -1){
            //Integer check? Something so you only print 10
            printf("%s",buffer);
        }
        free(buffer);
        fclose(histFile);
    }

    //Line number specified -> Run that command again
    else if (args[2] == NULL){
        //Initialize variables
        int lineNum = atoi(args[1]);
        int index = 1;
        int firstSpace = 0;
        int exists = 0;
        char * histArgs = malloc(255);

        //Loop through file until line number
        char *buffer = malloc(255);
        size_t size = 255;
        while((getline(&buffer,&size,histFile) != -1) && index <= lineNum && !(exists)){
            if(index == lineNum){
                //Copy the COMMAND, not the line number
                firstSpace = strcspn(buffer, " ") + 1;
                size = strlen(buffer);
                histArgs = realloc(histArgs,size -firstSpace);
                strncpy(histArgs,buffer+firstSpace,size - firstSpace);
                strcat(histArgs,"\0");
                exists = 1;

                //Remove trailing newline
                if(histArgs[strlen(histArgs)-1] == '\n'){
                    histArgs[strlen(histArgs)-1] = '\0';
                }
            }
            index ++;
        }
        free(buffer);
        fclose(histFile);

        //Borrowed code from user_prompt_loop() to rerun command
        //Conditional protects against out of bounds lines
        if(exists){
            char ** command;

            command = parse_command(histArgs);
            int returnCode = 0;

            //Extra code to handle exiting from execute_history()
            if (strcmp(command[0], EXIT) == 0){
                returnCode = execute_exit(command);
                if(returnCode == 0 ){
                    write_history(command);
                    int index = 0;
                    while(command[index] != NULL){
                        free(command[index]);
                        index++;
                    }
                    free(command[index]);
                    free(command);
                    exit(returnCode);
                }
            }
            else if (strcmp(command[0], PROC) == 0){
                returnCode = execute_proc(command);
            }
            else if (strcmp(command[0], HISTORY) == 0){
                returnCode = execute_history(command);
            }
            else if(strlen(command[0]) == 0){} //Empty case
            else{
                returnCode = execute_command(command);
            }

            int index = 0;
            while(command[index] != NULL){
                free(command[index]);
                index++;
            }
            free(command[index]);
            free(command);
            return returnCode;
        }
        //Line number out of bounds of history file
        else{
            fprintf(stderr, "Error using history. Line number does not exist\n");
            return -1;
        }
    }
    else{
        fprintf(stderr, "Invalid syntax. Input a single line number alongside history\n");
        return -1;
    }
    return 0;
}

//Write the recently entered command to the history file as a single line
int write_history(char **args){
    char* filename = ".421sh";
    int lineNum = 1;

    //Attempt to open file to check for current line number
    FILE *histFile = fopen(filename,"r");
    if(histFile == NULL){
        lineNum = 1;
    }
    else{
        //File exists, so loop until end & count lines
        char *buffer = malloc(10);
        size_t size = 10;
        while(getline(&buffer,&size,histFile) != -1){
            lineNum++;
        }
        free(buffer);
        fclose(histFile);
        
    }
    histFile = fopen(filename, "a");
    int index = 0;
    while(args[index] != NULL){
        //Print line num if first, otherwise separate by space
        if(index == 0){
            fprintf(histFile,"%u ",lineNum);
        }
        else{
            fprintf(histFile," ");
        }
        fprintf(histFile,"%s", args[index]);
        index++;
    }
    //End with newline
    fprintf(histFile,"\n");
    fclose(histFile);
    return 0;
}

void delete_history(){
    if(remove(".421sh") == 0){
        printf("Exiting terminal & deleting history...\n");
    }
    else{
        fprintf(stderr,"Error deleting history file\n");
    }

    return;
}