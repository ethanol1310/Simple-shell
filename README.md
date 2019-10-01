#Shell C Interface 
    Excute command in a sperate process 

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
