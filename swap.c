#include "swap.h"
uint bitmap[BITMAP_SIZE];

void bitmap_init()
{
    for (int i = 0; i < BITMAP_SIZE; i++)
    {
        bitmap[i] = ~0;
    }
}
int bitmap_get_free_page()
{
    uint index;
    int present = 0;
    for (int i = 0; i < BITMAP_SIZE; i++)
    {
        int right_most_set_bit = RIGHTMOST_SET_BIT(bitmap[i]);
        if (right_most_set_bit != 0)
        {
            index = i * 32 + __builtin_ctz(right_most_set_bit); // for counting number of trailing zeroes we have a surprisingly fast builtin function whiich does the work in O(1) time
            present = 1;
            break;
        }
    }
    if (present == 0)
        return -1;
    return index;
}

int bitmap_alloc_page(int page_index)
{
    int i = page_index / 32;
    int j = page_index % 32;
    bitmap[i] &= ~(1 << j);
    return 0;
}

int bitmap_dealloc_page(int page_index)
{
    int i = page_index / 32;
    int j = page_index % 32;
    bitmap[i] |= (1 << j);
    return 0;
}
