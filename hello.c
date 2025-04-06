#include "types.h"
#include "user.h"

unsigned long randstate = 1;
unsigned int
rand()
{
    randstate = randstate * 1664525 + 1013904223;
    return randstate;
}

char arr[8192 * 4];

int main()
{
    arr[0] = 'x'; // touch the page
    for (int i = 0; i < 8192 * 4; i++)
    {
        int random_index = rand() % (16096);
        arr[random_index] = arr[i];
    }
    int sz = 0;
    printf(1, "HELLO");

    printf(1, "HELLO");
    exit();
}
