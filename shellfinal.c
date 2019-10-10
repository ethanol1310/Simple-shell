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
#include <signal.h>

// Macro
#define JOBS_COUNT 20

#define BUF_SIZE 1024
#define TOKENS_BUF_SIZE 64
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024

#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

#define COMMAND_EXTERNAL 0
#define COMMAND_EXIT 1
#define COMMAND_CD 2
#define COMMAND_PWD 3
#define COMMAND_EXPORT 4
#define COMMAND_UNSET 5
#define COMMAND_SET 6

#define TOKEN_DELIM "\t\n\a\r "

// Struct
struct process
{
    char *command;
    int argc;
    char **args;
    char *file_redirection_input;
    char *file_redirection_output;
    pid_t pid;
    int type;
    struct process *next;
};

struct job
{
    int id;
    struct process *root;
    char *command;
    int mode;
};

struct shell_info
{
    char *usr_name;
    char *current_dir;
    char *pw_dir;
    struct job *jobs[JOBS_COUNT + 1];
};

// Global variables
extern char **environ;
char log_his[100][1024];
char *log_file;
int log_count, redirection_input, redirection_output, pipe_exec;
struct shell_info *shell;

// Function Prototype
char *trim_white(char *);
void create_log();
void shell_init();
void prompt();
char *read_input();
void fdprocess();
void fdwrite(char *);

void cd_exec(struct process *);
void pwd_exec();
void set_env(struct process *);
void unset_env(struct process *);

int remove_job(int);
int get_next_job_id();
int insert_job(struct job *);
struct job *create_job(struct process *);
void parse_redirection_input(struct process *);
void parse_redirection_output(struct process *);
char **parse_input(char *);

int exec_command(struct process *, struct job *);
int pipe_execute(struct job *);
int execute_cmd(struct process *);

int get_cmd_type(char *);
int print_processes_of_job(int);

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
        [usr_name][Directory]$
*/
void clearEnvVar()
{
    log_count = 0;
    redirection_input = 0;
    redirection_output = 0;
    pipe_exec = 0;
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

void shell_init()
{
    shell = (struct shell_info *)malloc(sizeof(struct shell_info));
    shell->usr_name = (char *)malloc(sizeof(char) * TOKENS_BUF_SIZE);
    shell->current_dir = (char *)malloc(sizeof(char) * PATH_BUFSIZE);
    shell->pw_dir = (char *)malloc(sizeof(char) * PATH_BUFSIZE);
    create_log();
}

int get_next_job_id()
{
    int i;

    for (i = 1; i <= JOBS_COUNT; i++)
    {
        if (shell->jobs[i] == NULL)
        {
            return i;
        }
    }

    return -1;
}

int insert_job(struct job *job)
{
    int id = get_next_job_id();

    if (id < 0)
    {
        return -1;
    }

    job->id = id;
    shell->jobs[id] = job;
    return id;
}

int remove_job(int id)
{
    if (id > JOBS_COUNT || shell->jobs[id] == NULL)
    {
        return -1;
    }

    shell->jobs[id] = NULL;

    return 0;
}

void prompt()
{
    // Check Buffer Overflow ???
    char cmd_info[254];

    shell->usr_name = getenv("USER");

    if (getcwd(shell->current_dir, PATH_BUFSIZE) != NULL)
    {
        sprintf(cmd_info, "[%s]", shell->usr_name);
        strcat(cmd_info, shell->current_dir);
        strcat(cmd_info, "$ ");

        printf("%s", cmd_info);
    }
    else
        perror("getcwd() error");
}

void cd_exec(struct process *proc)
{
    char *h = "/home";
    if (proc->args[1] == NULL)
        chdir(h);
    else if ((strcmp(proc->args[1], "~") == 0) || (strcmp(proc->args[1], "~/") == 0))
        chdir(h);
    else if (chdir(proc->args[1]) < 0)
        printf("bash: cd: %s: No such file or directory\n", proc->args[1]);
}

void pwd_exec()
{
    if (getcwd(shell->pw_dir, PATH_BUFSIZE) != NULL)
    {
        printf("%s\n", shell->pw_dir);
    }
    else
        perror("Shell err: getcwd error");
}

void set_env(struct process *proc)
{
    int n = 1;
    char *buf[10];
    if (proc->args[1] == NULL && strcmp(proc->args[0], "export") == 0)
    {
        char **env;
        for (env = environ; *env != 0; env++)
        {
            char *value = *env;
            printf("declare -x %s\n", value);
        }
        return;
    }
    buf[0] = strtok(proc->args[1], "=");
    while ((buf[n] = strtok(NULL, "=")) != NULL)
        n++;
    buf[n] = NULL;
    setenv(buf[0], buf[1], 0);
}

void unset_env(struct process *proc)
{
    if (proc->args[1] == NULL)
    {
        perror("Shell err: you must enter name of environment variables.\n");
        return;
    }
    else
    {
        unsetenv(proc->args[1]);
    }
}

int get_cmd_type(char *command)
{
    if (strcmp(command, "exit") == 0)
    {
        return COMMAND_EXIT;
    }
    else if (strcmp(command, "cd") == 0)
    {
        return COMMAND_CD;
    }
    else if (strcmp(command, "export") == 0)
    {
        return COMMAND_EXPORT;
    }
    else if (strcmp(command, "unset") == 0)
    {
        return COMMAND_UNSET;
    }
    else if (strcmp(command, "set") == 0)
    {
        return COMMAND_SET;
    }
    else if (strcmp(command, "exit") == 0)
    {
        return COMMAND_EXIT;
    }
    else
    {
        return COMMAND_EXTERNAL;
    }
}

int print_processes_of_job(int id)
{
    if (id > JOBS_COUNT || shell->jobs[id] == NULL)
    {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next)
    {
        printf(" %d", proc->pid);
    }
    printf("\n");

    return 0;
}

/*
    Split input into list args.
*/
void update_argc(struct process *proc)
{
    int i = 0;
    while (proc->args[i] != NULL)
        i++;
    proc->argc = i;
}

char **parse_input(char *command)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    char *input = strdup(command);
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

void parse_redirection_input(struct process *proc)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    char *temp_input = strdup(proc->command);
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
    proc->file_redirection_input = strdup(tokens[1]);
    proc->args = parse_input(tokens[0]);
    update_argc(proc);
    proc->type = get_cmd_type(proc->args[0]);
}

void parse_redirection_output(struct process *proc)
{
    char **tokens = (char **)malloc(sizeof(char *) * TOKENS_BUF_SIZE);
    char *token;
    int pos = 0;

    char *temp_input = strdup(proc->command);
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
    proc->file_redirection_output = strdup(tokens[1]);
    proc->args = parse_input(tokens[0]);
    update_argc(proc);
    proc->type = get_cmd_type(proc->args[0]);
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

struct job *create_job(struct process *proc)
{

    struct process *root_proc = NULL, *temp_proc = NULL;

    // Check redirection and parse input into args
    // Check redirection
    if (strchr(proc->command, '>') > 0)
    {
        redirection_output = 1;
        parse_redirection_output(proc);
        proc->next = NULL;
        root_proc = proc;
    }
    else if (strchr(proc->command, '<') > 0)
    {
        redirection_input = 1;
        parse_redirection_input(proc);
        proc->next = NULL;
        root_proc = proc;
    }

    // Check pipe
    if (strchr(proc->command, '|') > 0)
    {
        pipe_exec = 1;
        char *temp_input = strdup(proc->command);
        char **cmds = (char **)malloc(sizeof(char *) * 5);
        char *cmd;
        struct process *first_cmd = (struct process *)malloc(sizeof(struct process));
        struct process *second_cmd = (struct process *)malloc(sizeof(struct process));

        if (!cmds)
        {
            fprintf(stderr, "Shell err: Allocation error.\n");
            exit(EXIT_FAILURE);
        }

        cmd = strtok(temp_input, "|");
        first_cmd->command = cmd;
        cmd = strtok(NULL, "|");
        second_cmd->command = cmd;

        root_proc = first_cmd;
        first_cmd->next = second_cmd;
        second_cmd->next = NULL;
    }

    // No redirection, no pipe
    if (redirection_input == 0 && redirection_output == 0 && pipe_exec == 0)
    {
        proc->args = parse_input(proc->command);
        proc->type = get_cmd_type(proc->args[0]);
        update_argc(proc);
        proc->next = NULL;
        root_proc = proc;
    }

    struct job *new_job = (struct job *)malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = strdup(proc->command);
    return new_job;
}

/*
    Creating the child process and executing the command in the child.
*/
int exec_command(struct process *proc, struct job *job)
{
    int fd_redirection_input;
    int fd_redirection_output;

    // Check and execute internal command
    if (proc->type != COMMAND_EXTERNAL)
    {
        if (proc->type == COMMAND_CD)
        {
            cd_exec(proc);
            return 1;
        }
        else if (proc->type == COMMAND_PWD)
        {
            pwd_exec();
            return 1;
        }

        else if (proc->type == COMMAND_EXPORT || proc->type == COMMAND_SET)
        {
            set_env(proc);
            return 1;
        }

        else if (proc->type == COMMAND_UNSET)
        {
            unset_env(proc);
            return 1;
        }
        return 0;
    }

    pid_t childpid;

    childpid = fork();
    if (childpid == 0)
    {
        // Singnal
        signal(SIGTTIN, SIG_DFL); // Terminal input for background process
        signal(SIGTTOU, SIG_DFL); // Terminal output for background process
        signal(SIGCHLD, SIG_DFL); // Child stopped or terminated

        proc->pid = getpid();

        // Input redirection
        if (redirection_input == 1)
        {
            int fd_redirection_input = open(proc->file_redirection_input, O_RDONLY, 0);
            if (fd_redirection_input < 0)
            {
                fprintf(stderr, "Shell err: Failed to open %s for reading.\n", proc->file_redirection_input);
            }
            dup2(fd_redirection_input, STDIN_FILENO);
            close(fd_redirection_input);
            redirection_input = 0;
        }

        // Output redirection
        if (redirection_output == 1)
        {
            int fd_redirection_output = creat(proc->file_redirection_output, 0644);
            if (fd_redirection_output < 0)
            {
                fprintf(stderr, "Shell err: Failed to open %s for writing.\n", proc->file_redirection_output);
            }
            dup2(fd_redirection_output, STDOUT_FILENO);
            close(fd_redirection_output);
            redirection_output = 0;
        }

        // Execute command

        if (execvp(proc->args[0], proc->args) == -1)
            perror("Shell err: execute error.\n");

        exit(EXIT_FAILURE);
    }
    else if (childpid < 0)
    {
        perror("Shell err: forking error.\n");
    }
    else
    {
        proc->pid = childpid;

        if (job->mode != BACKGROUND_EXECUTION)
        {
            waitpid(-1, NULL, 0);
        }
    }

    return 1;
}

/*
    Pipe.
*/
int pipe_execute(struct job *job)
{
    int backup_out = dup(STDOUT_FILENO);
    int backup_in = dup(STDIN_FILENO);

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
        create_job(job->root->next);
        exec_command(job->root->next, job);
        dup2(backup_in, STDIN_FILENO);
    }
    else
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        create_job(job->root);
        exec_command(job->root, job);
        dup2(backup_out, STDOUT_FILENO);
        waitpid(-1, NULL, 0);
    }

    return 1;
}

int execute_cmd(struct process *proc)
{
    struct job *job;
    int job_id;
    int status;

    if (strchr(proc->command, '|') != NULL)
    {
        job = create_job(proc);
        job->mode = PIPELINE_EXECUTION;
        job_id = insert_job(job);

        pipe_exec = 0;
        pipe_execute(job);
    }
    else if (strchr(proc->command, '&') != NULL)
    {
        proc->command[strlen(proc->command) - 1] = '\0';
        job = create_job(proc);
        job->mode = BACKGROUND_EXECUTION;
        job_id = insert_job(job);

        status = exec_command(proc, job);
    }
    else
    {
        job = create_job(proc);
        job->mode = FOREGROUND_EXECUTION;
        if (job->root->type == COMMAND_EXTERNAL)
        {
            job_id = insert_job(job);
        }

        status = exec_command(proc, job);
    }

    if (job->root->type == COMMAND_EXTERNAL)
    {
        if (job->mode == FOREGROUND_EXECUTION)
        {
            remove_job(job_id);
        }
        else if (job->mode == BACKGROUND_EXECUTION)
        {
            print_processes_of_job(job_id);
        }
    }
    return status;
}

/*
    Main program.
*/
int main(int argc, char **argv)
{
    int status;
    shell_init();
    do
    {
        clearEnvVar();
        fdprocess();
        prompt();
        struct process *new_proc = (struct process *)malloc(sizeof(struct process));
        new_proc->command = read_input();

        if (strcmp(new_proc->command, "exit") == 0)
        {
            status = 0;
            break;
        }
        else if (strcmp(new_proc->command, "!!") == 0)
        {
            if (log_count == 0)
            {
                printf("Shell err: no history.\n");
                continue;
            }
            strcpy(new_proc->command, log_his[log_count - 1]);
            status = execute_cmd(new_proc);
        }
        else
        {
            fdwrite(new_proc->command);
            status = execute_cmd(new_proc);
        }
    } while (status);

    printf("Goodbye.\n");
    remove("log.txt");
    return 0;
}