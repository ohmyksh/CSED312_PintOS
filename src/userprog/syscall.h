#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
# include <stdbool.h>
typedef int pid_t;
typedef int mapid_t;

void syscall_init (void);
/* modified for lab2_2 */
void is_valid_addr(void *addr);
void get_argument(void *esp, int *arg, int count);
struct file *process_get_file(int fd);
void halt(void);
void exit(int status);

pid_t exec (const char *cmd_line);
int wait (pid_t pid);

/* file */
bool create(const char* file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size, void* esp);
int write (int fd, const void *buffer, unsigned size, void* esp);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
mapid_t mmap(int fd, void* addr);
void munmap(mapid_t mapid);
#endif /* userprog/syscall.h */
