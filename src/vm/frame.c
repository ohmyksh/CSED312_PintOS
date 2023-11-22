#include "vm/frame.h"
#include <list.h>
#include "threads/malloc.h"

extern struct lock file_lock;

// Managing frame table

void frame_table_init(void)
{
    list_init(&frame_table);
	lock_init(&frame_lock);
	frame_clock = NULL;
}

void frame_insert(struct frame *frame)
{
    lock_acquire(&frame_lock);
    list_push_back(&frame_table, &frame->ft_elem);
    lock_release(&frame_lock);
}

void frame_delete(struct frame *frame)
{
	// if (frame_clock != &frame->ft_elem)
	// 	list_remove(&frame->ft_elem);
	// else if (frame_clock == &frame->ft_elem)
	// 	frame_clock = list_remove(frame_clock);
    list_remove(&frame->ft_elem);
}

struct frame* frame_find(void* addr)
{
    struct list_elem *e;
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
	{
		struct frame *frame = list_entry(e, struct frame, ft_elem);
		if ((frame->page_addr) == addr)
			return frame;
	}
	return NULL;
}


struct frame* alloc_frame(enum palloc_flags flags)
{
	if((flags & PAL_USER) == 0)
		return NULL;

    struct frame *frame; 
    frame = (struct frame *)malloc(sizeof(struct frame));
    if (!frame) return NULL;
    memset(frame, 0, sizeof(struct frame));

    frame->thread = thread_current();
    frame->page_addr = palloc_get_page(flags);
    // while (!frame->page_addr)
    // {
    //     evict_frame();
    //     frame->page_addr = palloc_get_page(flags); 
    // }

    if(frame->page_addr)
    {
        // if(list_empty(&frame_table))
        // {
        //     frame_clock = &(frame->ft_elem);
        // }
        frame_insert(frame);
        return frame;
    }
    
}


void free_frame(void *addr)
{
    lock_acquire(&frame_lock);
	struct frame *frame = frame_find(addr);
	if (frame)
	{
		pagedir_clear_page(frame->thread->pagedir, frame->vme->vaddr);
		frame_delete(frame);
		palloc_free_page(frame->page_addr);
		free(frame);
	}
    lock_release(&frame_lock);
}