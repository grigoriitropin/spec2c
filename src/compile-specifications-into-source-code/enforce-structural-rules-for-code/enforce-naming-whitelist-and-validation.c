// SPDX-License-Identifier: Apache-2.0
// whitelist + naming validation — shared with enforce.c

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

static const char *soful =
    "\n"
    "   ── SOUL.md §10 Naming (immutable) ───────────────────────────\n"
    "   A name is the primary documentation. It must make clear\n"
    "   what the thing does.\n"
    "\n"
    "   • Exactly 5 words, hyphen-separated. No more, no less.\n"
    "   • No type words. Banned: service, server, daemon, library,\n"
    "     tool, binary, package, module, system, utility,\n"
    "     application, program, process, worker.\n"
    "   • Describes WHAT it does, not what it is or how it is built.\n"
    "   • English only. Full words over abbreviations.\n"
    "   • Self-documenting. Among several similar tools, the\n"
    "     primary one's name reflects that — the user should not\n"
    "     have to guess which to run.\n"
    "   ──────────────────────────────────────────────────────────";

/* ── naming whitelist ──────────────────────────────────────────────── */
static struct { char name[128]; } allowed[512];
static int allowed_qty;

int match_header_against_include_whitelist(const char *hdr) {
    const char *ok[] = {
        "stdio.h","stdlib.h","string.h","errno.h","unistd.h","fcntl.h",
        "sys/stat.h","sys/types.h","sys/wait.h","sys/socket.h","sys/un.h",
        "sys/select.h",
        "dirent.h","regex.h","signal.h","stdint.h","stddef.h","stdbool.h","time.h",
        "cjson/cJSON.h","netinet/in.h",
        "runtime-for-generated-ipm-code.h",
        "ipm_builtins.h",
        "verify-structural-source-code-rules.h",
        "shared-type-declarations-across-modules/share-type-definitions-across-files.h",
        "../shared-type-declarations-across-modules/share-type-definitions-across-files.h",
        "share-check-types-and-declarations.h",
        NULL
    };
    for (int i = 0; ok[i]; i++) if (!strcmp(hdr, ok[i])) return 1;
    return 0;
}

/* ── centralized keyword / built-in exemption ─────────────────────── */
static int skip_name_validation_for_keywords(const char *name) {
    return !strcmp(name, "main") ||
           !strcmp(name, "while") || !strcmp(name, "for") ||
           !strcmp(name, "if") || !strcmp(name, "switch");
}

static int check_name_against_allowed_whitelist(const char *name) {
    if (skip_name_validation_for_keywords(name)) return 1;
    for (int i = 0; i < allowed_qty; i++)
        if (!strcmp(allowed[i].name, name)) return 1;
    return 0;
}

void validate_name_against_soul_rules(const char *what, const char *name, const char *fp) {
    if (skip_name_validation_for_keywords(name)) return;

    const char *banned_type[] = {
        "service","server","daemon","library","tool","binary",
        "package","module","system","utility","application",
        "program","process","worker",NULL
    };
    char is_file = (what[0] == 'f' && what[1] == 'i');
    char is_dir  = (what[0] == 'd' && what[1] == 'i');
    char sep = (is_file || is_dir) ? '-' : '_';

    const char *dir_note = is_dir ?
        "\n"
        "   A directory name must describe ONLY the files directly\n"
        "   inside this specific directory — not its subdirectories,\n"
        "   not its parent directories. The name must accurately\n"
        "   reflect the contents of this directory alone."
        : "";

    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0;
    char *tok = strtok(buf, &sep);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char eb[2048];
            snprintf(eb, sizeof(eb),
                "SOUL §10: word '%s' in %s '%s' at %s is too short (min 3 chars).\n%s%s",
                tok, what, name, fp, soful, dir_note);
            report_fatal_error_and_exit(eb);
        }
        for (int i = 0; banned_type[i]; i++)
            if (!strcmp(tok, banned_type[i])) {
                char eb[2048];
                snprintf(eb, sizeof(eb),
                    "SOUL §10: word '%s' in %s '%s' at %s is a banned type word.\n%s%s",
                    tok, what, name, fp, soful, dir_note);
                report_fatal_error_and_exit(eb);
            }
        tok = strtok(NULL, &sep);
    }
    if (words != 5) {
        char eb[2048];
        snprintf(eb, sizeof(eb),
            "SOUL §10: %s '%s' at %s has %d words — exactly 5 required (%s-separated).\n%s%s",
            what, name, fp, words, (is_file || is_dir) ? "hyphen" : "underscore", soful, dir_note);
        report_fatal_error_and_exit(eb);
    }
    if (!check_name_against_allowed_whitelist(name)) {
        char eb[2048];
        snprintf(eb, sizeof(eb),
            "SOUL §10: %s '%s' at %s — not in allowed-names.txt.\n%s%s",
            what, name, fp, soful, dir_note);
        report_fatal_error_and_exit(eb);
    }
}

static int validate_single_whitelist_entry_name(const char *line) {
    char buf_h[256], buf_u[256];
    snprintf(buf_h, sizeof(buf_h), "%s", line);
    snprintf(buf_u, sizeof(buf_u), "%s", line);
    int wh = 0, wu = 0, sh_h = 0, sh_u = 0;
    char *t;
    t = strtok(buf_h, "-"); while (t) { wh++; if ((int)strlen(t) < 3) sh_h++; t = strtok(NULL, "-"); }
    t = strtok(buf_u, "_"); while (t) { wu++; if ((int)strlen(t) < 3) sh_u++; t = strtok(NULL, "_"); }
    int ok = (wh == 5 && sh_h == 0) || (wu == 5 && sh_u == 0);
    if (ok) {
        const char *banned[] = {"service","server","daemon","library","tool","binary",
            "package","module","system","utility","application","program","process","worker",NULL};
        char tokbuf[256];
        snprintf(tokbuf, sizeof(tokbuf), "%s", line);
        char *sep2 = wh == 5 ? "-" : "_";
        t = strtok(tokbuf, sep2);
        while (t && ok) {
            for (int i = 0; banned[i]; i++)
                if (!strcmp(t, banned[i])) { ok = 0; break; }
            t = strtok(NULL, sep2);
        }
    }
    return ok;
}

void read_allowed_names_from_file(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/allowed-names.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) report_fatal_error_and_exit("cannot open allowed-names.txt");
    char line[256];
    while (fgets(line, sizeof(line), f) && allowed_qty < 512) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0) {
            if (line[0] == '#') continue;
            if (!skip_name_validation_for_keywords(line) && !validate_single_whitelist_entry_name(line)) {
                char eb[2048];
                snprintf(eb, sizeof(eb),
                    "SOUL §10: whitelist entry '%s' is invalid.\n"
                    "\n"
                    "   ── SOUL.md §10 Naming (immutable) ───────────────────────────\n"
                    "   A name is the primary documentation. It must make clear\n"
                    "   what the thing does.\n"
                    "\n"
                    "   • Exactly 5 words, hyphen-separated. No more, no less.\n"
                    "   • No type words. Banned: service, server, daemon, library,\n"
                    "     tool, binary, package, module, system, utility,\n"
                    "     application, program, process, worker.\n"
                    "   • Describes WHAT it does, not what it is or how it is built.\n"
                    "   • English only. Full words over abbreviations.\n"
                    "   • Self-documenting. Among several similar tools, the\n"
                    "     primary one's name reflects that — the user should not\n"
                    "     have to guess which to run.\n"
                    "   ──────────────────────────────────────────────────────────\n",
                    line);
                report_fatal_error_and_exit(eb);
            }
            snprintf(allowed[allowed_qty].name, 128, "%s", line); allowed_qty++;
        }
    }
    fclose(f);
}

int return_total_count_allowed_names(void) { return allowed_qty; }
const char *get_allowed_name_from_whitelist(int index) { return index < allowed_qty ? allowed[index].name : NULL; }

char banned_patterns[32][64];
int banned_patterns_count;

void read_banned_patterns_from_file(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/banned-patterns.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) report_fatal_error_and_exit("cannot open banned-patterns.txt");
    char line[64];
    while (fgets(line, sizeof(line), f) && banned_patterns_count < 32) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0) { snprintf(banned_patterns[banned_patterns_count], 64, "%s", line); banned_patterns_count++; }
    }
    fclose(f);
}
