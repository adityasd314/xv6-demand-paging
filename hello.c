#include "types.h"
#include "user.h"

int main(){
    int *ptr = (int *)0xFFFFDEEE;
    *ptr = 69;
    exit();
}
