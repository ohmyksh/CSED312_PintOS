#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* modified for lab2_2 */
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);
struct lock filesys_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  is_valid_addr((void *)(f->esp));
  for (int i = 0; i < 3; i++) 
    is_valid_addr(f->esp + 4*i);

  int argv[3];
  switch(*(uint32_t *)(f->esp))
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      get_argument(f->esp+4, argv, 1);
      exit((int)argv[0]);
      break;
    case SYS_EXEC:
      get_argument(f->esp+4, argv, 1);
      f->eax = exec((const char*)argv[0]);
      break;
    case SYS_WAIT:
      get_argument(f->esp+4, argv, 1);
      f->eax = wait((pid_t)argv[0]);
      break;
    case SYS_CREATE:
      get_argument(f->esp+4, argv, 2);
      f->eax = create((const char*)argv[0], (unsigned)argv[1]);
      break;
    case SYS_REMOVE:
      get_argument(f->esp+4, argv, 1);
      f->eax = remove((const char*)argv[0]);
      break;
    case SYS_OPEN:
      get_argument(f->esp+4, argv, 1);
      f->eax = open((const char*)argv[0]);
      break;
    case SYS_FILESIZE:
      get_argument(f->esp+4, argv, 1);
      f->eax = filesize((int)argv[0]);
      break;
    case SYS_READ:
      get_argument(f->esp+4, argv, 3);
      f->eax = read((int)argv[0], (void *)argv[1], (unsigned)argv[2]);
      break;
    case SYS_WRITE:
      get_argument(f->esp+4, argv, 3);
      f->eax = write((int)argv[0], (const void *)argv[1], (unsigned)argv[2]);
      break;
    case SYS_SEEK:
      get_argument(f->esp+4, argv, 2);
      seek((int)argv[0], (unsigned)argv[1]);
      break;
    case SYS_TELL:
      get_argument(f->esp+4, argv, 1);
      f->eax = tell((int)argv[0]);
      break;
    case SYS_CLOSE:
      get_argument(f->esp+4, argv, 1);
      close((int)argv[0]);
      break;
    default:
      exit(-1);
  }
}

/* modified for lab2_2 */
void is_valid_addr(void *addr)
{
  if (!addr || !is_user_vaddr(addr) || !pagedir_get_page(thread_current()->pagedir, addr)) 
    exit(-1);
}

void get_argument(void *esp, int *arg, int count)
{
  int i;
  void* arg_pos;
  for (i=0;i<count;i++)
  {
    // argument position
    arg_pos=esp+4*i;
    is_valid_addr(arg_pos);
    arg[i]=*(int*)(arg_pos);
  }
}

void halt(void)
{
  // Terminates Pintos by calling shutdown_power_off()
  shutdown_power_off();
}

void exit(int status)
{
  /* 
  Terminates the current user program, returning status to the kernel.
  If the process’s parent waits for it, this is the status that will be returned.
  Conventionally, a status of 0 indicates success and nonzero values indicate errors.
  */
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}


pid_t exec (const char *cmd_line)
{
  char *ptr = cmd_line;
  struct thread* child;
  pid_t pid;

  // 1. check cmd_line address
  while (true) {
    is_valid_addr(ptr);
    if(*ptr == '\0')
    {
      break;
    }
    ptr++;
  }
  
  // 2. create child process
  pid = process_execute(cmd_line);
  if(pid == -1)
  {
    return -1;
  }

  // 3. get process descriptor of child process
  child = get_child(pid);

  // 4. wait until child process is loaded
  sema_down(&(child->sema_load));

  // 5. return
  if(child->isload)
    return pid;
  else
    return -1;
}


int wait (pid_t pid)
{
  return process_wait(pid); // do same action as process_wait
}

/* file */
bool create(const char* file, unsigned initial_size)
{ 
  /*
  Creates a new file called file initially initial size bytes in size. 
  Returns true if successful, false otherwise. 
  Creating a new file does not open it: opening the new file is a separate operation which would require a open system call.
  */
  is_valid_addr((void*)file);
  if(!file)
  {
    exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool remove(const char* file)
{
  /* 
  Deletes the file called file. 
  Returns true if successful, false otherwise.
  A file may be removed regardless of whether it is open or closed, 
  and removing an open file does not close it.
  */
  return filesys_remove(file);
}

int open (const char *file)
{
  /* Opens the file called file.
  Returns a nonnegative integer handle called a “file descriptor” (fd), or -1 if the file could not be opened.

  Each process has an independent set of file descriptors.
  File descriptors are not inherited by child processes.
  
  When a single file is opened more than once, whether by a single process or different processes, each open returns a new file descriptor.
  Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
  */
 int fd;
 struct file* f;
 struct thread* cur;

 is_valid_addr((void*)file);

 f = filesys_open(file);
 if(f==NULL)
 {
  return -1;
 }

 /* deny write */
 if(!strcmp(thread_current()->name, file))
 {
  file_deny_write(f);
 }
 
 /* add file to process */ 
 cur = thread_current();
 fd = cur->fd_max;

 cur->fd_table[fd] = f;
 cur->fd_max++;

 return fd;
}

int filesize (int fd)
{
  /* Returns the size, in bytes, of the file open as fd. */
  struct file* f;
  f = process_get_file(fd);
  if(f)
  {
    return file_length(f);
  }
  return -1;
}

struct file *process_get_file(int fd)
{
  struct file *f;

  if( (fd > 1) && (fd < thread_current()->fd_max) )
  {
    f = thread_current()->fd_table[fd];
    return f;
  }
  return NULL; 
}

int read (int fd, void *buffer, unsigned size)
{
 /* Reads size bytes from the file open as fd into buffer. 
  Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file).
  Fd 0 reads from the keyboard using input_getc(). */
  int bytes_read=0;
  struct file *f;
  unsigned i;
  for (i = 0; i < size; i++)
    is_valid_addr(buffer+i);

  if(fd==0)
  {
    for (i = 0; i < size;i++)
    {
      ((char*)buffer)[i]=input_getc();
      if(((char*)buffer)[i] == '\0')
      {
        break;
      }
      bytes_read = i;
    }
  }
  else if(fd > 0)
  {
    f = process_get_file(fd);
    if(!f)
    {
      return -1;
    }
    lock_acquire(&filesys_lock);
    bytes_read = file_read(f, buffer, size);
    lock_release(&filesys_lock);
  }
  return bytes_read;
}

int write (int fd, const void *buffer, unsigned size)
{
  /* Writes size bytes from buffer to the open file fd.
  Returns the number of bytes actually written, which may be less than size if some bytes could not be written.
  
  The expected behavior is to write as many bytes as possible up to end-of-file and return the actual number written,
  or 0 if no bytes could be written at all.
  
  Fd 1 writes to the console -> should write all of buffer in one call to putbuf()
  */
  
 int bytes_write = 0;
 struct file* f;
 unsigned i;
  for (i = 0; i < size; i++)
    is_valid_addr(buffer+i);

 if(fd == 1)
 {
  lock_acquire(&filesys_lock);
  putbuf(buffer, size);
  lock_release(&filesys_lock);
  return size;
 }
 else if(fd > 1)
 {
  f = process_get_file(fd);
    if(!f)
    {
      return -1;
    }
    lock_acquire(&filesys_lock);
    bytes_write = file_write(f, buffer, size);
    lock_release(&filesys_lock);
 }
 return bytes_write;

}

void seek (int fd, unsigned position)
{
  /* 
  Changes the next byte to be read or written in open file fd to position, expressed in bytes from the beginning of the file. 
  (Thus, a position of 0 is the file’s start.)
  A seek past the current end of a file is not an error. 
  A later read obtains 0 bytes, indicating end of file.
  A later write extends the file, filling any unwritten gap with zeros.
  (However, in Pintos files have a fixed length until project 4 is complete, so writes past end of file will return an error.)
  These semantics are implemented in the file system and do not require any special effort in system call implementation.
  */
  struct file* f = process_get_file(fd);
  ASSERT(f != NULL);
  file_seek(f, position);
}

unsigned tell (int fd)
{
  /* Returns the position of the next byte to be read or written in open file fd, expressed
  in bytes from the beginning of the file. */
  struct file *f = process_get_file(fd);
  if (f)
  {
    return file_tell(f);
  }
  else
  {
    return -1;
  }
}

void close (int fd)
{
  /* Closes file descriptor fd. Exiting or terminating a process implicitly closes all its open
  file descriptors, as if by calling this function for each one. */
  struct file *f = process_get_file(fd);
  if(f)
  {
    if((fd>1) && (fd<thread_current()->fd_max))
    {
      file_close(f);
      thread_current()->fd_table[fd] = NULL;
    }
  }
}