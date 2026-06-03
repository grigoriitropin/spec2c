// SPDX-License-Identifier: Apache-2.0
// ipm_builtins — I/O, JSON, error, regex, type mapping (split part 1)
#include "runtime-for-generated-ipm-code.h"
#include <unistd.h>
#include <regex.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

string read_entire_file_into_string(const char *path) {
    FILE *f = (!path || !path[0]) ? stdin : fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) { if (f != stdin) fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)size, f);
    buffer[n] = '\0';
    if (f != stdin) fclose(f);
    return buffer;
}

void write_text_string_into_file(const char *path, const char *content) {
    if (!path || !content) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

json_object* convert_text_into_json_object(string content) {
    if (!content) return NULL;
    return cJSON_Parse(content);
}

void terminate_with_json_error_output(const char *function_name, const char *instruction_index_str,
              const char *error_msg, const char *fix_hint) {
    cJSON *r = cJSON_CreateObject();
    if (!r) { fprintf(stderr, "{\"ok\":false,\"error\":\"cJSON alloc failed\"}\n"); exit(1); }
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", error_msg ? error_msg : "unknown error");
    if (function_name && function_name[0])
        cJSON_AddStringToObject(r, "function", function_name);
    if (instruction_index_str && instruction_index_str[0])
        cJSON_AddNumberToObject(r, "instruction_index", atoi(instruction_index_str));
    if (fix_hint && fix_hint[0])
        cJSON_AddStringToObject(r, "fix_hint", fix_hint);
    char *s = cJSON_PrintUnformatted(r);
    if (s) { fprintf(stderr, "%s\n", s); free(s); }
    cJSON_Delete(r);
    exit(1);
}

void builtin_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "{\"ok\":false,\"error\":\"%s\"}\n", msg ? msg : "unknown error");
    exit(1);
}

void print_error_into_stderr_output(const char *msg) {
    fprintf(stderr, "error: %s\n", msg ? msg : "unknown");
}

void terminate_with_status_return_code(int code) {
    exit(code);
}

int match_pattern_against_text_string(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB)) return 0;
    int rc = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);
    return rc == 0 ? 1 : 0;
}

const char *resolve_spec_type_into_lang(const char *t) {
    if (!t) return "void *";
    if (!strcmp(t, "void")) return "void";
    if (!strcmp(t, "string")) return "char *";
    if (!strcmp(t, "int")) return "int";
    if (!strcmp(t, "float")) return "float";
    if (!strcmp(t, "boolean")) return "int";
    if (!strcmp(t, "json_object")) return "cJSON *";
    if (!strcmp(t, "json_array")) return "cJSON *";
    if (!strcmp(t, "db_handle")) return "struct vehir_db *";
    if (!strcmp(t, "subst_table")) return "subst_table *";
    if (!strcmp(t, "string_buffer")) return "string_buffer *";
    if (!strcmp(t, "file_handle")) return "FILE *";
    if (!strcmp(t, "u8_ptr")) return "const uint8_t *";
    if (!strcmp(t, "u32")) return "uint32_t";
    return "void *";
}

cJSON *list_files_inside_directory_path(const char *path) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    DIR *d = opendir(path);
    if (!d) { cJSON_Delete(arr); return NULL; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        cJSON *entry = cJSON_CreateObject();
        if (!entry) continue;
        cJSON_AddStringToObject(entry, "name", de->d_name);
        cJSON_AddStringToObject(entry, "path", full);
        cJSON_AddBoolToObject(entry, "is_dir", S_ISDIR(st.st_mode));
        cJSON_AddItemToArray(arr, entry);
    }
    closedir(d);
    return arr;
}

/* ── SHA256 (public domain, no dependencies) ───────────────────────── */
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

void compute_sha256_hash_into_bytes(const uint8_t *data, uint32_t len, uint8_t out[32]) {
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

char *compute_file_sha256_hex_digest(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1048576) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    uint8_t hash[32];
    compute_sha256_hash_into_bytes(buf, (uint32_t)n, hash);
    free(buf);
    char *hex = malloc(65);
    if (!hex) return NULL;
    for (int i = 0; i < 32; i++)
        snprintf(hex + i*2, 3, "%02x", hash[i]);
    hex[64] = 0;
    return hex;
}
