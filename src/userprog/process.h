#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void put_argv_stack(char *file_name, void **esp);
// void push_to_stack(void **esp, uint32_t value);

/* modified for lab2_3 */
struct thread* get_child(pid_t pid);
void remove_child(struct thread* t);
#endif /* userprog/process.h */
