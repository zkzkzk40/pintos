#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
typedef int pid_t;
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

enum process_status
{
    PROCESS_LOADING, /* Default state. */
    PROCESS_FAILED,  /* Failed to load. */
    PROCESS_NORMAL,  /* Running process. */
    PROCESS_EXITED,  /* Exited normally. */
};
struct process
{
    struct thread *thread;      /* Pointer to struct thread. */
    pid_t pid;                  /* Process identifier. */
    enum process_status status; /* Process state. */
    int exit_code;              /* Exit code. */
    struct list_elem allelem;   /* List element for all processes list. */
    struct list_elem elem;      /* List element for children list. */
    struct list children;       /* List of children. */
    struct process *parent;     /* Parent. */
    // struct semaphore sema_load; /* Parent block on this while loading. */
    // struct semaphore sema_wait; /* Parent block on this while waiting. */
    struct list files;          /* Opening files. */
    int fd;                     /* Max file descriptor num. */
    struct file *file;          /* Executable file loaded by self. */
};
#endif /* userprog/process.h */
