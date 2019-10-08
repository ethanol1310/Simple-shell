/*
# Shell C Interface 
## Excute command in a sperate process 

1. Creating the child process and executing the command in the child 
    - pid_t fork(void);
        - Call once, return twice
        - Concurrent excution
        - Duplicate but separate address spaces
        - Shared files
    - pid_t waitpid(pid_t pid, int *startup, int options);
        pid_t wait(int *startus);
        Calling wait(&status) is equivalent to calling waitpid(-1, &status, 0).

    ``` Problem internal command pwd, cd```

    - Implement internal command: pwd, cd

2. Providing a history feature
    - Save command in log.txt
    - !! excute the last command in log.txt

3. Adding support of input and output redirection
    - int creat(char *filename, mode_t mode); for output
    - int open (const char* Path, int flags [, int mode ]); for input
    - int dup2(int oldfd, int newfd);
    
4. Allowing the parent and child processes to communicate via a pipe
    - int pipe(int fds[2]); 
        - fds[0] for read
        - fds[1] for write
    - Backup file description STDIN and STDOUT
    - Open pipe and exec
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

// Macro
#define BUF_SIZE 1024
#define TOKEN_DELIM "\t\n\a\r "
#define TOKENS_BUF_SIZE 16

// Function Prototype
char *trim_white(char *);
void create_log();
void prompt();
void cd_exec();
void pwd_exec();
void parse_redirection_input(char *);
void parse_redirection_output(char *);
char *read_input();
void fdprocess();
void fdwrite(char *);
char **parse_input(char *);
int exec_command(char *);
int pipe_execute();
int execute_cmd(char *);
void set_env();
void unset_env();

// Global variables
char *input;
char **args;
extern char **environ;
char log_his[100][1024];
char *log_file;
char *file_redirection_input;
char *file_redirection_output;
int log_count, redirection_input, redirection_output;

// Define Function

/*
    Support Functions.
*/
char *trim_white(char *str)
{
    while (isspace(*str))
        ++str;
    return str;
}

/*
    Prompt for shell.
    Format: 
        [Simple shell][Directory]:$
*/
void clearEnvVar()
{
    log_count = 0;
    redirection_input = 0;
    redirection_output = 0;
}

void create_log()
{
    log_file = (char *)malloc(1024 * sizeof(char));
    char current_directory[BUF_SIZE];

    if (getcwd(current_directory, sizeof(current_directory)) == NULL)
    {
        perror("Shell err: getcwd error.\n");
        exit(EXIT_FAILURE);
    }
    strcpy(log_file, current_directory);
    strcat(log_file, "/");
    strcat(log_file, "log.txt");
}

void prompt()
{
    // Check Buffer Overflow ???
    char shell[254];
    char cwd[254];
    char *usrname;
    usrname = getenv("USER");

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        sprintf(shell, "[%s]", usrname);
        strcat(shell, cwd);
        strcat(shell, "$ ");

        printf("%s", shell);
    }
    else
        perror("getcwd() error");
}

void cd_exec()
{
    char *h = "/home";
    if (args[1] == NULL)
        chdir(h);
    else if ((strcmp(args[1], "~") == 0) || (strcmp(args[1], "~/") == 0))
        chdir(h);
    else if (chdir(args[1]) < 0)
        printf("bash: cd: %s: No such file or directory\n", args[1]);
}

void pwd_exec()
{
    char cwd[BUF_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
        perror("Shell err: getcwd error");
}

void set_env()
{
    int n = 1;
    char *buf[10];
    if (args[1] == NULL && strcmp(args[0], "export") == 0)
    {
        char **env;
        for (env = environ; *env != 0; env++)
        {
            char *value = *env;
            printf("declare -x %s\n", value);
        }
        return;
    }
    buf[0] = strtok(args[1], "=");
    while ((buf[n] = strtok(NULL, "=")) != NULL)
        n++;
    buf[n] = NULL;
    setenv(buf[0], buf[1], 0);
}

void unset_env() 
{
    if (args[1] == NULL) 
    {
        perror("Shell err: you must enter name of environment variables.\n");
        return;
    }
    else
    {
        unsetenv(args[1]);
    }
    
}

void parse_redirection_input(char *input)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    char *temp_input = strdup(input);
    if (!tokens)
    {
        fprintf(stderr, "Shell err: Allocation error.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(temp_input, "<");
    while (token != NULL)
    {
        tokens[pos++] = token;

        // Check overflow number of args
        if (pos >= TOKENS_BUF_SIZE)
        {
            fprintf(stderr, "Shell err: too much args.\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, "<");
    }
    tokens[1] = trim_white(tokens[1]);
    file_redirection_input = strdup(tokens[1]);
    args = parse_input(tokens[0]);
}

void parse_redirection_output(char *input)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    char *temp_input = strdup(input);
    if (!tokens)
    {
        fprintf(stderr, "Shell err: Allocation error.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(temp_input, ">");
    while (token != NULL)
    {
        tokens[pos++] = token;

        // Check overflow number of args
        if (pos >= TOKENS_BUF_SIZE)
        {
            fprintf(stderr, "Shell err: too much args.\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, ">");
    }
    tokens[1] = trim_white(tokens[1]);
    file_redirection_output = strdup(tokens[1]);
    args = parse_input(tokens[0]);
}

/*
    Read until newline.
    BUF_SIZE = 1024.
    Check overflow.
*/
char *read_input()
{
    char *buf = (char *)malloc(sizeof(char) * BUF_SIZE);

    if (!buf)
    {
        fprintf(stderr, "Shell err: Allocation error.\n");
        exit(EXIT_FAILURE);
    }

    // Read until newline
    char c;
    int pos = 0;

    while (1)
    {
        c = getchar();

        if (c == EOF)
        {
            exit(EXIT_FAILURE);
        }
        else if (c == '\n')
        {
            buf[pos] = '\0';
            return buf;
        }
        else
        {
            buf[pos++] = c;
        }

        // Check overflow
        if (pos >= BUF_SIZE)
        {
            fprintf(stderr, "Shell err: overflow input.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/*
    Read log history.
*/
void fdprocess()
{
    int fd_read;

    fd_read = open(log_file, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
    char *buf = (char *)malloc(sizeof(char) * BUF_SIZE);

    int bytes_read;
    char *temp = (char *)malloc(sizeof(char) * 1024);
    int index = 0;
    while ((bytes_read = read(fd_read, buf, sizeof(buf))) > 0)
    {
        for (int i = 0; i < bytes_read; i++)
        {
            temp[index++] = buf[i];
            if (buf[i] == '\n')
            {
                temp[index - 1] = '\0';
                strcpy(log_his[log_count++], temp); // Duplicate
                index = 0;
            }
        }
    }
    close(fd_read);
}

/*
    Write history.
*/
void fdwrite(char *input)
{
    int fd_write;
    char data[1024];

    strcpy(data, input);
    strcat(data, "\n");

    int data_len = strlen(data);
    fd_write = open(log_file, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    int len = strlen(input);
    if (write(fd_write, data, data_len) < 0)
    {
        printf("Shell err: Error in writing history file.\n");
        return;
    }
    close(fd_write);
}

/*
    Split input into list args.
*/
char **parse_input(char *input)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    if (!tokens)
    {
        fprintf(stderr, "Shell err: Allocation error.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(input, TOKEN_DELIM);
    while (token != NULL)
    {
        tokens[pos++] = token;

        // Check overflow number of args
        if (pos >= TOKENS_BUF_SIZE)
        {
            fprintf(stderr, "Shell err: too much args.\n");
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, TOKEN_DELIM);
    }
    tokens[pos] = NULL;
    return tokens;
}

/*
    Creating the child process and executing the command in the child.
*/
int exec_command(char *input_direct)
{
    pid_t pid;
    int fd_redirection_input;
    int fd_redirection_output;
    char *temp_input = strdup(input_direct);

    pid = fork();
    if (pid == 0)
    {
        // Check redirection
        if (strchr(input, '>') > 0)
        {
            redirection_output = 1;
            parse_redirection_output(temp_input);
        }
        else if (strchr(input, '<') > 0)
        {
            redirection_input = 1;
            parse_redirection_input(temp_input);
        }

        // Check redirection and parse input into args
        // No redirection
        if (redirection_input == 0 && redirection_output == 0)
        {
            args = parse_input(temp_input);
        }

        // Input redirection
        if (redirection_input == 1)
        {
            int fd_redirection_input = open(file_redirection_input, O_RDONLY, 0);
            if (fd_redirection_input < 0)
            {
                fprintf(stderr, "Shell err: Failed to open %s for reading.\n", file_redirection_input);
                return (EXIT_FAILURE);
            }
            dup2(fd_redirection_input, STDIN_FILENO);
            close(fd_redirection_input);
            redirection_input = 0;
        }

        // Output redirection
        if (redirection_output == 1)
        {
            int fd_redirection_output = creat(file_redirection_output, 0644);
            if (fd_redirection_output < 0)
            {
                fprintf(stderr, "Shell err: Failed to open %s for writing.\n", file_redirection_output);
                return (EXIT_FAILURE);
            }
            dup2(fd_redirection_output, STDOUT_FILENO);
            close(fd_redirection_output);
            redirection_output = 0;
        }

        // Check and execute cd, pwd
        if (strcmp(args[0], "cd") == 0)
        {
            cd_exec();
            return 1;
        }
        else if (strcmp(args[0], "pwd") == 0)
        {
            pwd_exec();
            return 1;
        }

        else if (strcmp(args[0], "export") == 0 || strcmp(args[0], "set") == 0)
        {
            set_env();
            return 1;
        }

        else if (strcmp(args[0], "unset") == 0)
        {
            unset_env();
            return 1;
        }

        // Execute command
        else if (execvp(args[0], args) == -1)
        {
            perror("Shell err: execute error.\n");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Shell err: forking error.\n");
    }
    else
    {
        waitpid(-1, NULL, 0);
    }

    return 1;
}

/*
    Pipe.
*/
int pipe_execute()
{
    char *temp_input = strdup(input);
    char **cmds = (char **)malloc(sizeof(char *) * 5);
    char *cmd;
    int backup_out = dup(STDOUT_FILENO);
    int backup_in = dup(STDIN_FILENO);

    if (!cmds)
    {
        fprintf(stderr, "Shell err: Allocation error.\n");
        exit(EXIT_FAILURE);
    }

    cmd = strtok(temp_input, "|");
    cmds[0] = cmd;
    cmd = strtok(NULL, "|");
    cmds[1] = cmd;

    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t p1;

    if (pipe(pipefd) < 0)
    {
        printf("\nPipe could not be initialized");
        return 0;
    }
    p1 = fork();
    if (p1 < 0)
    {
        printf("\nCould not fork");
        return 0;
    }

    if (p1 == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        exec_command(cmds[1]);
        dup2(backup_in, STDIN_FILENO);
    }
    else
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        exec_command(cmds[0]);
        dup2(backup_out, STDOUT_FILENO);
        waitpid(-1, NULL, 0);
    }
    return 1;
}

int execute_cmd(char *temp_input)
{
    if (strchr(temp_input, '|') != NULL)
    {
        return pipe_execute();
    }
    else
    {
        return exec_command(temp_input);
    }
}

/*
    Main program.
*/
int main(int argc, char **argv)
{
    int status = 0;

    create_log();
    do
    {
        clearEnvVar();
        fdprocess();
        prompt();
        input = read_input();

        if (strcmp(input, "exit") == 0)
        {
            status = 0;
            break;
        }
        else
        {
            if (strcmp(input, "!!") == 0)
            {
                if (log_count == 0)
                {
                    printf("Shell err: no history.\n");
                    status = 1;
                    continue;
                }
                strcpy(input, log_his[log_count - 1]);
                status = execute_cmd(input);
            }
            else
            {
                fdwrite(input);
                status = execute_cmd(input);
            }
        }

    } while (status);
    remove("log.txt");
    return 0;
}