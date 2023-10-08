#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void put_argv_stack(char *file_name, void **esp);
// void push_to_stack(void **esp, uint32_t value);
#endif /* userprog/process.h */
