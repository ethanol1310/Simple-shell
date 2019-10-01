Shell C Interface 
    Excute command in a sperate process 

1. Creating the child process and executing the command in the child 
    1.1 pid_t fork(void);
        1.1.1 Call once, return twice
        1.1.2 Concurrent excution
        1.1.3 Duplicate but separate address spaces
        1.1.4 Shared files
    1.2 pid_t waitpid(pid_t pid, int *startup, int options);
        pid_t wait(int *startus);
        Calling wait(&status) is equivalent to calling waitpid(-1, &status, 0).
    *** Problem internal command pwd, cd
    1.3 Implement internal command: pwd, cd

2. Providing a history feature
    2.1 Save command in log.txt
    2.2 !! excute the last command in log.txt

3. Adding support of input and output redirection
    2.1 int creat(char *filename, mode_t mode); for output
    2.2 int open (const char* Path, int flags [, int mode ]); for input
    2.2 int dup2(int oldfd, int newfd);
    
4. Allowing the parent and child processes to communicate via a pipe