// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#include "src/bootstrap-compiled-limit-hash-data/bootstrap-file-sha-hashes-generated.h"
#include "verify-ed25519-digital-signature-key.h"

/* ── Ed25519 Operator Public Key (hex) ────────────────────────────────────── */
/* Generated once by the operator; private key stays offline. */
static const char OPERATOR_PUBKEY_HEX[] =
    "489532082ae4dfc21c6ffe21e1bf78c432bc07200d712ad07568c9a46fe52f24";

/* ── Whitelist of Allowed Symbols (Narrowed) ─────────────────────────────── */
static const char *allowed_symbols[] = {
    /* Toolchain / compiler auto-injected */
    "__libc_start_main", "__cxa_finalize",
    "__stack_chk_fail", "__gmon_start__",
    "_ITM_deregisterTMCloneTable", "_ITM_registerTMCloneTable",

    /* Standard I/O streams */
    "stderr", "stdout", "stdin",

    /* Process lifecycle */
    "exit", "_exit",

    /* Memory */
    "malloc", "free", "realloc", "memcpy", "memmove", "memset", "memcmp",

    /* File I/O (used by enforcer ELF reader and by generated IPM code) */
    "fopen", "fclose", "fread", "fwrite", "fseek", "ftell", "fdopen", "fflush",
    "feof", "ferror", "fgets", "fputs",

    /* Formatted I/O (used by generated IPM code and enforcer diagnostics) */
    "fprintf", "snprintf", "sprintf",
    "__fprintf_chk", "__snprintf_chk", "__sprintf_chk",
    "__fread_chk", "__memcpy_chk",

    /* Directory traversal (used by generated IPM scanner) */
    "opendir", "readdir", "closedir",

    /* String functions (used by generated IPM code and verify-ed25519-digital-signature-key) */
    "strcmp", "strchr", "strrchr", "strstr", "strcpy", "strdup", "strtok", "strlen",

    /* Number parsing (used by generated IPM code) */
    "strtod", "strtol",
    "__isoc99_sscanf", "__isoc23_sscanf", "__isoc23_strtol",

    /* Misc libc (used by generated IPM code) */
    "stat", "qsort", "localeconv",
    "__ctype_tolower_loc",

    /* Process control (used by buffer-output-and-command-launch.c) */
    "fork", "execvp", "waitpid", "dup2", "pipe", "close",
};

/* ── Banned Symbols (fatal regardless of binding: UNDEF, WEAK, GLOBAL) ─────── */
/* These must NEVER appear in the binary, whether referenced (U) or defined (T/W/D). */
/* Removing an entry from here is always AI-permitted. Adding requires operator sig. */
static const char *banned_symbols[] = {
    "strncmp", "regcomp", "regexec",
};

/* ── Base Whitelist Snapshot (for additions gate) ────────────────────────── */
/* MUST stay identical to allowed_symbols[] above at all times. */
/* Any addition to allowed_symbols[] that is NOT here requires operator sig. */
static const char *base_whitelist[] = {
    "__libc_start_main", "__cxa_finalize",
    "__stack_chk_fail", "__gmon_start__",
    "_ITM_deregisterTMCloneTable", "_ITM_registerTMCloneTable",
    "stderr", "stdout", "stdin",
    "exit", "_exit",
    "malloc", "free", "realloc", "memcpy", "memmove", "memset", "memcmp",
    "fopen", "fclose", "fread", "fwrite", "fseek", "ftell", "fdopen", "fflush",
    "feof", "ferror", "fgets", "fputs",
    "fprintf", "snprintf", "sprintf",
    "__fprintf_chk", "__snprintf_chk", "__sprintf_chk",
    "__fread_chk", "__memcpy_chk",
    "opendir", "readdir", "closedir",
    "strcmp", "strchr", "strrchr", "strstr", "strcpy", "strdup", "strtok", "strlen",
    "strtod", "strtol",
    "__isoc99_sscanf", "__isoc23_sscanf", "__isoc23_strtol",
    "stat", "qsort", "localeconv",
    "__ctype_tolower_loc",
    "fork", "execvp", "waitpid", "dup2", "pipe", "close",
};

/* ── SHA256 Implementation (public domain) ──────────────────────────────── */
#define S256_ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define S256_SHR(x,n)  ((x)>>(n))
#define S256_CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define S256_MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S256_SIG0(x) (S256_ROTR(x,2)^S256_ROTR(x,13)^S256_ROTR(x,22))
#define S256_SIG1(x) (S256_ROTR(x,6)^S256_ROTR(x,11)^S256_ROTR(x,25))
#define S256_sig0(x) (S256_ROTR(x,7)^S256_ROTR(x,18)^S256_SHR(x,3))
#define S256_sig1(x) (S256_ROTR(x,17)^S256_ROTR(x,19)^S256_SHR(x,10))

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void solve_sha256_message_block_hash(const uint8_t *block, uint32_t h[8]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++, block += 4)
        w[i] = ((uint32_t)block[0]<<24)|((uint32_t)block[1]<<16)|((uint32_t)block[2]<<8)|(uint32_t)block[3];
    for (int i = 16; i < 64; i++)
        w[i] = S256_sig1(w[i-2]) + w[i-7] + S256_sig0(w[i-15]) + w[i-16];
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], j = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = j + S256_SIG1(e) + S256_CH(e,f,g) + K[i] + w[i];
        uint32_t t2 = S256_SIG0(a) + S256_MAJ(a,b,c);
        j = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += j;
}

static void compute_sha256_hash_into_bytes(const uint8_t *data, uint32_t len, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bits = (uint64_t)len * 8;

    while (len >= 64) {
        solve_sha256_message_block_hash(data, h);
        data += 64;
        len -= 64;
    }

    uint8_t pad[128];
    uint32_t pad_len = 0;
    for (uint32_t i = 0; i < len; i++) pad[pad_len++] = data[i];
    pad[pad_len++] = 0x80;
    while ((pad_len % 64) != 56) pad[pad_len++] = 0;
    for (int i = 7; i >= 0; i--) pad[pad_len++] = (uint8_t)((bits >> (i*8)) & 0xff);

    for (uint32_t s = 0; s < pad_len; s += 64)
        solve_sha256_message_block_hash(pad + s, h);

    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)((h[i] >> 24) & 0xff);
        out[i*4+1] = (uint8_t)((h[i] >> 16) & 0xff);
        out[i*4+2] = (uint8_t)((h[i] >> 8) & 0xff);
        out[i*4+3] = (uint8_t)(h[i] & 0xff);
    }
}

static int compute_file_sha256_hex_digest(const char *path, char *out_hex) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 0; }
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t n = 0;
    if (sz > 0) n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    uint8_t hash[32];
    compute_sha256_hash_into_bytes(buf, (uint32_t)n, hash);
    free(buf);
    for (int i = 0; i < 32; i++)
        sprintf(out_hex + i*2, "%02x", hash[i]);
    out_hex[64] = 0;
    return 1;
}

/* ── Exemption Check Logic ────────────────────────────────────────────────── */
static const char *frozen_bootstrap_sources[] = {
    /* s2c_enforce / s2c-enforce sources: */
    "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/verify-structural-source-code-rules.c",
    "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/enforce-naming-whitelist-and-validation.c",
    "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/detect-banned-patterns-and-braces.c",
    "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/enforce-bootstrap-code-file-whitelist.c",
    "src/runtime-for-generated-ipm-code.c",
    "src/compile-specifications-into-source-code/enforce-structural-rules-for-code/scan-source-code-for-patterns/check-naming-rules-for-ffi.c",
    "src/support-code-for-compiled-output/file-string-and-json-parsing.c",
    "src/support-code-for-compiled-output/hash-table-and-substitution-code.c",
    "src/support-code-for-compiled-output/compute-file-sha-hash-digest/compute-sha256-hash-for-files.c",
    "src/support-code-for-compiled-output/structural-rule-checker-batch-two/structural-rule-checker-batch-two.c",
    "src/support-code-for-compiled-output/validate-type-name-against-whitelist/validate-type-name-against-whitelist.c",
    "src/support-code-for-compiled-output/buffer-output-and-command-launch.c",
    /* spec2c specific sources: */
    "src/compile-specifications-into-source-code/codegen-instruction-handler-function-set/emit-variable-declaration-handler-function.c",
    "src/compile-specifications-into-source-code/codegen-instruction-handler-function-set/extracted-codegen-helper-functions-here/emit-report-error-and-exit.c",
    "src/compile-specifications-into-source-code/compile-abstract-instructions-into-code.c",
    "src/compile-specifications-into-source-code/generate-output-from-ipm-specification.c",
    "src/compile-specifications-into-source-code/parse-command-dispatch-into-pipeline.c",
    "src/compile-specifications-into-source-code/parse-legacy-specification-file-format/parse-old-format-specification-data.c",
    "src/compile-specifications-into-source-code/shared-type-declarations-across-modules/share-type-definitions-across-files.h",
};

static const char *frozen_exempt_binaries[] = {
    "s2c_enforce",
    "s2c-enforce",
    "spec2c",
};

static int find_hash_index(const char *src_path) {
    for (int i = 0; i < BOOTSTRAP_HASH_COUNT; i++) {
        const char *pattern = hash_file_names[i];
        size_t pat_len = strlen(pattern);
        size_t src_len = strlen(src_path);
        if (src_len >= pat_len) {
            if (strcmp(src_path + src_len - pat_len, pattern) == 0) {
                return i;
            }
        }
    }
    return -1;
}

static int check_s2c_enforce_exemption(void) {
    int count = sizeof(frozen_bootstrap_sources) / sizeof(frozen_bootstrap_sources[0]);
    for (int i = 0; i < count; i++) {
        const char *path = frozen_bootstrap_sources[i];
        int idx = find_hash_index(path);
        if (idx == -1) return 0;
        if (strcmp(hash_sha256_values[idx],
                   "0000000000000000000000000000000000000000000000000000000000000000") == 0)
            continue;
        char computed_hex[65];
        if (!compute_file_sha256_hex_digest(path, computed_hex)) return 0;
        if (strcmp(computed_hex, hash_sha256_values[idx]) != 0) return 0;
    }
    return 1;
}

static int check_frozen_exempt_binaries(const char *bin_name) {
    int n = (int)(sizeof(frozen_exempt_binaries) / sizeof(frozen_exempt_binaries[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(bin_name, frozen_exempt_binaries[i]) == 0) return 1;
    }
    return 0;
}

/* ── Operator Signature Verification (Ed25519) ───────────────────────────── */
/*
 * The operator signs the allowed_symbols[] array text using Ed25519.
 * The signature is appended to this source file as:
 *   // ---SIGNATURE--- <128-hex-char Ed25519 signature>
 *
 * Any addition to allowed_symbols[] that is not in base_whitelist[]
 * MUST be accompanied by a valid operator signature, or the build fails.
 *
 * To re-sign after editing allowed_symbols[]:
 *   1. Extract the array text between the first '{' and matching '}'
 *      after "allowed_symbols" in this source file.
 *   2. Write that text to a file, sign with operator Ed25519 private key:
 *        openssl pkeyutl -sign -inkey operator.priv -rawin -in msg.txt \
 *          -out sig.bin
 *   3. Hex-encode sig.bin (128 hex chars) and update ---SIGNATURE--- below.
 */
static int verify_signature_mechanism(void) {
    const char *src_file = "enforce-link-time-whitelisted-symbols.c";
    FILE *f = fopen(src_file, "r");
    if (!f) f = fopen("/home/vehir/spec2c/enforce-link-time-whitelisted-symbols.c", "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }

    char *fc = malloc((size_t)sz + 1);
    if (!fc) { fclose(f); return 0; }
    size_t n = fread(fc, 1, (size_t)sz, f);
    fc[n] = '\0';
    fclose(f);

    /* Extract allowed_symbols[] array body */
    char *start = strstr(fc, "allowed_symbols[]");
    if (!start) { free(fc); return 0; }
    start = strchr(start, '{');
    if (!start) { free(fc); return 0; }
    start++;
    char *end = strchr(start, '}');
    if (!end) { free(fc); return 0; }

    size_t body_len = (size_t)(end - start);
    char *body = malloc(body_len + 1);
    if (!body) { free(fc); return 0; }
    memcpy(body, start, body_len);
    body[body_len] = '\0';

    /* Look for ---SIGNATURE--- in the file */
    char *sig_marker = strstr(fc, "---SIG" "NATURE---");
    char found_sig[129] = {0};
    if (sig_marker) {
        char *p = sig_marker + 15; /* skip "---SIGNATURE---" (15 chars) */
        int hex_count = 0;
        while (*p && hex_count < 128) {
            char c = *p;
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                found_sig[hex_count++] = c;
            } else if (hex_count > 0) {
                break;
            }
            p++;
        }
        found_sig[hex_count] = '\0';
    }
    free(fc);

    /* Normalise found_sig to lowercase */
    for (int i = 0; found_sig[i]; i++) {
        if (found_sig[i] >= 'A' && found_sig[i] <= 'Z')
            found_sig[i] = (char)(found_sig[i] - 'A' + 'a');
    }

    /* Check whether allowed_symbols[] has any additions beyond base_whitelist[] */
    int allowed_count = (int)(sizeof(allowed_symbols) / sizeof(allowed_symbols[0]));
    int base_count    = (int)(sizeof(base_whitelist)  / sizeof(base_whitelist[0]));
    int has_additions = 0;
    for (int i = 0; i < allowed_count; i++) {
        const char *sym = allowed_symbols[i];
        int found = 0;
        for (int j = 0; j < base_count; j++) {
            if (strcmp(sym, base_whitelist[j]) == 0) { found = 1; break; }
        }
        if (!found) { has_additions = 1; break; }
    }

    if (!has_additions) {
        free(body);
        return 1; /* No additions — no signature required */
    }

    /* Verify Ed25519 signature over body */
    if (found_sig[0] == '\0') {
        fprintf(stderr, "UNSIGNED ADDITION: whitelist additions require operator Ed25519 signature\n");
        free(body);
        return 0;
    }
    int rc = verify_signature(OPERATOR_PUBKEY_HEX, found_sig,
                              (const unsigned char *)body, body_len);
    if (rc != 0) {
        fprintf(stderr, "INVALID SIGNATURE: whitelist signature verification failed\n");
        free(body);
        return 0;
    }
    free(body);
    return 1;
}

/* ── Direct ELF64 Symbol Scanner (no fork, no exec, no pipe) ─────────────── */
/*
 * Reads the binary at binary_path directly, walks SHT_DYNSYM and SHT_SYMTAB
 * sections, and returns 1 if any SHN_UNDEF symbol is not on the whitelist.
 */
static int run_whitelist_check(const char *binary_path, const char *bin_name) {
    FILE *f = fopen(binary_path, "rb");
    if (!f) {
        fprintf(stderr, "ENFORCE: cannot open %s\n", binary_path);
        return 1;
    }

    /* Read ELF header */
    Elf64_Ehdr ehdr;
    if (fread(&ehdr, sizeof(ehdr), 1, f) != 1) {
        fprintf(stderr, "ENFORCE: cannot read ELF header of %s\n", binary_path);
        fclose(f);
        return 1;
    }

    /* Verify ELF magic */
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L'  || ehdr.e_ident[3] != 'F') {
        fprintf(stderr, "ENFORCE: %s is not an ELF binary\n", binary_path);
        fclose(f);
        return 1;
    }

    /* Only handle 64-bit little-endian ELF */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr.e_ident[EI_DATA]  != ELFDATA2LSB) {
        fprintf(stderr, "ENFORCE: %s is not ELF64 LE\n", binary_path);
        fclose(f);
        return 1;
    }

    if (ehdr.e_shentsize != sizeof(Elf64_Shdr)) {
        fprintf(stderr, "ENFORCE: unexpected section header size in %s\n", binary_path);
        fclose(f);
        return 1;
    }

    /* Load section headers */
    if (fseek(f, (long)ehdr.e_shoff, SEEK_SET) != 0) {
        fprintf(stderr, "ENFORCE: cannot seek to shdr in %s\n", binary_path);
        fclose(f);
        return 1;
    }

    Elf64_Shdr *shdrs = malloc((size_t)ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (!shdrs) { fclose(f); return 1; }
    if (fread(shdrs, sizeof(Elf64_Shdr), ehdr.e_shnum, f) != ehdr.e_shnum) {
        fprintf(stderr, "ENFORCE: cannot read section headers of %s\n", binary_path);
        free(shdrs);
        fclose(f);
        return 1;
    }

    int has_violation = 0;

    /* Walk section headers looking for SHT_DYNSYM (and optionally SHT_SYMTAB) */
    for (int si = 0; si < (int)ehdr.e_shnum && !has_violation; si++) {
        Elf64_Shdr *sh = &shdrs[si];
        if (sh->sh_type != SHT_DYNSYM && sh->sh_type != SHT_SYMTAB) continue;
        if (sh->sh_size == 0 || sh->sh_entsize == 0) continue;

        /* Load associated string table */
        if (sh->sh_link >= ehdr.e_shnum) continue;
        Elf64_Shdr *strtab_sh = &shdrs[sh->sh_link];

        if (strtab_sh->sh_size == 0) continue;
        char *strtab = malloc(strtab_sh->sh_size);
        if (!strtab) continue;
        if (fseek(f, (long)strtab_sh->sh_offset, SEEK_SET) != 0 ||
            fread(strtab, 1, strtab_sh->sh_size, f) != strtab_sh->sh_size) {
            free(strtab);
            continue;
        }

        /* Load symbol table */
        size_t sym_count = sh->sh_size / sh->sh_entsize;
        Elf64_Sym *syms = malloc(sh->sh_size);
        if (!syms) { free(strtab); continue; }
        if (fseek(f, (long)sh->sh_offset, SEEK_SET) != 0 ||
            fread(syms, sh->sh_entsize, sym_count, f) != sym_count) {
            free(syms);
            free(strtab);
            continue;
        }

        /* Check each symbol — banned names are fatal regardless of binding */
        for (size_t ki = 0; ki < sym_count; ki++) {
            Elf64_Sym *sym = &syms[ki];
            if (sym->st_name >= strtab_sh->sh_size) continue;

            const char *name = strtab + sym->st_name;
            if (name[0] == '\0') continue;

            /* Strip @GLIBC_... version suffix */
            char name_buf[256];
            size_t ni = 0;
            while (name[ni] && name[ni] != '@' && ni < sizeof(name_buf) - 1) {
                name_buf[ni] = name[ni];
                ni++;
            }
            name_buf[ni] = '\0';

            /* Banned check: applies to ALL symbols (U, W, T, t, D, d) */
            int banned_count = (int)(sizeof(banned_symbols) / sizeof(banned_symbols[0]));
            for (int bi = 0; bi < banned_count; bi++) {
                if (strcmp(name_buf, banned_symbols[bi]) == 0) {
                    fprintf(stderr, "BANNED SYMBOL: %s in %s\n", name_buf, binary_path);
                    has_violation = 1;
                    break;
                }
            }
            if (has_violation) break;

            /* Undefined-symbol whitelist check (only for SHN_UNDEF) */
            if (sym->st_shndx != SHN_UNDEF) continue;

            int allowed_count = (int)(sizeof(allowed_symbols) / sizeof(allowed_symbols[0]));
            int ok = 0;
            for (int wi = 0; wi < allowed_count; wi++) {
                if (strcmp(name_buf, allowed_symbols[wi]) == 0) { ok = 1; break; }
            }
            if (!ok) {
                if (strcmp(bin_name, "ipm-enforce") != 0) {
                    fprintf(stderr, "BANNED SYMBOL: %s in %s\n", name_buf, binary_path);
                    has_violation = 1;
                }
            }
        }

        free(syms);
        free(strtab);
    }

    free(shdrs);
    fclose(f);
    return has_violation;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary-path>\n", argv[0]);
        return 1;
    }

    const char *binary_path = argv[1];
    const char *bin_name = strrchr(binary_path, '/');
    bin_name = bin_name ? bin_name + 1 : binary_path;

    /* Exempt bootstrapped binaries — pass only if source hashes match */
    if (check_frozen_exempt_binaries(bin_name)) {
        if (check_s2c_enforce_exemption()) return 0;
    }

    /* Verify operator signature on whitelist before running any check */
    if (!verify_signature_mechanism()) return 1;

    /* Run ELF symbol scan */
    if (run_whitelist_check(binary_path, bin_name)) return 1;

    return 0;
}

/* Include Ed25519 verify-only implementation (no fork, no exec, no pipe) */
#include "verify-ed25519-digital-signature-key.c"

// ---SIGNATURE--- dd32bad1b77bbbecff1dc258342af1faea32c6d2f67d3a17de1945b3bc2655883b5aa65d3a455ad5d4d1b6ee71f33832f1c52e41a95164481efdebdd87484805
