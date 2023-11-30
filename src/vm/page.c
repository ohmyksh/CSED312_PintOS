#include "vm/page.h"
#include "vm/frame.h"
#include <string.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "vm/swap.h"

static unsigned vm_hash (const struct hash_elem *e, void *aux);
static bool vm_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

extern struct lock filesys_lock;

// vm (hash table) initialization
void vm_init (struct hash *vm) //
{
	hash_init(vm, vm_hash, vm_less, NULL);
}

static unsigned vm_hash (const struct hash_elem *e, void *aux UNUSED)
{
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
	return hash_int((int)vme->vaddr);
}

static bool vm_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	return hash_entry(a, struct vm_entry, elem)->vaddr < hash_entry(b, struct vm_entry, elem)->vaddr;
}	

// vm entry
bool vme_insert (struct hash *vm, struct vm_entry *vme)
{	
	// 인자로 넘겨 받은 vme를 vm entry에 insert
	if (hash_insert(vm, &vme->elem)) 
		return true;
	else 
		return false;
	
}

bool vme_delete (struct hash *vm, struct vm_entry *vme) // syscall munmap에서 호출
{
	lock_acquire(&frame_lock);
	// 인자로 받은 vme를 vm_entry에서 삭제
	if (hash_delete(vm, &vme->elem))
	{
		free_frame(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
		free(vme);
		lock_release(&frame_lock);
		return true;
	}
	else
	{
		lock_release(&frame_lock);
		return false;
	}
}	

struct vm_entry *vme_find (void *vaddr)
{
	// 인자로 받은 vaddr에 해당하는 vm_entry를 찾아서 return
	struct hash *vm = &thread_current()->vm;
	struct vm_entry vme;
	struct hash_elem *elem;

	// (pg_round_down())을 이용하여 vaddr에 해당하는 page number을 추출
	vme.vaddr = pg_round_down(vaddr);
	
	// hash_find() 함수를 이용하여 vm_entry를 찾기
	if ((elem = hash_find(vm, &vme.elem)))
		return hash_entry(elem, struct vm_entry, elem);
	else 
		return NULL;
}

void vm_destroy_func(struct hash_elem *e, void *aux UNUSED)
{
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
	lock_acquire(&frame_lock);
	if(vme)
	{
		if(vme->is_loaded)
		{
			free_frame(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
		}
		free(vme);
	}	
	lock_release(&frame_lock);
	
}

void vm_destroy (struct hash *vm)
{
	// hash_destroy() 함수를 사용하여 hash table의 bucket list와 vm_entry들을 삭제하는 함수
	hash_destroy(vm, vm_destroy_func);
}


bool load_file (void* addr, struct vm_entry *vme)
{
	// 1. file_read_at으로 physical page에 read_bytes만큼 read
	lock_acquire(&filesys_lock);
	int byte_read = file_read_at(vme->file, addr, vme->read_bytes, vme->offset);
	lock_release(&filesys_lock);
	if (byte_read != (int)vme->read_bytes)
		return false;

	// 2. zero_bytes만큼 남는 부분을‘0’으로 채우기
	memset(addr + vme->read_bytes, 0, vme->zero_bytes);
	return true;

}


struct vm_entry *vme_construct ( uint8_t type, void *vaddr, bool writable, bool is_loaded, struct file* file, size_t offset, size_t read_bytes, size_t zero_bytes)
{
	struct vm_entry* vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));
	if (!vme) 
		return NULL;
	memset(vme, 0, sizeof(struct vm_entry));
	// vm_entry struct의 member variable 설정
	vme->type = type;
	vme->vaddr = vaddr;
	vme->writable = writable;
	vme->is_loaded = is_loaded;
	vme->file = file;
	vme->offset = offset;
	vme->read_bytes = read_bytes;
	vme->zero_bytes = zero_bytes;

	return vme;
}
