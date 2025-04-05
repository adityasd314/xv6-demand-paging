#include "types.h"
#include "user.h"

char arr[8192 * 2];  // allocate 16KB on stack (default stack ~8KB)

int main(){
    // char arr[8192 * 2];  // allocate 16KB on stack (default stack ~8KB)
    arr[0] = 'x';        // touch the page
    // memset(arr, '0', 8192*2);
    for(int i= 0;i < 8192*2;i++){
        arr[i] = 'x';
    }
    int sz = 0;
    printf(1, "HELLO");
  
 
    printf(1, "HELLO");
    exit();
}
