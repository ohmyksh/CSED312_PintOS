#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <stdbool.h>

void swap_init(void);
bool swap_in(size_t slot_index, void *kaddr);
size_t swap_out(void* kaddr);

#endif