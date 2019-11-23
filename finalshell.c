/*Anahita Mohapatra 260773967*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <dirent.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>


//declaration of methods in this file
char *get_a_line(void);
char** split(char* line, char* delim);
int processInput(char* line, char** com1, char** com2 );
int execute_command(char** params);
int checkCommands(char **com);
int process_piped_commands(char **com1, char **com2);
int cd(char **path);
int displayHistory();
int limit(char **userlim);
void sigkill(int sig);
void sigintHandler(int sig);
int my_system(char *line);

/*GLOBAL VARIABLES*/
    //store user's commands for internal history function 
    char *history[100];
    int hist_count = 0;

    //for piped commands
    char *fifo_name;
    char *fifo_path;

    //for internal limit command 
    struct rlimit rl;



char *get_a_line(void) {    
    char *line = NULL;
    size_t buff = 1024; 
    int gl;
    //get user input from command line
    gl = getline(&line, &buff, stdin);
    //make a copy of user's request
    char *copy = strdup(line);
    //if getline is successfull, then we store linr in history array and return user's input
    if (gl >= 0) {
         history[hist_count] = copy;
         hist_count = (hist_count + 1) % 100;
         return line;
    }
    //if getline is unsuccessful we return NULL
    else { 
        return NULL;
 }
}

//this method can split a given string according to a given delimiter 
char** split(char* line, char* delim){
    int buffer_size = 1024; 
    char **params = malloc(buffer_size * sizeof(char));
    //using strtok function to tokenize "line" with a given delimiter
    char *token = strtok(line, delim);
    int i = 0; 
    //Error handling --> insufficient resources to form array 
    if(!params){
        fprintf(stderr, "Error: Unable to allocate space for buffer");
        exit(EXIT_FAILURE);
    }
    //Store string tokens in an array params
     while (token != NULL){
         params[i] = token;
         i++;
         //reallocate more memory if we run out of space in our array
         if (i >= buffer_size){
            buffer_size += buffer_size;
            params = realloc(params, buffer_size);
            //Error-handling --> handles case in which we cannot increase the size of the array
         if(!params){
            fprintf(stderr, "Error: Unable to allocate more space for array!");
            exit(EXIT_FAILURE);
         }
        }
        //if we have no errors, retrieve next token in string
        token = strtok(NULL, delim);
     } 
     //Set last element in array = NULL
     params[i] = NULL;
     //return array of elements
     return params;
}

int processInput(char* line, char** com1, char** com2 ){
 int buffer_size = 1024;
 int result;
 //delimiter 1 = "|" --> split piped commands
 char delim[10];
 strcpy(delim,"|\n");
//delimeter 2 = " " --> split command and arguments
char delim2[10];
strcpy(delim2, " \n");

 char **command = malloc(buffer_size * sizeof(char*));
 char **piped_commands = malloc(buffer_size * sizeof(char*));

 //split user input into commands 
 piped_commands = split(line,delim);

//if there is more than one command then we split the separate commands into arguments and store them into com1 and com2 strings
 //Next we process the piped commands and return the result
  if(piped_commands[1] !=NULL){
     com1 = split(piped_commands[0], delim2);
     com2 = split(piped_commands[1], delim2);
     result = process_piped_commands(com1, com2);
     return result;
  }
  //if we only have one comamnd, then we split command into arguments and execute the simple command
  else{
     command = split(line, delim2);
     result = checkCommands(command);
     return result; 
  }
}
  ////////////////////////////////////////////////////
 ///////////////*COMMAND EXECUTION*//////////////////
////////////////////////////////////////////////////


int execute_command(char** params) {
  pid_t pid = fork(); //fork process
    //Error handling -> Inform user if process forks incorrectly 
    if (pid == -1) { 
        fprintf(stderr, "Error: Unable to fork process");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // child process
        execvp(params[0], params); //execute command
        printf("\nunknown command\n"); //imform user if command is not executed
        return 0;
    } else { // parent process
        int childstatus;
        waitpid(pid, &childstatus, 0); 
}   
      return 1;
}

//check commands to redirect internal commands to respective functions 
int checkCommands(char **com) {
    int result;
    if ((strcmp(com[0],"cd") == 0) || (strcmp(com[0],"chdir") == 0)) {
            return cd(com);
        }
        else if (strcmp(com[0], "history") == 0) {
            return displayHistory();
        }
        else if (strcmp(com[0], "limit") == 0) {
            return limit(com);
        }
        else if (strcmp(com[0], "exit") == 0){
             exit(0);
    }
    //otherwise we execute external command 
        else {
            return execute_command(com);  
    }
}


int process_piped_commands(char **com1, char **com2) {
    int fd;
    pid_t pid1, pid2;

    //handle error if no fifo path is given by user in command line
    if (fifo_path == NULL) {
        printf("Error: No fifo path detected");
        exit(1);
    }
    //first child created
    pid1 = fork();
    //Handle error if unable to complete fork
    if (pid1<0) {
        perror("Error: Unable to complete first fork");
        return -1;
    }
    //run first command
    if (pid1==0) {
        close(1);
        fd = open(fifo_path, O_WRONLY);
        dup(fd);
        //execute first command
        execvp(com1[0],com1);
        //Handle error if unable to complete first command
        perror("Error: Unable to execute first command");
        return 1;
    }

    //second child
    pid2 = fork();
    //Handle error if unable to fork
    if (pid2<0) {
        perror("Error: Unable to complete second fork");
        return -1;
    }
    //run second command
    if (pid2==0) {
        /* Set the process input to the output of the pipe. */
        close(0);
        fd = open(fifo_path, O_RDONLY);
        dup(fd);
        //execute second command
        execvp(com2[0],com2);
        //Handle error if unable to complete second command
        perror("Error: Unable to execute second command");
        close(fd);
        return 1;
    }
    /* Wait for the children to finish, then exit. */
    waitpid(pid1,NULL,0);
    waitpid(pid2,NULL,0);

    return 1;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////*INTERNAL COMMANDS*////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

int cd(char **path) {
    int res;
    const char *home_dir;
    //if we are given no argument for cd, we redirect to home directory
    if (path[1] == NULL) {
        home_dir = getenv("HOME");
        res = chdir(home_dir);
    }
    else {
        // or else we cd into specified directory
        res = chdir(path[1]);
    }
     if (res != 0) {
        //inform user if we are unable to change directory
        perror("Error: Unable to change into specified directory\n");
    }
    return 1;
}


int displayHistory() {
    printf("\n");
    int count = hist_count;
    int num_track = 1;
    do {
        //if history array element exists, we print stored command 
        if (history[count] != NULL) {
            printf("%5d. ", num_track);
            printf("%s\n", history[count]);
            num_track++;
        }
        count = (count + 1) % 100;
    } while (count != hist_count); //loop through history until we return to same spot in array 
    return 1;
}

int limit(char **userlim) {
    char *ptr;
     if (getrlimit(RLIMIT_DATA, &rl) == 0){
        //if user inputs a new limit to be set, we set the new limit
        // or else, we simply print the current limit 
        if (userlim[1] == NULL){
            printf("The current limit is: %lu\n", rl.rlim_cur);
            fflush(stdout);
            return 1;
        }else{
            long unsigned int lim = strtoul(userlim[1],&ptr,10);
            rl.rlim_cur = lim;
            
            //Error handling if we are unable to set new limit 
            if (setrlimit(RLIMIT_DATA, &rl) == -1){
                printf("Error: Could not set the specified limit\n");
                fflush(stdout);
                return -1;
            }
            //inform user that the new limit has been set
            printf("New limit has been set to: %lu\n", lim);
            fflush(stdout);
            return 1;
        }
    }
    else
    {
        printf("Couldn't read current limit.\n");
        fflush(stdout);
        return -1;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////END OF BUILT INTERNAL COMMANDS///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////


/* SIGNAL HANDLERS*/
 void sigkill(int sig){
    printf("\nCTRL Z not recognized\n");
    printf("tshell>>");
    fflush(stdout);
 }

void sigintHandler(int sig) { 
      printf("\n Type \"exit\" to exit or another command to continue\n");
      printf("tshell>>");
      fflush(stdout);
}
/////////////////////////////////////////////////////////////////////////////////////////////////

int my_system(char *line) {
    char **com1; //for first command
    char **com2; //for possible second piped command 
    //splitting the line we read in into commands /parameters and executing them accordingly
   return processInput(line, com1, com2);  
}


//char * fifo_path;
int main(int argc, char *argv[]) {
   // name of pipe passed in as argument
    if (argv[1] != NULL) {
        fifo_name = argv[1];
        char cwd[PATH_MAX];
        //getting absolute path of file created with mkfifo for piped commands
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                 strcat(cwd, "/");
                 strcat(cwd, fifo_name);
                 fifo_path = cwd;
            }  else {
                perror("CWD ERROR");
                return 1;
         }
    }

    int status = 1;
    char *line;
    
    //CTRLC and CTRLZ handlers
    signal(SIGINT, sigintHandler);
    signal(SIGTSTP, sigkill);

    //main loop for our C program shell 
    while (status) {
        //shell prompt
        printf("tshell>> ");
        //get user input 
        line = get_a_line();
        //if user input isn't null then we process and execute command(s) with my_system
        if (line != NULL) {
            my_system(line);
            }     
        else {
            //if user does not input anything we break 
            break;
        }
    }
    return 0;
}
