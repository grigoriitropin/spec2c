// SPDX-License-Identifier: Apache-2.0
// FFI name validator — callable from IPM via extern_imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *banned_type_words[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

char *check_name_following_soul_rules(char *what, char *name, char *fp) {
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
            char *err = malloc(256);
            snprintf(err, 256, "word '%s' too short (min 3)", tok);
            return err;
        }
        for (int i = 0; banned_type_words[i]; i++)
            if (!strcmp(tok, banned_type_words[i])) {
                char *err = malloc(256);
                snprintf(err, 256, "banned word '%s'", tok);
                return err;
            }
        tok = strtok(NULL, sep_str);
    }
    if (words != 5) {
        char *err = malloc(256);
        snprintf(err, 256, "has %d words (need 5)", words);
        return err;
    }
    return NULL;
}
