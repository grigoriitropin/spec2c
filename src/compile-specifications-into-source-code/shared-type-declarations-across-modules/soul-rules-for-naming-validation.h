// SPDX-License-Identifier: Apache-2.0
// soul-validation.h — shared validation library for SOUL §7 + §10
// Used by both IPM validator (spec2c) and C enforcer (s2c_enforce)

#ifndef SOUL_VALIDATION_H
#define SOUL_VALIDATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── canonical banned type words (single source of truth) ──────────── */
static const char *soul_banned_type_words[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

/* ── centralized error reporter ───────────────────────────────────── */
static void report_soul_violation_and_exit(const char *code, const char *context, const char *fix) {
    fprintf(stderr, "SOUL VIOLATION [%s]\n", code);
    if (context) fprintf(stderr, "  error: %s\n", context);
    if (fix)     fprintf(stderr, "  fix:   %s\n", fix);
    exit(1);
}

/* ── SOUL §10 name validator ──────────────────────────────────────── */
static void verify_name_complies_with_soul(const char *name) {
    if (!name || !name[0]) return;
    if (!strcmp(name, "main")) return;

    char sep_str[2] = {strchr(name, '-') ? '-' : '_', 0};
    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0;
    char *tok = strtok(buf, sep_str);

    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char ctx[256], fix[256];
            snprintf(ctx, sizeof(ctx), "word '%s' in '%s' is too short (min 3 chars)", tok, name);
            snprintf(fix, sizeof(fix), "rename using full English words, no abbreviations");
            report_soul_violation_and_exit("NAME_WORD_TOO_SHORT", ctx, fix);
        }
        for (int i = 0; soul_banned_type_words[i]; i++) {
            if (!strcmp(tok, soul_banned_type_words[i])) {
                char ctx[256], fix[256];
                snprintf(ctx, sizeof(ctx), "word '%s' in '%s' is a banned type word", tok, name);
                snprintf(fix, sizeof(fix), "replace with a word describing WHAT it does, not WHAT it is");
                report_soul_violation_and_exit("NAME_BANNED_TYPE_WORD", ctx, fix);
            }
        }
        tok = strtok(NULL, sep_str);
    }

    if (words != 5) {
        char ctx[256], fix[256];
        snprintf(ctx, sizeof(ctx), "'%s' has %d words (need exactly 5)", name, words);
        snprintf(fix, sizeof(fix), "rename using exactly 5 hyphen-separated words");
        report_soul_violation_and_exit("NAME_WRONG_WORD_COUNT", ctx, fix);
    }
}

#endif
