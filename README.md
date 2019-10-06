# Shell C Interface 

## Installation
```
gcc -o shellmain shellmain.c
```

## Command
[x] exit
[x] cd
[x] pwd
[x] env
[x] export 
[x] set
[x] unset
[x] history
[] help

## Usage
To start simple shell, run the command below: 
```
./shellmain
```
1. [x]For internal command pwd, cd, set, unset, export
```
$ pwd
$ cd 
$ export
$ export MYVAR="abcd"
$ set MYVAR="abcd"
$ unset MYVAR
```

2. [x]For regular command
```
$ ls
$ ls -l
$ ps
$ cat README.md
```

3. [x]Execute history
```
$ !!
```

4. [x]Single redirection
```
$ ls -l > output
$ grep README < output
```

5. [x]Single pipe
```
$ cat output | grep README
```



## How to implement
1. Creating the child process and executing the command in the child 
    - pid_t fork(void);
        - Call once, return twice
        - Concurrent excution
        - Duplicate but separate address spaces
        - Shared files
    - pid_t waitpid(pid_t pid, int *startup, int options);
    - pid_t wait(int *startus);
        Calling wait(&status) is equivalent to calling waitpid(-1, &status, 0).

    ``` Problem internal command pwd, cd```

    - Implement internal command: pwd, cd, exit, export, set, unset

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

## Credit
- 1712689 Huỳnh Ngọc Quân
- 1712759 Phạm Minh Thắng


