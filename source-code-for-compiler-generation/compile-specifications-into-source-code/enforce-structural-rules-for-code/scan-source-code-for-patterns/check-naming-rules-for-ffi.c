// SPDX-License-Identifier: Apache-2.0
// FFI name validator — callable from IPM via extern_imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cjson/cJSON.h>
#include "verify-structural-source-code-rules.h"

static const char *banned_type_words[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

static fn_entry_t fns[512];
static int fn_qty = 0;
static inc_entry_t incs[128];
static int inc_qty = 0;
static int total_main = 0;

extern int validate_file_stem_naming_dfa(const char *name_arg);
extern int find_every_main_function_block(const char *path);

const char *check_name_following_soul_rules(const char *what, const char *name, const char *fp) {
    (void)fp;
    if (!name || !name[0]) return NULL;
    if (!strcmp(name, "main")) return NULL;
    if (!strcmp(name, "while") || !strcmp(name, "for") ||
        !strcmp(name, "if") || !strcmp(name, "switch")) return NULL;

    char is_file = (what[0] == 'f' && what[1] == 'i');
    char is_dir  = (what[0] == 'd' && what[1] == 'i');
    char sep_str[2] = {(is_file || is_dir) ? '-' : '_', 0};
    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0;
    char *tok = strtok(buf, sep_str);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char *err = (char *)malloc(256);
            snprintf(err, 256, "word '%s' too short (min 3)", tok);
            return (const char *)err;
        }
        for (int i = 0; banned_type_words[i]; i++)
            if (!strcmp(tok, banned_type_words[i])) {
                char *err = (char *)malloc(256);
                snprintf(err, 256, "banned word '%s'", tok);
                return (const char *)err;
            }
        tok = strtok(NULL, sep_str);
    }
    if (words != 5) {
        char *err = (char *)malloc(256);
        snprintf(err, 256, "has %d words (need 5)", words);
        return (const char *)err;
    }
    return NULL;
}

void initialize_naming_rules_enforcer_ffi(const char *srcdir) {
    const char *data_dir = srcdir;
    char sd[4096];
    snprintf(sd, sizeof(sd), "%s/src", srcdir);
    struct stat st_sd;
    if (!stat(sd, &st_sd) && S_ISDIR(st_sd.st_mode)) {
        data_dir = sd;
    }
    read_allowed_names_from_file(data_dir);
    read_banned_patterns_from_file(data_dir);
    load_non_source_file_allowlist(data_dir);
    load_bootstrap_whitelist_from_disk(data_dir);
    load_operator_signed_exemption_table(srcdir);
}

const char *match_exemption_table_name_ffi(const char *name) {
    const char *r = match_name_against_exemption_table(name);
    return r ? r : "";
}

int check_bootstrap_source_whitelist_ffi(const char *name, const char *sub, const char *dirpath) {
    int is_c_or_h = (strlen(name) > 2 && (!strcmp(name + strlen(name) - 2, ".c") || !strcmp(name + strlen(name) - 2, ".h")));
    if (!is_c_or_h) {
        size_t dn_len = strlen(name);
        if (!(dn_len > 4 && !strcmp(name + dn_len - 4, ".ipm"))) {
            if (!check_non_source_file_allowlist(name)) {
                fprintf(stderr, "FATAL: non-source file in %s: %s\n", dirpath, name);
                exit(1);
            }
            return 1;
        }
    }
    if (!match_name_against_bootstrap_list(name)) {
        size_t dn_len2 = strlen(name);
        if (!(dn_len2 > 4 && !strcmp(name + dn_len2 - 4, ".ipm"))) {
            fprintf(stderr, "spec2c: SOUL §7: %s not in bootstrap C whitelist\n  → rewrite this functionality as an IPM module, the C bootstrap is frozen\n", sub);
            exit(1);
        }
    }
    return 0;
}

int check_allowed_name_whitelist_ffi(const char *name) {
    return check_name_against_allowed_whitelist(name);
}

int validate_file_stem_dfa_ffi(const char *fullname) {
    char stem[256];
    snprintf(stem, sizeof(stem), "%s", fullname);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';
    return validate_file_stem_naming_dfa(stem);
}

void report_file_naming_error_ffi(const char *fullname, const char *path, int err_code) {
    char stem[256]; snprintf(stem, sizeof(stem), "%s", fullname);
    char *dot = strrchr(stem, '.'); if (dot) *dot = '\0';
    if (err_code == 6)
        fprintf(stderr, "SOUL §10: file '%s' at %s has multiple dots\n  → source files must have exactly one dot (before extension)\n", fullname, path);
    else if (err_code == 7)
        fprintf(stderr, "SOUL §10: file '%s' at %s has empty stem\n", fullname, path);
    else if (err_code == 8)
        fprintf(stderr, "SOUL §10: file '%s' at %s has leading hyphen\n  → rename without leading '-'\n", fullname, path);
    else if (err_code == 1)
        fprintf(stderr, "SOUL §10: file '%s' at %s has double hyphen\n  → exactly one '-' between each word\n", fullname, path);
    else if (err_code == 2)
        fprintf(stderr, "SOUL §10: file '%s' at %s has trailing hyphen\n  → remove trailing '-'\n", fullname, path);
    else if (err_code == 4 || err_code == 5) {
        int tl = 0;
        for (int i = 0; stem[i]; i++) {
            if (stem[i] == '-') { if (tl < 3) break; tl = 0; }
            else tl++;
        }
        if (err_code == 4)
            fprintf(stderr, "SOUL §10: file '%s' at %s — token has %d chars (min 3)\n  → use full words separated by single '-'\n", fullname, path, tl);
        else
            fprintf(stderr, "SOUL §10: file '%s' at %s — last token has %d chars (min 3)\n  → each word must be ≥3 characters\n", fullname, path, tl);
    } else if (err_code == 3) {
        char bc = 0;
        for (int i = 0; stem[i]; i++)
            if (stem[i] != '-' && (stem[i] < 'a' || stem[i] > 'z')) { bc = stem[i]; break; }
        fprintf(stderr, "SOUL §10: file '%s' at %s — char '%c' (0x%02x) not allowed\n  → use only lowercase a-z and single '-'\n", fullname, path, bc, (unsigned char)bc);
    } else if (err_code == 9) {
        int tk = 0, iw = 0;
        for (int i = 0; stem[i]; i++) {
            if (stem[i] == '-') iw = 0;
            else if (!iw) { tk++; iw = 1; }
        }
        fprintf(stderr, "SOUL §10: file '%s' at %s has %d word(s) — at least 5 required\n  → rename to ≥5 hyphen-separated words\n", fullname, path, tk);
    }
    exit(1);
}

void check_directory_file_limits_ffi(const char *dirpath, int file_cnt, int dir_cnt) {
    if (file_cnt > 3) {
        fprintf(stderr, "spec2c: SOUL §7: %s has %d files (max 3)\n  → create a subdirectory and move files there\n", dirpath, file_cnt);
        exit(1);
    } else if (file_cnt == 0 && dir_cnt == 0) {
        fprintf(stderr, "FATAL: empty source directory in %s\n", dirpath);
        exit(1);
    }
}

void check_single_file_rules_ffi(const char *sub, const char *fullname) {
    int is_c = !strcmp(fullname + strlen(fullname) - 2, ".c");
    int is_source = is_c || (strlen(fullname) > 4 && !strcmp(fullname + strlen(fullname) - 4, ".ipm"));
    check_single_file_for_violations(sub, is_c, is_source, fns, &fn_qty, incs, &inc_qty);
    int has_main = find_every_main_function_block(sub);
    if (has_main) {
        int exempt = 0;
        for (int i = 0; main_exempt_directory_names[i]; i++)
            if (strstr(sub, main_exempt_directory_names[i])) { exempt = 1; break; }
        if (!exempt) total_main++;
    }
}

void check_project_main_count_ffi(const char *srcdir) {
    search_for_unused_function_code(fns, fn_qty, srcdir);
    int mc = 0;
    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) {
            int exempt = 0;
            for (int j = 0; main_exempt_directory_names[j]; j++)
                if (strstr(fns[i].file, main_exempt_directory_names[j])) { exempt = 1; break; }
            if (!exempt) mc++;
        }
    }
    if (mc != 1) {
        fprintf(stderr, "spec2c: SOUL §7: exactly one main() required, found %d\n  → keep exactly one entry point\n", mc);
        exit(1);
    }
}
