#include <stdlib.h>
__attribute__((constructor)) void init(void) {
    system("id");
}
int main(void) { return 0; }
