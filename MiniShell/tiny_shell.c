#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>


//--------------------------------------global declarations----------------------------------------------

char *historylist[100];             //set history list to max 100
char isFirstHistElement[] = "true"; //initialize to 0, this indicates the history array is empty
int counter = 0;                    //will hold the amount of elements in history list
char *fifoname;
int isControlCPressed = 0; //set to false
struct rlimit lim;

//--------------------------------------get_a_line-------------------------------------------------------

char *get_a_line()
{
    char *input;
    size_t inputsize = 0;
    getline(&input, &inputsize, stdin); //from stdio.h
    int len = strlen(input);            //need to get length of the input

    for (int j = 0; j < strlen(input); j++)
    {
        //replace any newlines or carriage returns with \0
        if (input[j] == '\n' || input[j] == '\r')
        {
            input[j] = '\0';
        }
    }

    if (feof(stdin) != 0)
    {
        exit(0);
    }
    return input;
}

//--------------------------------------len--------------------------------------------------------------

int length(char *str)
{
    return strlen(str);
}

//--------------------------------------history----------------------------------------------------------

void addHistory(char *hist_to_add)
{
    if (strcmp(isFirstHistElement, "true") == 0)
    {
        //initialize historylist
        for (int j = 0; j < 100; j++)
        {
            historylist[j] = NULL;
        }
        isFirstHistElement[0] = 'n';  //the array is initialized so change the first character
        historylist[0] = hist_to_add; //add the element to the list
        counter++;                    //increase num of history counter
    }
    else if (counter < 100)
    {
        //add element to history list and increase counter
        historylist[counter] = hist_to_add;
        counter++;
    }
}

void printHistory()
{
    int num = 1;
    for (int i = 0; i < counter; i++)
    {
        printf("%d\t", num);            //print number and tab
        printf("%s\n", historylist[i]); //print history entry
        num++;                          //increase the number
    }
}

//----------------------------------tokenizer------------------------------------------------------------------

void tokenizer(char *line, char *commands[])
{
    int i = 0;
    char *p = strtok(line, " "); //separate by spaces

    while (p != NULL)
    {

        if (strlen(p) > 0)
        {
            commands[i++] = p; //add to commands array
        }
        p = strtok(NULL, " ");
    }
}

//--------------------------------------signal handling---------------------------------------------------

void SIGINThandler(int sig)
{
    signal(sig, SIGINThandler);
    isControlCPressed = 1; //set to true
    return;
    //will handle in main
}

void SIGTSTPhandler(int sig)
{
    return; //ignore ctrl-z
}

//-----------------------------------------limit--------------------------------------------------------

void limit(long int input)
{
    //set soft and hard limit to the specified number in the input
    lim.rlim_cur = input;
    lim.rlim_max = input;
    if (setrlimit(RLIMIT_DATA, &lim) != 0)
    {
        perror("Couldn't set limit\n");
    }
}

//------------------------------------------piping-------------------------------------------------------

void piping(char *line)
{

    char *left_pipe_command;
    char *right_pipe_command;
    char *left_commands[10];
    char *right_commands[10];
    char *pipe_symbol = "|";

    //initialize
    for (int j = 0; j < 10; j++)
    {
        left_commands[j] = NULL;
        right_commands[j] = NULL;
    }

    left_pipe_command = strtok(line, pipe_symbol); //add what is to the left of the pipe symbol into left_pipe_command
    right_pipe_command = strtok(0, pipe_symbol);   //add what is to the left of the pipe symbol into left_pipe_command

    right_pipe_command++; //get rid of the the space before the actual command

    tokenizer(left_pipe_command, left_commands);   //tokenize by space
    tokenizer(right_pipe_command, right_commands); //tokenize by space
    int status;

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        close(1);                                         //close stdout
        int fd1 = open(fifoname, O_WRONLY);               //open write side of fifo
        dup2(fd1, 1);                                     //stdout redirected
        status = execvp(left_commands[0], left_commands); //execute the command, fifo is written to
        if (status == -1)
        {
            //process failed
            perror("error: execvp failed\n");
            _exit(EXIT_FAILURE);
        }
        close(fd1); //close file descriptor
        //return;
    }
    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        close(0);                                           //close stdin
        int fd2 = open(fifoname, O_RDONLY);                 //open read side of fifo
        dup2(fd2, 0);                                       //stdin redirected
        status = execvp(right_commands[0], right_commands); //execute the command
        if (status == -1)
        {
            //process failed
            perror("error: execvp failed\n");
            _exit(EXIT_FAILURE);
        }
        //return;
        close(fd2); //close file descriptor
    }

    wait(NULL); //wait
    wait(NULL); //wait
}

//-------------------------------------------------my_system----------------------------------------------

void my_system(char *line)
{
    char *commands[50]; //array that will hold commands
    int status;

    if ((fifoname != NULL) && (strstr(line, "|") != NULL))
    {
        piping(line);
        return;
    }

    //initialize
    for (int j = 0; j < 50; j++)
    {
        commands[j] = NULL;
    }

    //tokenize the input line, and put it into the commands arrauy
    tokenizer(line, commands);

    if (strcasecmp(line, "exit") == 0)
    {
        exit(0);
    }
    if (strcmp("EOF", commands[0]) == 0)
    {
        exit(0);
    }

    if ((strcasecmp(commands[0], "chdir") == 0) || (strcasecmp(commands[0], "cd") == 0))
    {
        status = chdir(commands[1]);
        if (status == -1)
        {
            perror(commands[1]);
            _exit(EXIT_FAILURE);
        }
        return;
    }

    else if (strcmp(commands[0], "mkfifo") == 0)
    {
        fifoname = commands[1]; //set the fifo name
        mkfifo(commands[1], 0777);
        return;
    }
    if (strcasecmp(line, "limit") == 0)
    {
        long int input = atoi(commands[1]);
        limit(input);
        return;
    }
    if (strcasecmp(commands[0], "history") == 0)
    {
        printHistory(historylist);
        return;
    }

    //begin fork process, will return -1 if error, 0 if child successfully created
    int pid = fork();

    //check if child successfully created, if yes then do chid process
    if (pid == 0)
    {

        status = execvp(commands[0], commands);
        if (status == -1)
        {
            //process failed
            perror("error: execvp failed\n");
            _exit(EXIT_FAILURE);
        }

        exit(0);
    }

    //if child not created then do parent process
    else
    {

        waitpid(pid, &status, 0); //returns process ID of child
        if (status == -1)
        {
            //process failed
            printf("error:  parent execvp failed\n");
        }
    }
    if (feof(stdin) != 0)
    {
        exit(0);
    }
}

//----------------------------------------------main------------------------------------------------

int main(int argc, char *argv[])
{

    setbuf(stdout, NULL);

    //assume the only arg can be fifo
    if (argc == 2)
    {
        fifoname = argv[1];
    }

    while (1)
    {
        signal(SIGINT, SIGINThandler);   //for ctrl+c
        signal(SIGTSTP, SIGTSTPhandler); //for ctrl+Z

        if (isControlCPressed == 1)
        {
            printf("\nDo you want to terminate the shell? (y/n)\n");
            fflush(stdout);
            char *answer = get_a_line();
            if (strcasecmp(answer, "y") == 0)
            {
                exit(0);
            }
            else if (strcasecmp(answer, "n") == 0)
            {
                free(answer);
                isControlCPressed = 0; //set back to false
                                       //  return;
            }
            else
            {
                printf("invalid answer\n");
                fflush(stdout);
                SIGINThandler(0);
            }
        }

        char *line = get_a_line();
        //addHistory(line); //add the line to history list
        if (length(line) > 1)
        {

            addHistory(line); //add the line to history list

            //check for end of file or exit
            if (feof(stdin) || strcmp(line, "exit\n") == 0)
            {
                exit(0);
            }

            my_system(line);
        }
    }
}