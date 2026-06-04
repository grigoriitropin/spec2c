#include <dlfcn.h>
int main(void) {
    void *handle = dlopen("lib.so", RTLD_LAZY);
    return 0;
}
