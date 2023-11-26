#ifndef VM_PAGE_H
#define VM_PAGE_H

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2

#include <hash.h>
#include "userprog/syscall.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"

struct vm_entry 
{
	uint8_t type; //VM_BIN, VM_FILE, VM_ANON의 타입
	void *vaddr; // virtual page number
	bool writable; // write permission
	bool is_loaded; // physical memory의 load 여부 flag 
	struct file* file; // mapping된 파일 
	size_t offset; // read file offset
	size_t read_bytes; // virtual page에 쓰여져 있는 byte 수
	size_t zero_bytes; // 0으로 채울 남은 페이지의 byte 수
    struct hash_elem elem; // Hash Table element
    struct list_elem mmap_elem; // mmap list element
    size_t swap_slot;
};

struct mmap_file {
  mapid_t mapid;        
  struct file* file;     
  struct list_elem elem; 
  struct list vme_list;  
};

void vm_init (struct hash *vm);

struct vm_entry *vme_find (void *vaddr);

bool vme_insert (struct hash *vm, struct vm_entry *vme);
bool vme_delete (struct hash *vm, struct vm_entry *vme);
void vm_destroy_func(struct hash_elem *e, void *aux);
void vm_destroy (struct hash *vm);
bool load_file (void* kaddr, struct vm_entry *fte);
struct vm_entry *vme_construct ( uint8_t type, void *vaddr, bool writable, bool is_loaded, struct file* file, size_t offset, size_t read_bytes, size_t zero_bytes);
#endif