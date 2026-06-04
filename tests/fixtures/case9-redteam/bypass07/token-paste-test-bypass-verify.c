#define PASTE(a,b) a##b
int main(void) {
    PASTE(go,to) fail;
    return 0;
}
