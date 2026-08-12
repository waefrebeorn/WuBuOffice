#include "wubugrammar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PAIRS 64
static char pair_wrong[MAX_PAIRS][40];
static char pair_right[MAX_PAIRS][40];
static int n_pairs = 0;

/* built-in usage/misspelling pairs */
static const char *init_wrong[] = {
    "alot","definately","seperate","recieve","occured","untill","wich","teh","recieve",
    "adress","wich","calender","freind","suprise","goverment","begining","neccessary"
};
static const char *init_right[] = {
    "a lot","definitely","separate","receive","occurred","until","which","the","receive",
    "address","which","calendar","friend","surprise","government","beginning","necessary"
};

static int builtin_loaded = 0;
static void load_builtin(void) {
    if (builtin_loaded) return;
    builtin_loaded = 1;
    int n = (int)(sizeof init_wrong / sizeof init_wrong[0]);
    for (int i = 0; i < n && n_pairs < MAX_PAIRS; i++) {
        strncpy(pair_wrong[n_pairs], init_wrong[i], 39);
        strncpy(pair_right[n_pairs], init_right[i], 39);
        pair_wrong[n_pairs][39]=0; pair_right[n_pairs][39]=0;
        n_pairs++;
    }
}

int wubugrammar_add_pair(const char *wrong, const char *right) {
    load_builtin();
    if (n_pairs >= MAX_PAIRS || !wrong || !right) return -1;
    strncpy(pair_wrong[n_pairs], wrong, 39); pair_wrong[n_pairs][39]=0;
    strncpy(pair_right[n_pairs], right, 39); pair_right[n_pairs][39]=0;
    n_pairs++;
    return 0;
}

static int is_vowel(unsigned char c) { c=(unsigned char)tolower(c); return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }

static void emit(wubugrammar_finding *out, int cap, int *cnt,
                 int start, int len, int id, const char *msg) {
    if (*cnt < cap) {
        out[*cnt].start = start; out[*cnt].len = len; out[*cnt].issue_id = id;
        snprintf(out[*cnt].message, sizeof out[*cnt].message, "%s", msg);
    }
    (*cnt)++;
}

int wubugrammar_check(const char *text, wubugrammar_finding *out, int cap) {
    if (!text) return 0;
    load_builtin();
    int n = (int)strlen(text);
    int cnt = 0;

    /* tokenize lowercase words with positions */
    for (int i = 0; i < n; i++) {
        if (!isalpha((unsigned char)text[i])) continue;
        int wstart = i;
        while (i < n && isalpha((unsigned char)text[i])) i++;
        int wlen = i - wstart;
        /* doubled word: same word repeated, only letters+apostrophe */
        int j = i;
        while (j < n && (isspace((unsigned char)text[j]) || text[j]=='\n' || text[j]=='\t')) j++;
        int s2 = j;
        while (j < n && (isalpha((unsigned char)text[j]) || text[j]=='\'')) j++;
        if (wlen == j - s2 && j - s2 > 0) {
            int eq = 1;
            for (int k = 0; k < wlen; k++)
                if (tolower((unsigned char)text[wstart+k]) != tolower((unsigned char)text[s2+k])) { eq=0; break; }
            if (eq) emit(out, cap, &cnt, wstart, wlen, 1, "Repeated word");
        }

        /* a/an before next word starting with vowel/consonant */
        /* find the word: copy to compare */
        char word[64]; int wc = wlen < 63 ? wlen : 63;
        for (int k = 0; k < wc; k++) word[k] = (char)tolower((unsigned char)text[wstart+k]);
        word[wc] = 0;
        if (strcmp(word,"a")==0 || strcmp(word,"an")==0) {
            int m = i;
            while (m < n && !isalpha((unsigned char)text[m])) m++;
            if (m < n) {
                char nc = (char)tolower((unsigned char)text[m]);
                if (strcmp(word,"a")==0 && is_vowel((unsigned char)nc))
                    emit(out,cap,&cnt,wstart,wlen,2,"'a' before vowel: use 'an'");
                else if (strcmp(word,"an")==0 && !is_vowel((unsigned char)nc))
                    emit(out,cap,&cnt,wstart,wlen,3,"'an' before consonant: use 'a'");
            }
        }
        /* common misspellings (word-boundary match) */
        for (int p = 0; p < n_pairs; p++) {
            if (strcmp(word, pair_wrong[p]) == 0) {
                char msg[160];
                snprintf(msg, sizeof msg, "'%s' -> '%s'", pair_wrong[p], pair_right[p]);
                emit(out, cap, &cnt, wstart, wlen, 4, msg);
                break;
            }
        }
        i = wstart + wlen - 1;
    }

    /* double space / double punctuation */
    for (int i = 0; i < n - 1; i++) {
        if ((text[i]==' ' && text[i+1]==' ') || (text[i]=='.' && text[i+1]=='.'))
            emit(out, cap, &cnt, i, 2, 5, "Double space or period");
    }
    return cnt;
}
