/* Known-Answer Test for the C SHA-256 primitive.
 * Closes the Thompson semantic-gap at the hash level: a deterministic-but-wrong
 * hash would pass freeze/manifest/VCS silently. This gate compares against the
 * published FIPS 180-4 vectors, so any drift from standard SHA-256 fails the build. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void compute_sha256_hash_into_bytes(const uint8_t *data, uint32_t len, uint8_t out[32]);

static int check_one_known_answer_vector(const char *label, const char *msg,
                                         uint32_t len, const char *expect_hex) {
    uint8_t out[32];
    compute_sha256_hash_into_bytes((const uint8_t *)msg, len, out);
    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", out[i]);
    if (strcmp(hex, expect_hex) != 0) {
        fprintf(stderr, "SHA256 KAT FAIL [%s]: got %s expected %s\n", label, hex, expect_hex);
        return 1;
    }
    return 0;
}

int main(void) {
    int fail = 0;
    fail |= check_one_known_answer_vector("empty", "", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    fail |= check_one_known_answer_vector("abc", "abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    fail |= check_one_known_answer_vector("nist56",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    if (fail) { fprintf(stderr, "SHA256 KAT: FAILED\n"); return 1; }
    return 0;
}
