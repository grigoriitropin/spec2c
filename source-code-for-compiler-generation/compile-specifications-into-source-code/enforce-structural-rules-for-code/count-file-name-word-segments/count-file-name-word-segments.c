// SPDX-License-Identifier: Apache-2.0
#include "../verify-structural-source-code-rules.h"
#include <stdio.h>
#include <string.h>

int count_stem_tokens_lowercase_bytewise(const char *stem, const char *fullname, const char *path) {
    int tokens = 0, tok_len = 0;
    const char *p = stem;
    while (*p) {
        if (*p == '-') {
            if (tok_len < 3) {
                fprintf(stderr, "SOUL §10: file '%s' at %s — token has %d chars (min 3)\n"
                        "  → use full words separated by single '-'\n", fullname, path, tok_len);
                return -1;
            }
            tokens++; tok_len = 0; p++;
            if (*p == '-') {
                fprintf(stderr, "SOUL §10: file '%s' at %s has double hyphen\n"
                        "  → exactly one '-' between each word\n", fullname, path);
                return -1;
            }
            if (*p == '\0') {
                fprintf(stderr, "SOUL §10: file '%s' at %s has trailing hyphen\n"
                        "  → remove trailing '-'\n", fullname, path);
                return -1;
            }
            continue;
        }
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) {
            fprintf(stderr, "SOUL §10: file '%s' at %s — char '%c' (0x%02x) not allowed\n"
                    "  → use only lowercase a-z, digits 0-9 and single '-'\n", fullname, path, *p, (unsigned char)*p);
            return -1;
        }
        tok_len++; p++;
    }
    if (tok_len < 3) {
        fprintf(stderr, "SOUL §10: file '%s' at %s — last token has %d chars (min 3)\n"
                "  → each word must be ≥3 characters\n", fullname, path, tok_len);
        return -1;
    }
    return tokens + 1;
}
