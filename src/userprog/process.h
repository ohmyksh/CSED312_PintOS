#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"
typedef int pid_t;



tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */

/* modified for lab2_2 */
void put_argv_stack(char *file_name, void **esp);
/* modified for lab2_3 */
struct thread* get_child(pid_t pid);
void remove_child(struct thread* t);

// modified for lab3
bool handle_fault(struct vm_entry *vme);
bool expand_stack(void *addr);
