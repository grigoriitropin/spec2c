// SPDX-License-Identifier: Apache-2.0
// canonical SOUL §10 banned type words
// Included by: IPM validator (spec2c), C enforcement (s2c_enforce)

#ifndef SOUL_BANNED_WORDS_H
#define SOUL_BANNED_WORDS_H

static const char *soul_banned_words[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

#endif
