#include "types.h"
#ifndef SWAP_H
#define SWAP_H

// putting all in this one 
/* 
what we are planning to do 
We will have a global bitmap 
uint bitmap[32] => now this can track 32*32 = 1024 pages
when allocating a page we can get the leftmost set bit in O(1) time
so we will linearly search the 32 entries and get the first free page
*/
#define LEFTMOST_SET_BIT(x) (x & -x)
#define EXTRACT_PTE_FLAGS(x) (x & 0x1FFF) // gets the last 12 bits (1 << 13) - 1
#define IS_MARK_AS_SWAPPED(x) (x & (1 << 9))
#define MARK_AS_SWAPPED(x) (x | (1 << 9)) 
#define PTE_AS_SWAP (index, flags) ((uint)((index) << PTXSHIFT | (flags))) // contructrs address from page directory index, page table index and offset

extern uint bitmap[32];
void init_bitmap(); // sets all to 111..
int get_free_page(); // returns the first 1
int set_page(int);
int clear_page(int);

// IDEA
/*
Pagefault -> check if in swap -> if yes get index -> read from swap 
PAgefault -> lru full -> move to swap -> change the PTE to contain the index in swap and set available bit as in swap and reset PTE_P 

*/
#endif