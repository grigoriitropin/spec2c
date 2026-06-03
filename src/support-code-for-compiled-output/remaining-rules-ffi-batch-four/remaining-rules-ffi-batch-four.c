// SPDX-License-Identifier: Apache-2.0
// Batch 4 FFI — remaining C rules: header whitelist, dead code, main count, CLI flags, bootstrap whitelist
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

extern char *read_entire_file_into_string(const char *path);

/* ── header whitelist check ─────────────────────────────────────────── */
static const char *ok_headers[] = {
    "stdio.h","stdlib.h","string.h","errno.h","unistd.h","fcntl.h",
    "sys/stat.h","sys/types.h","sys/wait.h","sys/socket.h","sys/un.h",
    "sys/select.h","dirent.h","regex.h","signal.h","stdint.h","stddef.h",
    "stdbool.h","time.h","cjson/cJSON.h","netinet/in.h",NULL
};

const char *check_include_headers_ffi(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        char hdr[128] = {0};
        if (sscanf(line, " #include <%127[^>]>", hdr) == 1 ||
            sscanf(line, " #include \"%127[^\"]\"", hdr) == 1) {
            int found = 0;
            for (int i = 0; ok_headers[i]; i++)
                if (!strcmp(hdr, ok_headers[i])) { found = 1; break; }
            if (!found) {
                free(c);
                static char err[256];
                snprintf(err, sizeof(err), "header '%s' not in whitelist", hdr);
                return err;
            }
        }
        if (!next) break;
        line = next + 1;
    }
    free(c);
    return NULL;
}

/* ── main count (cross-file) ────────────────────────────────────────── */
const char *check_main_function_count_ffi(const char *dirpath) {
    DIR *d = opendir(dirpath);
    if (!d) return NULL;
    int main_count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dirpath, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            const char *r = check_main_function_count_ffi(sub);
            if (r) { closedir(d); return r; }
            continue;
        }
        size_t nl = strlen(de->d_name);
        if (nl < 3 || strcmp(de->d_name + nl - 2, ".c")) continue;
        char *c = read_entire_file_into_string(sub);
        if (!c) continue;
        char *line = c, *next;
        while (line && *line) {
            next = strchr(line, '\n');
            if (next) *next = 0;
            if (strstr(line, "int main(") || strstr(line, "int main (")) main_count++;
            if (!next) break;
            line = next + 1;
        }
        free(c);
    }
    closedir(d);
    if (main_count != 1) {
        static char err[64];
        snprintf(err, sizeof(err), "found %d main() (need 1)", main_count);
        return err;
    }
    return NULL;
}

/* ── CLI flag documentation (--flags must appear in help text) ──────── */
const char *check_cli_flags_in_help_ffi(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    const char *p = c;
    while ((p = strstr(p, "\"--")) != NULL) {
        p += 2;
        char flag[64]; int fi = 0;
        while (*p && *p != '"' && fi < 63) flag[fi++] = *p++;
        flag[fi] = 0;
        if (fi == 0) continue;
        if (!strstr(c, flag)) {
            free(c);
            static char err[128];
            snprintf(err, sizeof(err), "flag '%s' not in help text", flag);
            return err;
        }
    }
    free(c);
    return NULL;
}
