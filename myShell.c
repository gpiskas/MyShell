/*
*  MyShell
*  Copyright (C) 2013  George Piskas
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*  Contact: geopiskas@gmail.com
*/

////////////////////////////////////////////////////////////////
//   Operating Systems - Assignment 1 - Simple Linux Shell    //
//						   Dev Team 						  //
//============================================================//
//	Nestoridhs Adwnhs		  2078		antonest@csd.auth.gr  //
//	Oikonomidhs Gewrgios	  2080		georgido@csd.auth.gr  //
//	Piskas Gewrgios			  2087		gpiskasv@csd.auth.gr  //
//	Siwzos-Drosos Swkraths	  2134		siozosdr@csd.auth.gr  //
////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>

#define TRUE 1
#define FALSE 0

// A type definition "Command" which contains the command
typedef struct {
	// args[0] represents the actual command
	// args[1-4] represent the arguments (max 4 args)
    // args[5] always NULL
    char *args[6];

	// TRUE if the process is background, FALSE otherwise
    int isBackground;
    int numOfArgs;
} Command;

// Prints the PIDs of the background processes that exited (zombies)
void checkBgStatus() {
    int pid = waitpid(-1, NULL, WNOHANG);
    while (pid!=-1 && pid!=0) {
        printf("BG_PID %d Exited.\n", pid);
        pid = waitpid(-1, NULL, WNOHANG);
    }
}

// Checks if the user input is valid. (0 valid, -1 invalid)
int validInput(char input[]) {
    regex_t regex;
    regcomp(&regex, "^ *\n$", 0);
	// Checks for whitespace
    if (regexec(&regex, input, 0, NULL, 0) == 0) {
        regfree(&regex);
	// Checks if command exceeds 100 characters
    } else if (strlen(input) == 99) {
        printf("Command dumped. Max 100 characters.\n");
        // Empty fgets buffer
        do {
            fgets(input, 100, stdin);
        } while (strlen(input) == 0);
	// Valid input case
    } else {
        regfree(&regex);
        input[strlen(input) - 1] = '\0';
        return 0;
    }
    return -1;
}

// Executes a parsed command
void executeCmd(Command* myCommand, char currentDir[]) {
    int flag = -1; // Error checking flag
	// If command does not contain absolute path
    if (strchr(myCommand->args[0], '/') == NULL) {
		// Concatinate the current dir to the command and execute
        flag = execvp( strcat(strcat(currentDir, "/"), myCommand->args[0]), myCommand->args);
		// In case there was an error, execute without path
        if (flag == -1 && execvp(myCommand->args[0], myCommand->args) == -1) {
            perror(myCommand->args[0]);
            exit(-1);
        }
    } else {
        if (execvp(myCommand->args[0], myCommand->args) == -1) {
            perror(myCommand->args[0]);
            exit(-1);
        }
    }
}

// Parses the user input (0 success, -1 error)
int parseCmd(Command* myCommand, char input[]) {
	// args[0] = executable
    myCommand->args[0] = strtok(input, "    \t");
    int i;
	// Arguments cannot exceed 4
    for (i = 1; i < 5; i++) {
        myCommand->args[i] = strtok(NULL, "    \t");
        if (myCommand->args[i] != NULL) {
            (myCommand->numOfArgs)++;
        }
    }
	// If they do, returns error
    if (strtok(NULL, "    \t") != NULL) {
        printf("Too many arguments given.(4 max)\n");
        return -1;
    }
    return 0;
}

// Checks if the current process must be executed in the background
void ambCheck(Command* myCommand, char input[]) {
    if (input[strlen(input) - 1] == '&' && (input[strlen(input) - 2] == ' ' || input[strlen(input) - 2] == '\t')) {
        myCommand->isBackground = TRUE;
        input[strlen(input) - 1] = '\0';
    }
}

// Checks if redirection is present (type identifier, -1 no redirection)
int redirCheck(char input[], char fileName[]) {

	// Recognizes the redirection type and returns its identifier
	// Value of fileName is 4 characters after redir pointer, hence redir + 4
    char *redir = NULL;
    if (strstr(input, "==>") != NULL) {
        redir = strstr(input, "==>");
        input[redir - input] = '\0';
        strcpy(fileName, redir + 4);
        return 0; // redirFlag = 0
    } else if (strstr(input, "<==") != NULL) {
        redir = strstr(input, "<==");
        input[redir - input] = '\0';
        strcpy(fileName, redir + 4);
        return 1; // redirFlag = 1
    } else if (strstr(input, "-->") != NULL) {
        redir = strstr(input, "-->");
        input[redir - input] = '\0';
        strcpy(fileName, redir + 4);
        return 2; // redirFlag = 2
    } else {
        return -1;
    }
}

// Checks if wildcards are present (0 wildcards, -1 no wildcards)
int wildcardCheck(Command* myCommand, char currentDir[]) {

	// If the * symbol is present in the command
    if (myCommand->args[0][0] == '*') {
        printf("Cannot use wildcard in the command.\n");
        return -1;
    }

	// Loop that checks if each argument has the wildcard symbol and acts accordingly
    int i;
    for (i = 1; i <= myCommand->numOfArgs; i++) {

		// If the argument contains a wildcard, the argument is substituded with all the possible combinations
        if (myCommand->args[i][0] == '*') {
            char wildcard[strlen(myCommand->args[i])];
            strcpy(wildcard, (myCommand->args[i]) + 1); // +1 excludes wildcard (*)
            regex_t regex;
            regcomp(&regex, strcat(wildcard, "$"), 0); // Regexp example: "g.txt$"

			// Open current directory and get all entities
            struct dirent *ent;
            DIR *dir = opendir(currentDir);
            char *arg;
			// Checks if there are more files to be processed for matching
            while ((ent = readdir(dir)) != NULL) {
				// If the entity filename matches the regular expression
                if (regexec(&regex, ent->d_name, 0, NULL, 0) == 0) {
                    arg = (char*) malloc(strlen(ent->d_name) + 1);
                    strcpy(arg, ent->d_name);
					// Substitutes the argument containing the wildcard with the new filename
                    myCommand->args[i] = arg;
                    // We only substitute one filename in this loop and add the rest later, hence break
					break;
                }
            }

			// If no matching filename was found
            if (ent == NULL) {
                int j = i;
                while (j <= myCommand->numOfArgs) {
                    // Remove the wildcard argument and move the rest of the arguments one position backwards
                    myCommand->args[j] = myCommand->args[j + 1];
                    j++;
                }
                (myCommand->numOfArgs)--;
                i--;
                continue; // Next instruction
            }

			// Continue checking for matching filenames and store them in empty args[] slots (still max 4 args)
            while ((ent = readdir(dir)) != NULL) {
                if (regexec(&regex, ent->d_name, 0, NULL, 0) == 0) {
                    if (myCommand->numOfArgs < 4) {
                        (myCommand->numOfArgs)++;
                        arg = (char*) malloc(strlen(ent->d_name) + 1);
                        strcpy(arg, ent->d_name);
                        myCommand->args[myCommand->numOfArgs] = arg;
                        myCommand->args[myCommand->numOfArgs + 1] = NULL;
                    } else {
                        printf("Too many arguments given.(4 max)\n");
                        closedir(dir);
                        regfree(&regex);
                        return -1;
                    }
                }
            }
            closedir(dir);
            regfree(&regex);
        }
    }
    return 0;
}

// Checks if pipes are present (0 piping, -1 no piping)
int pipeCheck(Command *myCommand, char input[], int* pipeIsBgPtr, int* pipeCounterPtr, char pipeCmdBuffer[][100]) {
	// If the | symbol is not present
    if (strchr(input, '|') == NULL) {
        return -1;
    }

	// If the process is background
    if (myCommand->isBackground == TRUE) {
        (*pipeIsBgPtr) = TRUE;
    }

	// Tokenize the input
    char *pipeStr = strtok(input, "|");
    while (pipeStr != NULL) {
		// Ignore whitespace
        while (pipeStr[0] == ' ' || pipeStr[0] == '\t') {
            pipeStr++;
        }
		// Load each individual command in the pipe buffer
        strcpy(pipeCmdBuffer[(*pipeCounterPtr)], pipeStr);
        (*pipeCounterPtr)++;
        pipeStr = strtok(NULL, "|");
    }
    (*pipeCounterPtr)--;
	// Set a definitive value in args[0] in order to recognize the case of piping
    myCommand->args[0] = "pipe";
    return 0;
}

// Executes the piping process (0 success, -1 error)
int executePipe(Command* myCommand, char currentDir[], int pipeCounter, char pipeCmdBuffer[][100]) {
	// Pipe file descriptors, 2 for each command pair
    int pipefds[2 * pipeCounter];
    int p = 0;
	// Creation of the pipes using the above file descriptors
    for (p = 0; p < pipeCounter; p++) {
        if (pipe(pipefds + p * 2) == -1) {
            perror("pipe");
            return -1;
        }
    }

	// For each command, set the corresponding pipes and execute
    int cmdCnt = 0;
    while (cmdCnt <= pipeCounter) {
        pid_t pipePid = fork();
        if (pipePid == 0) { // In child

			// If not in first command
            if (cmdCnt != 0) {
				// Replace standard input file descriptor with the corresponding pipefds
                if (dup2(pipefds[(cmdCnt - 1)*2], 0) == -1) {
                    perror("dup2");
                    return -1;
                }
            }

			// If not in last command
            if (cmdCnt != pipeCounter) {
				// Replace standard output file descriptor with the corresponding pipefds
                if (dup2(pipefds[cmdCnt * 2 + 1], 1) == -1) {
                    perror("dup2");
                    return -1;
                }
            }

			// Close all pipe file descriptors
            for (p = 0; p < 2 * pipeCounter; p++) {
                close(pipefds[p]);
            }

			// Checks if the current process must be executed in the background
            ambCheck(myCommand, pipeCmdBuffer[cmdCnt]);
			// Parses pipeCmdBuffer[cmdCnt] and stores it in myCommand
            if (parseCmd(myCommand, pipeCmdBuffer[cmdCnt]) == -1) {
                return -1;
            }
			// Checks if wildcards are present
            if (wildcardCheck(myCommand, currentDir) == -1) {
                return -1;
            }
			// Executes myCommand
            executeCmd(myCommand, currentDir);
        } else if (pipePid < 0) {
            perror("fork");
            return -1;
        }
        cmdCnt++;
    }

	// Close all pipe file descriptors in the parent
    for (p = 0; p < 2 * pipeCounter; p++) {
        close(pipefds[p]);
    }

	// Waits for all pipe commands to end
    for (p = 0; p < pipeCounter + 1; p++) {
        wait(NULL);
    }
    return 0;
}



int main() {

/*
//
// INITIALIZATION
//
*/
    // Initial directory is "/home"
    chdir("/home");

    // History variables
    char cmdHist[10][100]; // cmdHist[0] unused
    int cmdHistCnt = 0;
    for (cmdHistCnt = 0; cmdHistCnt < 10; cmdHistCnt++) { // Initialization to "\0"
        strcpy(cmdHist[cmdHistCnt], "\0");
    }
    cmdHistCnt = 1;
    int loadfromHist = FALSE;

    char currentDir[256]; // Current directory
    char input[100]; // User input

    // Pipe variables
    char pipeCmdBuffer[100][100]; // A buffer which keeps all the commands of each pipe segment
    int pipeCounter = 0; // The amount of pipes (not commands) in the input

    Command *myCommand;
	myCommand = (Command*) malloc(sizeof (Command)); // for free command below
    do {
		// Free the command in the beginning of each loop
        free(myCommand);

        // Initialize the command
        myCommand = (Command*) malloc(sizeof (Command));
        myCommand->isBackground = FALSE;
        myCommand->numOfArgs = 0;

/*
//
// USER INPUT
//
*/
		// Store the current directory in currentDir
        getcwd(currentDir, 256);
		// Prints the PIDs of the background processes that exited (zombies)
        checkBgStatus();

        // Either get user input or load from history
        if (loadfromHist == TRUE) {
            strcat(input,"\n"); // Append enter key to input
            // Write instead of printf for timing reasons
            write(1,"MyShell:hist$ ",14);
            write(1,input,strlen(input));
            loadfromHist = FALSE;
        } else {
            printf("MyShell:%s$ ", currentDir);
            fflush(stdout);
            fgets(input, sizeof (input), stdin);
        }
        // Check for input validity and add to history
        if (validInput(input) == -1) {// Invalid input
            continue;
        } else {
            regex_t regex;
            // Regular expression filters commands that do not match "hist" or "hist w/e" commands
            regcomp(&regex, "\\(^hist$\\)\\|\\(^hist \\)", 0);
            if (regexec(&regex, input, 0, NULL, 0) != 0) {
                strcpy(cmdHist[cmdHistCnt], input);
				// Circular substitution of history array
                if (cmdHistCnt == 9) {
                    cmdHistCnt = 1;
                } else {
                    cmdHistCnt++;
                }
            }
            regfree(&regex);
        }

/*
//
// PARSING
//
*/
        // Checks if the current process must be executed in the background
        ambCheck(myCommand, input);

        // Checks if redirection is present
        char fileName[strlen(input)];
        int redirFlag = redirCheck(input, fileName);

		// Checks if pipes are present
        int pipeIsBg = FALSE;
        int pipeFlag = pipeCheck(myCommand, input, &pipeIsBg, &pipeCounter, pipeCmdBuffer);
        if (pipeFlag == -1) { // If no pipe exists
			// Parses the user input
            if (parseCmd(myCommand, input) == -1) {
				// If the parsing fails, request new input
                continue;
            }
			// Checks if wildcards are present
            if (wildcardCheck(myCommand, currentDir) == -1) {
				// If the wildcardCheck fails, request new input
                continue;
            }
        } else if(pipeFlag != -1 && redirFlag != -1){
			printf("Redirection unsupported with piping.\n");
			continue;
        }

/*
//
// EXECUTION
//
*/
		// Case of exit command
        if (strcmp(myCommand->args[0], "exit") == 0) {
			exit(0);
		// Case of cd command
        } else if (strcmp(myCommand->args[0], "cd") == 0) {
			// Chance path if valid
            if (chdir(myCommand->args[1]) != 0) {
                perror("cd");
            }
		// Case of hist command
        } else if (strcmp(myCommand->args[0], "hist") == 0) {
			// If args[1] is present (hist w/e)
            if (myCommand->args[1] != NULL){
				// Get the number of history record to load from. If args[1] is not a number, 0 is returned
                int histNum = atoi(myCommand->args[1]);
				// Validity checks and input substitution
                if (histNum > 9 || histNum < 1) {
                    printf("Invalid hist argument: %s.\n",myCommand->args[1]);
                } else if (strcmp(cmdHist[histNum], "\0") == 0){
                    printf("No history in position %d.\n",histNum);
                } else {
                    strcpy(input, cmdHist[histNum]);
                    loadfromHist = TRUE;
                }
			// If args[1] is NULL, print all history
            } else {
                int h = 1;
                for (h = 1; h < 10; h++) {
                    if (strcmp(cmdHist[h], "\0")) {
                        printf("%d := %s\n", h, cmdHist[h]);
                    }
                }
            }
		// Case of piping
        } else if (pipeFlag == 0) {
			// Fork and execute piping in child process
            pid_t pipeProcess = fork();
            if (pipeProcess == 0) {// In child
				// Executes the piping process
                if (executePipe(myCommand, currentDir, pipeCounter, pipeCmdBuffer) == -1) {
                    exit(-1);
                }
				// exit the piping child
                exit(0);
            } else if (pipeProcess > 0) {// In parent (shell)
				// If the process is not supposed for background
                if (!pipeIsBg) {
					// Wait for the child process to end
                    if (waitpid(pipeProcess, NULL, WUNTRACED) == -1) {
                        perror("wait");
                        continue;
                    }
				// If it is a background process print its PID
                } else {
                    printf("Assigned BG_PID: %d.\n", pipeProcess);
                }
            } else {
                perror("fork");
                exit(-1);
            }
            pipeCounter = 0;
		// Case of simple user input
        } else {

            pid_t childPid = fork();
            if (childPid == 0) {// In child

                // Check if redirection is present
				// redirOutput is a file descriptor used to replace stdin or stdout
                int redirOutput;
                if (redirFlag == 0) {
                    // Open or create the destination file with the matching or desired filename
                    redirOutput = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                    // Redirect the file descriptors
                    if (dup2(redirOutput, 1)==-1) {
                        perror("dup2");
                    }
                } else if (redirFlag == 1) {
                    redirOutput = open(fileName, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                    if (dup2(redirOutput, 0)==-1) {
                        perror("dup2");
                    }
                } else if (redirFlag == 2) {
                    redirOutput = open(fileName, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                    if (dup2(redirOutput, 1)==-1) {
                        perror("dup2");
                    }
                }
				// Executes a parsed command
                executeCmd(myCommand, currentDir);
            } else if (childPid > 0) {// In parent (shell)
				// If the process is not supposed for background
                if (!(myCommand->isBackground)) {
					// Wait for the childprocess to end
                    if (waitpid(childPid, NULL, WUNTRACED) == -1) {
                        perror("wait");
                        continue;
                    }
				// If it is a background process print its PID
                } else {
                    printf("Assigned BG_PID: %d.\n", childPid);
                }
            } else {
                perror("fork");
                exit(-1);
            }
        }
	// Loop forever and ever...
    } while (TRUE);
}
