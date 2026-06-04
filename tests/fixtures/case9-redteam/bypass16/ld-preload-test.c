#include <stdlib.h>
int main(void) {
    setenv("LD_PRELOAD", "lib.so", 1);
    return 0;
}
