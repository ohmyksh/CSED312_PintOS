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

#include "vm/page.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);
struct lock filesys_lock;
extern struct lock frame_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  thread_current()->esp = f->esp;
  
  is_valid_addr((void *)(f->esp));
  int i;
  for (i = 0; i < 3; i++) 
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
      f->eax = exec((const char*)argv[0], f->esp);
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
      f->eax = read((int)argv[0], (void *)argv[1], (unsigned)argv[2], f->esp);
      break;
    case SYS_WRITE:
      get_argument(f->esp+4, argv, 3);
      f->eax = write((int)argv[0], (const void *)argv[1], (unsigned)argv[2], f->esp);
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
    case SYS_MMAP:
      get_argument(f->esp+4, argv, 2);
      f->eax = mmap(argv[0], (void *)argv[1]);
      break;
    case SYS_MUNMAP:
      get_argument(f->esp+4, argv, 1);
      munmap(argv[0]);
      break;
    default:
      exit(-1);
  }
}

/* modified for lab2_2 */
void is_valid_addr(void *addr)
{
  // if (!addr || !is_user_vaddr(addr) || !pagedir_get_page(thread_current()->pagedir, addr)) 
  //   exit(-1);
  if (!addr || !is_user_vaddr(addr)) 
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


pid_t exec (const char *cmd_line, void* esp)
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
  
  // pinning
  size_t remained = strlen(cmd_line)+1;
  void *buffer_temp = (void*)cmd_line;
  while(remained > 0)
  {
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    struct vm_entry* vme = vme_find(pg_round_down(buffer_temp));
    if(vme)
    {
      if(!vme->is_loaded)
      {
        if (!handle_fault(vme))
        {
          exit(-1);
        }
      }
    }
    else
    {
      uint32_t base = 0xC0000000;
      uint32_t limit = 0x800000;
      uint32_t lowest_stack_addr = base-limit;
      if ( (buffer_temp >= (esp-32)) && (buffer_temp >= lowest_stack_addr))
      {
        if (!expand_stack(buffer_temp))
        {
          exit(-1);
        }
      }
      else
      {
        exit(-1);
      }
    }
    
    lock_acquire(&frame_lock);
    size_t read_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_pin(frame_to_pin->page_addr);
    lock_release(&frame_lock);
    remained -= read_bt;
    buffer_temp += read_bt;
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

  remained = strlen(cmd_line)+1;
  buffer_temp = (void*)cmd_line;
  while(remained > 0)
  {
    lock_acquire(&frame_lock);
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    size_t read_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_unpin(frame_to_pin->page_addr);
    lock_release(&frame_lock);
    remained -= read_bt;
    buffer_temp += read_bt;
  }

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
  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return success;
}

bool remove(const char* file)
{
  /* 
  Deletes the file called file. 
  Returns true if successful, false otherwise.
  A file may be removed regardless of whether it is open or closed, 
  and removing an open file does not close it.
  */
  is_valid_addr((void*)file);
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

 lock_acquire(&filesys_lock);
 f = filesys_open(file);
 if(f==NULL)
 {
  lock_release(&filesys_lock);
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

 lock_release(&filesys_lock);
 return fd;
}

int filesize (int fd)
{
  /* Returns the size, in bytes, of the file open as fd. */
  struct file* f;
  lock_acquire(&filesys_lock);
  f = process_get_file(fd);
  if(f)
  {
    lock_release(&filesys_lock);
    return file_length(f);
  }
  lock_release(&filesys_lock);
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

int read (int fd, void *buffer, unsigned size, void* esp)
{
 /* Reads size bytes from the file open as fd into buffer. 
  Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file).
  Fd 0 reads from the keyboard using input_getc(). */
  int bytes_read=0;
  struct file *f;
  
  unsigned i;
  for (i = 0; i < size; i++)
    is_valid_addr(buffer+i);
  
  // pinning
  size_t remained = size;
  void *buffer_temp = (void*)buffer;
  while(remained > 0)
  {
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    struct vm_entry* vme = vme_find(pg_round_down(buffer_temp));
    if(vme)
    {
      if(!vme->is_loaded)
      {
        if (!handle_fault(vme))
        {
          exit(-1);
        }
      }
    }
    else
    {
      uint32_t base = 0xC0000000;
      uint32_t limit = 0x800000;
      uint32_t lowest_stack_addr = base-limit;
      if ( (buffer_temp >= (esp-32)) && (buffer_temp >= lowest_stack_addr))
      {
        if (!expand_stack(buffer_temp))
        {
          exit(-1);
        }
      }
      else
      {
        exit(-1);
      }
    }
    lock_acquire(&frame_lock);
    size_t read_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_pin(frame_to_pin->page_addr);
    lock_release(&frame_lock);
    remained -= read_bt;
    buffer_temp += read_bt;
  }
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
    // read
    lock_acquire(&filesys_lock);
    bytes_read += file_read(f, buffer, size);
    lock_release(&filesys_lock);
  }

  remained = size;
  buffer_temp = (void*)buffer;
  while(remained > 0)
  {
    lock_acquire(&frame_lock);
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    size_t read_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_unpin(frame_to_pin->page_addr);
    remained -= read_bt;
    buffer_temp += read_bt;
    lock_release(&frame_lock);
  }
  return bytes_read;
}

int write (int fd, const void *buffer, unsigned size, void *esp)
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
  {
    is_valid_addr(buffer+i);
  }

  // pinning
  size_t remained = size;
  void *buffer_temp = (void*)buffer;
  while(remained > 0)
  {
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    struct vm_entry* vme = vme_find(pg_round_down(buffer_temp));
    if(vme)
    {
      if(!(vme->is_loaded))
      {
        if (!handle_fault(vme))
        {
          exit(-1);
        }
      }
    }
    else
    {
      uint32_t base = 0xC0000000;
      uint32_t limit = 0x800000;
      uint32_t lowest_stack_addr = base-limit;
      if ( (buffer_temp >= (esp-32)) && (buffer_temp >= lowest_stack_addr))
      {
        if (!expand_stack(buffer_temp))
        {
          exit(-1);
        }
      }
      else
      {
        exit(-1);
      }
    }
    
    lock_acquire(&frame_lock);
    size_t write_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_pin(frame_to_pin->page_addr);
    remained -= write_bt;
    buffer_temp += write_bt;
    lock_release(&frame_lock);
  }

 if(fd == 1)
 {
  lock_acquire(&filesys_lock);
  putbuf(buffer, size);
  lock_release(&filesys_lock);
  bytes_write = size;
 }
 else if(fd > 1)
 {
    f = process_get_file(fd);
    if(!f)
    {
      return -1;
    }
    lock_acquire(&filesys_lock);
    bytes_write += file_write(f, buffer, size);
    lock_release(&filesys_lock);
 }
 
 remained = size;
 buffer_temp = (void*)buffer;
 
 while(remained > 0)
  {
    lock_acquire(&frame_lock);
    // size_t ofs = buffer_temp - pg_round_down(buffer_temp);
    size_t write_bt = remained > PGSIZE - pg_ofs(buffer_temp) ? PGSIZE - pg_ofs(buffer_temp) : remained;
    struct frame* frame_to_pin = find_frame_for_vaddr(pg_round_down(buffer_temp));
    frame_unpin(frame_to_pin->page_addr);
    remained -= write_bt;
    buffer_temp += write_bt;
    lock_release(&frame_lock);
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
  lock_acquire(&filesys_lock);
  struct file* f = process_get_file(fd);
  ASSERT(f != NULL);
  file_seek(f, position);
  lock_release(&filesys_lock);
}

unsigned tell (int fd)
{
  /* Returns the position of the next byte to be read or written in open file fd, expressed
  in bytes from the beginning of the file. */
  lock_acquire(&filesys_lock);
  struct file *f = process_get_file(fd);
  if (f)
  {
    lock_release(&filesys_lock);
    return file_tell(f);
  }
  else
  {
    lock_release(&filesys_lock);
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


// modified for lab3
mapid_t mmap(int fd, void* addr)
{
  if(is_kernel_vaddr(addr))
    exit(-1);
  // addr이 0인 경우, addr이 page 정렬되지 않은 경우
  if(!addr || pg_ofs(addr) != 0 || (int)addr%PGSIZE !=0)
    return -1;

  // for vm_entry
  int file_remained;
  size_t offset = 0;

  // 1. mmap_file 구조체 생성 및 메모리 할당
	struct mmap_file *mfe = (struct mmap_file *)malloc(sizeof(struct mmap_file));
  if (!mfe) return -1;   
	memset(mfe, 0, sizeof(struct mmap_file));

	// 2. file open
  lock_acquire(&filesys_lock);
  struct file* file = file_reopen(process_get_file(fd));
  file_remained = file_length(file);
  lock_release(&filesys_lock);
  // fd로 열린 파일의 길이가 0바이트인 경우
  if (!file_remained) 
  {
    return -1;
  }


	// 3. vm_entry 할당
	list_init(&mfe->vme_list);	
  
	while(file_remained > 0)// file 다 읽을 때 까지 반복
	{
		// vm entry 할당
    if (vme_find(addr)) return -1;

    size_t page_read_bytes = file_remained < PGSIZE ? file_remained : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    struct vm_entry* vme = vme_construct(VM_FILE, addr, true, false, file, offset, page_read_bytes, page_zero_bytes);
    if (!vme) 
      return false;

		// 2. vme_list에 mmap_elem과 연결된 vm entry 추가
    list_push_back(&mfe->vme_list, &vme->mmap_elem);
		// 3. current thread에 대해 vme insert
    vme_insert(&thread_current()->vm, vme);
		
    // 4. file addr, offset 업데이트 (page size만큼)
    addr += PGSIZE;
    offset += PGSIZE;
		// 5. file에 남은 길이 업데이트 (page size만큼)
    file_remained -= PGSIZE;
	}

  // 4. mmap_list, mmap_next 관리
  mfe->mapid = thread_current()->mmap_next++;
  list_push_back(&thread_current()->mmap_list, &mfe->elem);
  mfe->file = file;
	return mfe->mapid;
}


void munmap(mapid_t mapid)
{
  // 1. thread의 mmap_list에서 mapid에 해당하는 mfe 찾기
	struct mmap_file *mfe = NULL;
  struct list_elem *e;
  for (e = list_begin(&thread_current()->mmap_list); e != list_end(&thread_current()->mmap_list); e = list_next(e))
  {
    mfe = list_entry (e, struct mmap_file, elem);
    if (mfe->mapid == mapid) break;
  }
  if(mfe == NULL) return;

	// 2. 해당 mfe의 vme_list를 돌면서 vme를 지우기
	for (e = list_begin(&mfe->vme_list); e != list_end(&mfe->vme_list);)
  {
    struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);
    if(vme->is_loaded && (pagedir_is_dirty(thread_current()->pagedir, vme->vaddr)))
    {
      lock_acquire(&filesys_lock);
      file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
      lock_release(&filesys_lock);
      lock_acquire(&frame_lock);
      free_frame(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
      lock_release(&frame_lock);
    }
    vme->is_loaded = false;
    e = list_remove(e);
    vme_delete(&thread_current()->vm, vme);
  }
	// 4. mfe를 mmap_list에서 제거
  list_remove(&mfe->elem);
  // 5. mfe 구조체 자체를 free
  free(mfe); 
}