/*
 * Name : Yujiro Nakanishi
 * Class: CS470
 * Category: Assignment#5
 * Date: 3/20/2018
 */


#include <stdio.h>    /* printf perror */
#include <stdlib.h>   /* strtok exit */
#include <unistd.h>   /* fork execvp dup2 pid_t */
#include <string.h>   /* strcmp */
#include <errno.h>    /* errno */
#include <sys/wait.h> /* waitpid */
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>    /* open */
#define MAX_ARG 50
#define READ    0
#define WRITE   1
#define DEBUG

/**
 * @brief   identify (1) type of command and (2) where to split the command
 * @note 
 *      type
 *          => 0 if no | or >> 
 *          => 1 if | 
 *          => 2 if >>
 */

void identifyType(char* command, int* type, int* splitPosition){
    int pos = 0;

    (*type) = 0;

    while(command[pos] != '\0'){

        if(command[pos] == '|'){
            (*type) = 1;
            (*splitPosition) = pos;
        }
        else if(command[pos] == '>' && command[pos+1] == '>'){
            (*type) = 2;
            (*splitPosition) = pos;
        } 

        pos++;
    }
}

/**
 * @brief   from position P of string S1, take out L characters,
 *          and put it to S2
 * @return    
 *          => 0 if success
 *          => -1 if P or L is invalid 
 */

int substr(char* S1, char* S2, int P, int L){
    if( P < 0 || L < 0 || L > strlen(S1) ){
        return -1;
    }

    for( S1 += P; *S1 != '\0' && L > 0; L--){
        *S2++ = *S1++;
    }

    *S2 = '\0';

    return 0;
}


/**
 * @brief   split command into part1 and part2 if | or >> exists
*/

void splitCommand(char* command, char* part1, char* part2, int* type){
    int splitPosition = 0;

    identifyType(command, type, &splitPosition);

    #ifndef DEBUG
    printf("splitPosition = %d\n", splitPosition);
    #endif
    
    if( (*type) != 0 ){ 
        int start = ( (*type) == 1 )? splitPosition + 1 : splitPosition + 2;
        int len   = strlen(command) - start;
        
        substr(command, part1, 0, splitPosition);
        substr(command, part2, start, len);
    } else {
        strcpy(part1, command);
    }

    #ifndef DEBUG
    printf("part1 = %s\n", part1);
    printf("part2 = %s\n", part2);
    #endif
}

/**
 * @brief   take command from user
 * @note
 *   Input more than 255 char will be discarded
 */

void getCommandFromUser(char* command){
    /* %255[^\n] => read atmost 255 */
    /* %*[^\n]   => char more than 255 gets discarded*/
    scanf("%255[^\n]%*[^\n]", command);
    
    /* remove \n at end */
    scanf("%*c");
    
    #ifndef DEBUG
    printf("command = %s\n", command);
    #endif
}

/**
 * @brief   split command into tokens
 */

int tokenizer(char* command, char** tokens){
    int numToken = 0;

    tokens[numToken] = strtok(command, " ");

    while(numToken < MAX_ARG){
        tokens[++numToken] = strtok(NULL, " ");

        if(tokens[numToken] == NULL){
            break;
        }
    }

    return numToken;
}


/**
 * @brief   parse the command
 * @return  
 *          => 0 if run in background
 *          => 1 if else
 */

int parser(char* command, char** parsedCommand){
    int argc = tokenizer(command, parsedCommand);
    int ret;
    
    if(strcmp( parsedCommand[argc - 1], "&") == 0){
        parsedCommand[argc - 1] = NULL;
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}


/**
 * @brief   execute regular command that does not have pipe or redirection   
 */

void Execute_Regular_Command(char* part1){
    char* parsedCommand[20];
    int background; 
    pid_t pid;

    background = parser(part1, parsedCommand);

    if( (pid = fork()) == 0){
        execvp(parsedCommand[0], parsedCommand);
        perror("execvp failed\n");
        exit(1);
    } else {
        if(!background){
            waitpid(pid, NULL, 0);
        }
    }
}

/**
 * @brief   execute piped command
 * @note
 *       0:stdin
 *       1:stdout
 *       2:stderr 
 */

void Execute_Pipe_Command(char* part1, char* part2){
    char* parsedCommandLeft[20];
    char* parsedCommandRight[20];
    int fds[2];
    pid_t pid1, pid2;
    int background;

    if( pipe(fds) < 0){
        perror("pipe failed\n");
        exit(1);
    } 


    background = parser(part1, parsedCommandLeft) || 
                 parser(part2, parsedCommandRight);
    
    if( (pid1 = fork()) < 0){
        perror("fork failed\n");
        exit(1);
    } 

    if( pid1 == 0 ){
        dup2(fds[READ], 0);
        close(fds[WRITE]);
        execvp(parsedCommandRight[0], parsedCommandRight);
        perror("execvp failed\n");
    }
    
    if( (pid2 = fork()) < 0){
        perror("fork failed\n");
        exit(1);
    }

    if( pid2 == 0 ){
        dup2(fds[WRITE], 1);
        close(fds[READ]);
        execvp(parsedCommandLeft[0], parsedCommandLeft);
        perror("execvp failed\n");
    }
    else {
        close(fds[0]);
        close(fds[1]);
        
        if(!background){
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
    }
}


/**
 * @brief   execute redirect command
 */

void Execute_Redirection_Command(char* part1, char* part2){
    char* parsedCommandLeft[20];
    char* parsedCommandRight[20];
    int fds[2];
    int fd;
    int count;
    pid_t pid1, pid2;
    int background;
    char c;

    if( pipe(fds) < 0){
        perror("pipe failed\n");
        exit(1);
    } 


    background = parser(part1, parsedCommandLeft) || 
                 parser(part2, parsedCommandRight);
   
 
    if( (pid1 = fork()) < 0){
        perror("fork failed\n");
        exit(1);
    } 

    if( pid1 == 0 ){
        dup2(fds[WRITE], 1);
        close(fds[READ]);
        execvp(parsedCommandLeft[0], parsedCommandLeft);
        perror("execvp failed\n");
        exit(1);
    }

    if( (pid2 = fork()) < 0){
        perror("fork failed\n");
        exit(1);
    }

    if( pid2 == 0 ){
        if( (fd = open(parsedCommandRight[0], O_RDWR | O_CREAT | O_APPEND, 0600)) < 0){
            perror("open failed\n");
            close(fd);
            exit(1);
        }

        dup2(fds[READ], 0);
        close(fds[WRITE]);

        while( (count = read(0, &c, 1)) > 0){
            write(fd, &c, 1);
        }

        close(fd);
        exit(1);
    }
    else {
        close(fds[READ]);
        close(fds[WRITE]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    }

}

int main(void){
    char commandline[256];
    char part1[128];
    char part2[128];
    int  type = -1;

    printf("\nMyshell> ");
    getCommandFromUser(commandline);

    while( (strcmp(commandline, "quit")!=0) &&
           (strcmp(commandline, "exit")!=0)    ){
        splitCommand(commandline, part1, part2, &type);

        if(type == 0){
            Execute_Regular_Command(part1);
        }
        else if(type == 1){
            Execute_Pipe_Command(part1, part2);
        }
        else if(type == 2){
            Execute_Redirection_Command(part1, part2);
        }

        #ifndef DEBUG
        printf("\nType is %d", type);
        #endif 

        printf("\nMyshell> ");
        getCommandFromUser(commandline);
    }
    return 0;
}
