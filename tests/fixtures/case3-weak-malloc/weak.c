#include <stddef.h>
__attribute__((weak)) void *malloc(size_t n) { return NULL; }
int main(void) { return 0; }
