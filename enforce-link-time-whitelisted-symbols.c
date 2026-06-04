// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

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
    "lstat", "execlp",
    "lstat", "execlp",
};

/* ── Bootstrap weak stubs (operator-signed, pinned by integrity manifest) ──
   These 6 symbols are the ONLY permitted weak definitions. Any other WEAK
   symbol in any binding → FATAL (Error 3 fix: class-level, not name-blacklist). */
static const char *permitted_weak_stubs[] = {
    /* Operator-signed bootstrap stubs (runtime-weak-stubs-part-two.c) */
    "match_name_against_exemption_table",
    "check_non_source_file_allowlist",
    "match_name_against_bootstrap_list",
    "check_single_file_for_violations",
    "search_for_unused_function_code",
    "check_name_against_allowed_whitelist",
    /* Operator-signed IPM runtime stubs (runtime-for-generated-ipm-code.c) */
    "validate_file_stem_naming_dfa",
    "find_every_main_function_block",
    "read_allowed_names_from_file",
    "read_banned_patterns_from_file",
    "load_non_source_file_allowlist",
    "load_bootstrap_whitelist_from_disk",
    "load_operator_signed_exemption_table",
    /* Compiler/linker-injected WEAK symbols */
    "_ITM_deregisterTMCloneTable",
    "_ITM_registerTMCloneTable",
    "__cxa_finalize",
    "__gmon_start__",
    "data_start",
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
    "lstat", "execlp",
    "lstat", "execlp",
};

/* ── Exemption Check Logic ────────────────────────────────────────────────── */
static const char *frozen_exempt_binaries[] = {
    "s2c_enforce",
    "s2c-enforce",
    "spec2c",
};

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
    const char *src_name = "enforce-link-time-whitelisted-symbols.c";
    char src_path[4096];
    char exe_path[4096];
    FILE *f = NULL;
    /* Resolve our own source from the executable's own location (SOUL §7:
       zero hardcode — derive at runtime, never a machine-specific path). */
    ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (exe_len > 0) {
        exe_path[exe_len] = '\0';
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - exe_path) + 1;
            if (dir_len + strlen(src_name) < sizeof(src_path)) {
                memcpy(src_path, exe_path, dir_len);
                memcpy(src_path + dir_len, src_name, strlen(src_name) + 1);
                f = fopen(src_path, "r");
            }
        }
    }
    if (!f) f = fopen(src_name, "r"); /* fallback: cwd-relative (build contract) */
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

    /* TLS segment detection: pre-main callback vector (XZ/ShadowChain class).
       Read program headers BEFORE section headers — must FATAL early. */
    if (ehdr.e_phnum > 0 && ehdr.e_phentsize == sizeof(Elf64_Phdr)) {
        Elf64_Phdr *phdrs = malloc((size_t)ehdr.e_phnum * sizeof(Elf64_Phdr));
        if (phdrs) {
            if (fseek(f, (long)ehdr.e_phoff, SEEK_SET) == 0 &&
                fread(phdrs, sizeof(Elf64_Phdr), ehdr.e_phnum, f) == ehdr.e_phnum) {
                for (int pi = 0; pi < ehdr.e_phnum; pi++) {
                    if (phdrs[pi].p_type == PT_TLS) {
                        fprintf(stderr, "TLS DETECTED: %s has TLS segment (pre-main callback vector)\n",
                            binary_path);
                        free(phdrs); fclose(f); return 1;
                    }
                    /* Executable stack: ROP gadget surface, NX bypass vector */
                    if (phdrs[pi].p_type == PT_GNU_STACK && (phdrs[pi].p_flags & PF_X)) {
                        fprintf(stderr, "EXECUTABLE STACK: %s has executable stack\n", binary_path);
                        free(phdrs); fclose(f); return 1;
                    }
                }
            }
            free(phdrs);
        }
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

            /* IFUNC detection: GNU indirect functions are hijack vectors (XZ-style).
               No IFUNC symbol is permitted in any enforcer binary. */
            if (ELF64_ST_TYPE(sym->st_info) == STT_GNU_IFUNC) {
                fprintf(stderr, "IFUNC SYMBOL: %s in %s\n", name_buf, binary_path);
                has_violation = 1;
                break;
            }

            /* WEAK binding check: only explicit operator-signed stubs permitted.
               allowed_symbols[] is for UNDEF references, NOT for WEAK definitions.
               Structural backstop: a permitted WEAK name that shadows a dangerous
               libc function (memcpy, malloc, strcmp, etc.) is STILL FATAL — crossing
               the permit list with a dangerous name is a configuration error. */
            if (ELF64_ST_BIND(sym->st_info) == STB_WEAK) {
                int is_permitted = 0;
                int sc = (int)(sizeof(permitted_weak_stubs) / sizeof(permitted_weak_stubs[0]));
                for (int si = 0; si < sc; si++) {
                    if (!strcmp(name_buf, permitted_weak_stubs[si])) { is_permitted = 1; break; }
                }
                if (is_permitted) {
                    /* Backstop: WEAK-permitted names must not shadow dangerous libc */
                    int ac = (int)(sizeof(allowed_symbols) / sizeof(allowed_symbols[0]));
                    for (int ai = 0; ai < ac; ai++) {
                        if (!strcmp(name_buf, allowed_symbols[ai])) {
                            /* Only flag if it's a dangerous name, not toolchain glue */
                            if (name_buf[0] != '_') {
                                fprintf(stderr, "WEAK SHADOW: %s in %s\n", name_buf, binary_path);
                                has_violation = 1; break;
                            }
                        }
                    }
                } else {
                    fprintf(stderr, "WEAK SYMBOL: %s in %s\n", name_buf, binary_path);
                    has_violation = 1;
                }
                if (has_violation) break;
            }

            /* Undefined-symbol whitelist check (only for SHN_UNDEF) */
            if (sym->st_shndx != SHN_UNDEF) continue;
            /* Exempt bootstrap binaries + ipm-enforce from UNDEF whitelist check */
            if (check_frozen_exempt_binaries(bin_name)) continue;
            if (strcmp(bin_name, "ipm-enforce") == 0) continue;

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

    /* Exempt bootstrapped binaries */
    check_frozen_exempt_binaries(bin_name);

    /* Verify operator signature on whitelist before running any check */
    if (!verify_signature_mechanism()) return 1;

    /* Run ELF symbol scan */
    if (run_whitelist_check(binary_path, bin_name)) return 1;

    return 0;
}

/* Include Ed25519 verify-only implementation (no fork, no exec, no pipe) */
#include "verify-ed25519-digital-signature-key.c"

// ---SIGNATURE--- dd32bad1b77bbbecff1dc258342af1faea32c6d2f67d3a17de1945b3bc2655883b5aa65d3a455ad5d4d1b6ee71f33832f1c52e41a95164481efdebdd87484805
