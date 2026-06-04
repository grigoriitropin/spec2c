#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUBKEY_HEX "489532082ae4dfc21c6ffe21e1bf78c432bc07200d712ad07568c9a46fe52f24"

int verify_signature(const char *public_key_hex,
                     const char *signature_hex,
                     const unsigned char *contents,
                     size_t contents_len);

int main() {
    FILE *f = fopen("/tmp/rt-5/operator-signed-integrity-manifest-hashes.json", "rb");
    if (!f) { perror("json"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = 0;

    FILE *f2 = fopen("/tmp/rt-5/operator-signed-integrity-manifest-hashes.sig", "rb");
    if (!f2) { perror("sig"); return 1; }
    char sig[256] = {0};
    size_t rd2 = fread(sig, 1, 255, f2);
    fclose(f2);
    sig[rd2] = 0;

    printf("JSON (%ld bytes, rd=%zu): '%s'\n", sz, rd, buf);
    printf("SIG (%zu bytes, rd2=%zu): '%s'\n", strlen(sig), rd2, sig);

    int r = verify_signature(PUBKEY_HEX, sig, (unsigned char*)buf, rd);
    printf("verify_signature returned: %d\n", r);
    return 0;
}
