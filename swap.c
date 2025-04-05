#include "swap.h"
uint bitmap[32];

void init_bitmap()
{
    for (int i = 0; i < 32; i++)
    {
        bitmap[i] = ~0;
    }
}
int get_free_page()
{
    uint index;
    int present = 0;
    for (int i = 0; i < 32; i++)
    {
        int left_most_set_bit = LEFTMOST_SET_BIT(bitmap[i]);
        if (left_most_set_bit != 0)
        {
            index = i * 32 + __builtin_ctz(left_most_set_bit); // for counting number of trailing zeroes we have a surprisingly fast builtin function whiich does the work in O(1) time
            present = 1;
            break;
        }
    }
    if (present == 0)
        return -1;
    return index;
}

int set_page(int page_index)
{
    int i = page_index / 32;
    int j = page_index % 32;
    bitmap[i] &= ~(1 << j);
    return 0;
}

int clear_page(int page_index)
{
    int i = page_index / 32;
    int j = page_index % 32;
    bitmap[i] |= (1 << j);
    return 0;
}
