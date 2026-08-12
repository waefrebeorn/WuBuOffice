#include "wubudropcap.h"

int wubudropcap_init(wubudropcap *d, int lines) {
    if (!d) return -1;
    d->enabled = (lines >= 2 && lines <= 5) ? 1 : 0;
    d->lines = (lines >= 2 && lines <= 5) ? lines : 0;
    return 0;
}

int wubudropcap_enable(wubudropcap *d, int lines) {
    if (!d || lines < 2 || lines > 5) return -1;
    d->enabled = 1;
    d->lines = lines;
    return 0;
}

int wubudropcap_disable(wubudropcap *d) {
    if (!d) return -1;
    d->enabled = 0;
    return 0;
}

int wubudropcap_lines(const wubudropcap *d) {
    return (d && d->enabled) ? d->lines : 0;
}
