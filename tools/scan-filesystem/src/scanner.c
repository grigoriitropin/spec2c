// SPDX-License-Identifier: Apache-2.0
// scan-filesystem — fast directory tree walker, JSON lines output
// Usage: scan-filesystem <root> [--max-depth N]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>

static int max_depth = 64;

static const char *type_char(int mode) {
    if (S_ISDIR(mode))  return "dir";
    if (S_ISREG(mode))  return "file";
    if (S_ISLNK(mode))  return "link";
    if (S_ISFIFO(mode)) return "fifo";
    if (S_ISSOCK(mode)) return "sock";
    return "other";
}

static int walk_callback(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf) {
    if (ftwbuf->level > max_depth) return FTW_SKIP_SUBTREE;
    if (ftwbuf->level == 0) return 0; /* skip root itself */

    /* one JSON line per entry */
    fprintf(stdout, "{\"path\":\"%s\",\"type\":\"%s\",\"size\":%ld,\"depth\":%d}\n",
            fpath, type_char(sb->st_mode),
            (long)sb->st_size, ftwbuf->level);
    return 0;
}

int main(int argc, char **argv) {
    const char *root = ".";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--max-depth") && i + 1 < argc)
            max_depth = atoi(argv[++i]);
        else if (argv[i][0] != '-')
            root = argv[i];
    }

    if (nftw(root, walk_callback, 64, FTW_PHYS) != 0) {
        fprintf(stderr, "scan-filesystem: error walking %s\n", root);
        return 1;
    }
    return 0;
}
