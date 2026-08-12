/* wubudropcap.h — drop cap: first-letter styling for a paragraph. */
#ifndef WUBUDROPCAP_H
#define WUBUDROPCAP_H

typedef struct {
    int lines;        /* how many text lines the drop cap spans (2-5) */
    int enabled;
} wubudropcap;

int wubudropcap_init(wubudropcap *d, int lines);
int wubudropcap_enable(wubudropcap *d, int lines);
int wubudropcap_disable(wubudropcap *d);
int wubudropcap_lines(const wubudropcap *d);

#endif
