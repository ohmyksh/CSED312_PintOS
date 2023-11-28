#include "vm/frame.h"
#include "vm/swap.h"
#include <list.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include <string.h>
#include "filesys/file.h"

extern struct lock filesys_lock;
// Managing frame table

void frame_table_init(void)
{
    list_init(&frame_table);
	lock_init(&frame_lock);
	frame_clock = NULL;
}

void frame_insert(struct frame *frame)
{
	//lock_acquire(&frame_lock);
    list_push_back(&frame_table, &frame->ft_elem);
	//lock_release(&frame_lock);
}

void frame_delete(struct frame *frame)
{	
	if (frame_clock != &frame->ft_elem)
		list_remove(&frame->ft_elem);
	else if (frame_clock == &frame->ft_elem)
		frame_clock = list_remove(frame_clock);
}

struct frame* frame_find(void* addr)
{
    struct list_elem *e;
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame *frame = list_entry(e, struct frame, ft_elem);
		if ((frame->page_addr) == addr)
		{
			return frame;
		}
	}
	return NULL;
}

struct frame* find_frame_for_vaddr(void* vaddr)
{
    struct list_elem *e;
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame *frame = list_entry(e, struct frame, ft_elem);
		if ((frame->vme->vaddr) == vaddr)
			return frame;
	}
	return NULL;
}

struct frame* alloc_frame(enum palloc_flags flags)
{
    struct frame *frame; 

	ASSERT(flags & PAL_USER);

    frame = (struct frame *)malloc(sizeof(struct frame));
	
    if (!frame) return NULL;
    memset(frame, 0, sizeof(struct frame));

    frame->thread = thread_current();
    frame->page_addr = palloc_get_page(flags);
    while (!(frame->page_addr))
    {
        evict_frame();
        frame->page_addr = palloc_get_page(flags); 
    }

	ASSERT(pg_ofs(frame->page_addr) == 0);
	frame->pinned = false;
	frame_insert(frame);		

    return frame;
    
}


void free_frame(void *addr)
{
	struct frame *frame = frame_find(addr);
	if (frame)
	{	
		frame->vme->is_loaded = false;
		pagedir_clear_page(frame->thread->pagedir, frame->vme->vaddr);
		palloc_free_page(frame->page_addr);
		//lock_acquire(&frame_lock);
		frame_delete(frame);
		//lock_release(&frame_lock);
		free(frame);
	}
}

// 6. swap table

void evict_frame()
{
	
	// 1. victim frame 찾기
  	struct frame *frame = find_victim();
	// 2. 해당 frame의 dirty bit 확인
  	bool dirty = pagedir_is_dirty(frame->thread->pagedir, frame->vme->vaddr);
	 
	// 3. frame을 evict할 때 vm_entry의 type을 고려
	switch(frame->vme->type)
	{
		case VM_FILE:
			if(dirty)
			{	
				lock_acquire(&filesys_lock);
				file_write_at(frame->vme->file, frame->page_addr, frame->vme->read_bytes, frame->vme->offset);
				lock_release(&filesys_lock);
			}
			break;
		case VM_BIN:
			if(dirty)
			{	
				frame->vme->type = VM_ANON;
				frame->vme->swap_slot = swap_out(frame->page_addr);
				
			}
			break;
		case VM_ANON:
			frame->vme->swap_slot = swap_out(frame->page_addr);
			break;
	}
	
	// 4. free frame
	pagedir_clear_page(frame->thread->pagedir, frame->vme->vaddr);
	palloc_free_page(frame->page_addr);
	//lock_acquire(&frame_lock);
	frame_delete(frame);
	//lock_release(&frame_lock);
	frame->vme->is_loaded = false;
	free(frame);
	
}


struct frame* find_victim()
{
	struct list_elem *e;
	struct frame *frame;
	
	while (true)
	{
		// clock algorithm에 따라 frame clock 이동
		// clock이 맨 끝을 가리키는 경우
		if (!frame_clock || (frame_clock == list_end(&frame_table)))
		{
			if (!list_empty(&frame_table))
			{
				frame_clock = list_begin(&frame_table);
				e = list_begin(&frame_table);
			}
			else // frame table이 비어있는 경우
				return NULL;
		}
		else // next로 이동
		{
			frame_clock = list_next(frame_clock);
			if (frame_clock == list_end(&frame_table))
				continue;
			e = frame_clock;
		}
		
		frame = list_entry(e, struct frame, ft_elem);
		// access bit 확인 -> 0이면 바로 Return
		if(!frame->pinned)
		{
			if (!pagedir_is_accessed(frame->thread->pagedir, frame->vme->vaddr))
			{
				return frame;
			}
			else
			{
				// access bit 1이면 0으로 바꾸고 그 다음으로 clock이동
				pagedir_set_accessed(frame->thread->pagedir, frame->vme->vaddr, false);
			}
		}
		
	}
}

void delete_all_frame(struct thread* t)
{
	//lock_acquire(&frame_lock);
	struct list_elem *e;
	
	for (e = list_begin(&frame_table); e != list_end(&frame_table);)
	{
		struct frame * f = list_entry(e, struct frame, ft_elem);
		if(f->thread == t)
		{
			frame_delete(f);
			e = list_remove(e);
			free(f);
		}
		else
			e = list_next(e);
	}
	//lock_release(&frame_lock);
}

void frame_pin(void *kaddr)
{
	struct frame *f;
	f = frame_find(kaddr);
	f->pinned = true;
}

void frame_unpin(void *kaddr)
{
	struct frame *f;
	f = frame_find(kaddr);
	f->pinned = false;
}