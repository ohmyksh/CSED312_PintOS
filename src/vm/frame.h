#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"


struct frame
{
	void *page_addr; 
	struct vm_entry *vme;
	struct thread *thread;
	struct list_elem ft_elem; 
	bool pinned;
};

struct list frame_table;
struct lock frame_lock;
struct list_elem *frame_clock;

void frame_table_init(void);
void frame_insert(struct frame *frame);
void frame_delete(struct frame *frame);
struct frame* alloc_frame(enum palloc_flags flags);
struct frame* frame_find(void* addr);
struct frame* find_frame_for_vaddr(void* vaddr);
void free_frame(void *addr);

void evict_frame(void);
struct frame* find_victim(void);

void frame_pin(void *kaddr);
void frame_unpin(void *kaddr);

#endif