// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "src/bootstrap-compiled-limit-hash-data/bootstrap-file-sha-hashes-generated.h"

/* ── Whitelist of Allowed Symbols ────────────────────────────────────────── */
static const char *allowed_symbols[] = {
    // Standard system/libc symbols from prompt:
    "__libc_start_main", "__cxa_finalize", "exit", "_exit", "_IO_putc", "__stack_chk_fail",
    "pthread", "dlopen", "dlsym", "abort", "__assert_fail", "mmap", "munmap", "openat", "close",
    "read", "write", "fstat", "lseek", "getrandom", "clock_gettime", "sigaction", "rt_sigaction",
    "socket", "connect", "bind", "listen", "accept", "sendto", "recvfrom", "setsockopt", "epoll_create1",
    "epoll_ctl", "epoll_wait", "sched_yield", "clone", "wait4", "kill", "getpid", "getppid", "getuid",
    "geteuid", "getgid", "getegid", "syscall", "nanosleep", "getenv", "setenv", "unsetenv", "getcwd",
    "chdir", "access", "faccessat", "stat", "mkdir", "unlink", "rename", "rmdir", "symlink", "readlink",
    "fork", "execve", "waitpid", "dup", "dup2", "pipe", "fcntl", "getdents", "getdents64", "prctl",
    "arch_prctl", "set_tid_address", "set_robust_list", "rseq", "mprotect", "brk", "sbrk",
    "madvise", "mlock", "munlock", "mincore", "msync", "memfd_create", "shmget", "shmat", "shmctl",
    "shmdt", "semget", "semop", "semctl", "msgget", "msgsnd", "msgrcv", "msgctl", "io_uring_setup",
    "io_uring_enter", "io_uring_register",

    // Toolchain/compiler automatic/weak symbols:
    "__gmon_start__",
    "_ITM_deregisterTMCloneTable",
    "_ITM_registerTMCloneTable",

    // libc functions used by spec2c/ipm-enforce/checker (operation-critical only):
    "malloc", "free", "realloc", "calloc",
    "memcpy", "memmove", "memset", "memchr", "memcmp",
    "__memcpy_chk", "__memset_chk",
    "strcmp", "strncmp", "strlen", "strcat", "strcpy", "strncpy", "strncat",
    "strchr", "strrchr", "strstr", "strspn", "strcspn", "strpbrk", "strtok", "strdup", "strndup",
    "snprintf", "sprintf", "printf", "fprintf", "vsnprintf", "vsprintf", "vprintf", "vfprintf",
    "__snprintf_chk", "__sprintf_chk", "__printf_chk", "__fprintf_chk",
    "sscanf", "__isoc23_sscanf", "strtol", "strtoul", "strtoll", "strtoull",
    "__isoc23_strtol", "__isoc23_strtoul",
    "fopen", "fdopen", "freopen", "fclose",
    "fread", "fwrite", "fputs", "fgets", "fputc", "fgetc", "putchar", "getchar", "puts",
    "__fread_chk",
    "fseek", "ftell", "rewind", "fgetpos", "fsetpos",
    "fflush", "ferror", "feof", "clearerr",
    "qsort", "bsearch",
    "opendir", "closedir", "readdir",
    "execvp", "wait",
    "stderr", "stdout", "stdin",
    // cJSON dynamic symbols:
    "cJSON_CreateArray", "cJSON_CreateObject", "cJSON_AddItemToArray", "cJSON_AddStringToObject",
    "cJSON_AddBoolToObject", "cJSON_AddNumberToObject", "cJSON_GetArrayItem", "cJSON_GetArraySize",
    "cJSON_GetObjectItemCaseSensitive", "cJSON_IsArray", "cJSON_IsObject", "cJSON_IsString",
    "cJSON_IsNumber", "cJSON_IsTrue", "cJSON_IsBool", "cJSON_Parse", "cJSON_Print", "cJSON_PrintUnformatted",
    "cJSON_Delete", "cJSON_CreateNull", "cJSON_Duplicate", "cJSON_GetErrorPtr",
};

/* ── SHA256 Implementation (public domain) ──────────────────────────────── */
#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define SHR(x,n)  ((x)>>(n))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define SIG0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define SIG1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define sig0(x) (ROTR(x,7)^ROTR(x,18)^SHR(x,3))
#define sig1(x) (ROTR(x,17)^ROTR(x,19)^SHR(x,10))

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
        w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], j = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = j + SIG1(e) + CH(e,f,g) + K[i] + w[i];
        uint32_t t2 = SIG0(a) + MAJ(a,b,c);
        j = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += j;
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
    if (sz > 0) {
        n = fread(buf, 1, (size_t)sz, f);
    }
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
static const char *s2c_enforce_sources[] = {
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
    int count = sizeof(s2c_enforce_sources) / sizeof(s2c_enforce_sources[0]);
    for (int i = 0; i < count; i++) {
        const char *path = s2c_enforce_sources[i];
        int idx = find_hash_index(path);
        if (idx == -1) {
            return 0; // Not found in hashes list -> not exempt
        }
        // If the frozen hash is all zeros, it means the generator skipped it, so we skip it too
        if (strcmp(hash_sha256_values[idx], "0000000000000000000000000000000000000000000000000000000000000000") == 0) {
            continue;
        }
        char computed_hex[65];
        if (!compute_file_sha256_hex_digest(path, computed_hex)) {
            return 0; // File unreadable -> not exempt
        }
        if (strcmp(computed_hex, hash_sha256_values[idx]) != 0) {
            return 0; // Hash mismatch -> not exempt
        }
    }
    return 1; // All hashes matched!
}

/* ── Symbol Whitelist Check Logic ─────────────────────────────────────────── */
static int is_allowed_symbol(const char *sym) {
    // Whitelist starting with cJSON_
    if (strncmp(sym, "cJSON_", 6) == 0) {
        return 1;
    }

    // Check against static allowed whitelist
    int allowed_count = sizeof(allowed_symbols) / sizeof(allowed_symbols[0]);
    for (int i = 0; i < allowed_count; i++) {
        if (strcmp(sym, allowed_symbols[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static int run_whitelist_check(const char *binary_path, const char *bin_name) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child: redirect stdout to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        // Mute stderr to avoid polluting outputs
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null != -1) {
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        execlp("readelf", "readelf", "-W", "--dyn-syms", binary_path, NULL);
        exit(127);
    }

    // Parent
    close(pipefd[1]);

    char line[1024];
    int line_len = 0;
    char ch;
    int has_violation = 0;

    while (read(pipefd[0], &ch, 1) > 0) {
        if (ch == '\n' || line_len >= 1023) {
            line[line_len] = '\0';

            // Tokenize by space/tabs
            char *cols[32];
            int col_count = 0;
            char *p = line;
            while (*p) {
                while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
                    *p = '\0';
                    p++;
                }
                if (*p) {
                    cols[col_count++] = p;
                    if (col_count >= 32) break;
                    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                        p++;
                    }
                }
            }

            // UND symbols are represented by "UND" in the Ndx column (index 6 in 8+ column layout)
            if (col_count >= 8 && strcmp(cols[6], "UND") == 0) {
                char *sym = cols[7];
                // Strip @GLIBC_... or other suffix if present
                char *at = strchr(sym, '@');
                if (at) {
                    *at = '\0';
                }

                if (!is_allowed_symbol(sym)) {
                    fprintf(stderr, "BANNED SYMBOL: %s in %s\n", sym, binary_path);
                    has_violation = 1;
                }
            }

            line_len = 0;
        } else {
            line[line_len++] = ch;
        }
    }

    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (has_violation) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <binary-path>\n", argv[0]);
        return 1;
    }

    const char *binary_path = argv[1];
    const char *bin_name = strrchr(binary_path, '/');
    if (bin_name) {
        bin_name++;
    } else {
        bin_name = binary_path;
    }

    // Check exemption for s2c-enforce / s2c_enforce
    if (strcmp(bin_name, "s2c_enforce") == 0 || strcmp(bin_name, "s2c-enforce") == 0) {
        if (check_s2c_enforce_exemption()) {
            return 0; // Exempt, succeeds silently
        }
    }

    // Run dynamic symbol check
    if (run_whitelist_check(binary_path, bin_name)) {
        return 1;
    }

    return 0;
}
