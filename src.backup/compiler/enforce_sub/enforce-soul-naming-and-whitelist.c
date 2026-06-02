// SPDX-License-Identifier: Apache-2.0
// enforce source helpers — structural checks on individual lines/functions

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

const char *soful = "SOUL §10 (immutable): 'Exactly 5 words, hyphen-separated. No more, no less. "
    "No type words. Banned: service,server,daemon,library,tool,binary,package,module,"
    "system,utility,application,program,process,worker. "
    "Describes WHAT it does, not what it is or how it is built. "
    "English only. Full words over abbreviations. Self-documenting.'";

/* ── naming whitelist ──────────────────────────────────────────────── */
static struct { char name[128]; } allowed[512]; static int allowed_qty;

int match_source_code_header_filename(const char *name) {
    size_t nl = strlen(name);
    return nl > 2 && (!strcmp(name + nl - 2, ".c") || !strcmp(name + nl - 2, ".h"));
}

int detect_function_definition_start_line(const char *line) {
    while (*line == ' ' || *line == '\t') return 0;
    if (*line == '/' || *line == '*' || *line == '#' || *line == '\0' || *line == '\n') return 0;
    if (strstr(line, "typedef") || strstr(line, "struct") || strstr(line, "enum")) return 0;
    return strrchr(line, '{') != NULL;
}

void pull_function_name_from_definition(const char *line, char *out, size_t sz) {
    const char *lp = strrchr(line, '(');
    if (!lp) { out[0] = 0; return; }
    while (lp > line && (*(lp-1) == ' ' || *(lp-1) == '\t' || *(lp-1) == '*')) lp--;
    const char *start = lp;
    while (start > line && *(start-1) != ' ' && *(start-1) != '\t' && *(start-1) != '*') start--;
    size_t len = (size_t)(lp - start);
    if (len >= sz) len = sz - 1;
    memcpy(out, start, len); out[len] = 0;
}

int count_lines_within_source_file(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    int lines = 0, ch; while ((ch = fgetc(f)) != EOF) if (ch == '\n') lines++;
    fclose(f); return lines;
}

int match_header_against_include_whitelist(const char *hdr) {
    const char *ok[] = {
        "stdio.h","stdlib.h","string.h","errno.h","unistd.h","fcntl.h",
        "sys/stat.h","sys/types.h","sys/wait.h","sys/socket.h","sys/un.h",
        "dirent.h","regex.h","stdint.h","stddef.h","stdbool.h","time.h",
        "cjson/cJSON.h","netinet/in.h",
        "runtime-for-generated-ipm-code.h",
        "verify-structural-source-code-rules.h",
        "common_h/share-type-definitions-across-files.h",
        "enforce_sub/verify-structural-source-code-rules.h",
        "../common_h/share-type-definitions-across-files.h",
        "vehir_lib.h","ipm_json.h","share-check-types-and-declarations.h", NULL
    };
    for (int i = 0; ok[i]; i++) if (!strcmp(hdr, ok[i])) return 1;
    return 0;
}

int check_name_against_allowed_whitelist(const char *name) {
    for (int i = 0; i < allowed_qty; i++)
        if (!strcmp(allowed[i].name, name)) return 1;
    return 0;
}

void validate_name_against_soul_rules(const char *what, const char *name, const char *fp) {
    const char *banned[] = {"service","server","daemon","library","tool","binary",
        "package","module","system","utility","application","program","process","worker",NULL};
    char is_file = (what[0] == 'f' && what[1] == 'i');
    char is_dir  = (what[0] == 'd' && what[1] == 'i');
    char sep = (is_file || is_dir) ? '-' : '_';
    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0; char *tok = strtok(buf, &sep);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char eb[2048]; snprintf(eb, sizeof(eb), "SOUL §10: word '%s' in %s '%s' at %s too short.\n%s", tok, what, name, fp, soful);
            report_fatal_error_and_exit(eb);
        }
        for (int i = 0; banned[i]; i++) if (!strcmp(tok, banned[i])) {
            char eb[2048]; snprintf(eb, sizeof(eb), "SOUL §10: word '%s' in %s '%s' at %s is banned.\n%s", tok, what, name, fp, soful);
            report_fatal_error_and_exit(eb);
        }
        tok = strtok(NULL, &sep);
    }
    if (words != 5) {
        char eb[2048]; snprintf(eb, sizeof(eb), "SOUL §10: %s '%s' at %s has %d words (need 5, %s-separated).\n%s",
            what, name, fp, words, (is_file || is_dir) ? "hyphen" : "underscore", soful);
        report_fatal_error_and_exit(eb);
    }
    if (!check_name_against_allowed_whitelist(name)) {
        char eb[2048]; snprintf(eb, sizeof(eb), "SOUL §10: %s '%s' at %s not in allowed-names.txt.\n%s", what, name, fp, soful);
        report_fatal_error_and_exit(eb);
    }
}

void read_allowed_names_from_file(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/allowed-names.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) report_fatal_error_and_exit("cannot open allowed-names.txt");
    char line[256];
    while (fgets(line, sizeof(line), f) && allowed_qty < 512) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0) { snprintf(allowed[allowed_qty].name, 128, "%s", line); allowed_qty++; }
    }
    fclose(f);
}
