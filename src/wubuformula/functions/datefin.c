/* WuBuOffice -- wubuformula/functions/datefin
 * Date/time and financial functions. Dates are Excel serial numbers (days
 * since 1899-12-30) with the legacy 1900 leap-year bug, handled by the shared
 * excel_serial/serial_to_ymd helpers defined here (used only by this module).
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Excel's "1900 date system" erroneously treats 1900 as a leap year (the
 * Lotus 1-2-3 compatibility bug): serial 60 = the phantom 1900-02-29, and
 * every date from 1900-03-01 onward is shifted +1 vs. the true Gregorian
 * calendar. We compute the pure-Gregorian serial then apply that +1 shift. */
static long excel_serial(int y, int m, int d) {
    if (m < 1 || m > 12 || d < 1) return 0;
    if (y == 1900 && m == 2 && d == 29) return 60; /* the phantom leap day */
    if (y == 1900 && m == 2 && d > 29) d = 29;      /* clamp impossible day */
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    long jdn = d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
    long greg = jdn - 2415020L; /* 1900-01-01 -> serial 1 */
    if (greg >= 60) greg += 1;  /* Excel's 1900 leap bug shift */
    return greg;
}
static void serial_to_ymd(long s, int *y, int *m, int *d) {
    long g = (s >= 61) ? s - 1 : s; /* undo the leap-bug shift */
    long jdn = g + 2415020L;
    long a = jdn + 32044;
    long b = (4 * a + 3) / 146097;
    long c = a - (146097 * b) / 4;
    long dd = (4 * c + 3) / 1461;
    long e = c - (1461 * dd) / 4;
    long mm = (5 * e + 2) / 153;
    *d = (int)(e - (153 * mm + 2) / 5 + 1);
    *m = (int)(mm + 3 - 12 * (mm / 10));
    *y = (int)(100 * b + dd - 4800 + (mm / 10));
}

/* days in a given month/year (Feb = 28 or 29 by real leap rules; the 1900
 * bug is irrelevant here since we never synthesize a 1900-02-29 target). */
static int last_day_of_month(int y, int m) {
    static const int days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return days[m - 1];
}

/* months between two serials, used by EDATE; returns the serial of the date
 * `months` months after the given serial. */
static long edate_serial(long s, int months) {
    int y, m, d; serial_to_ymd(s, &y, &m, &d);
    int total = m - 1 + months;
    int ny = y + total / 12;
    int nm = total % 12;
    if (nm < 0) { nm += 12; ny -= 1; }
    nm += 1;
    /* clamp day to the target month's true length (Feb accounts for leap year,
     * except the 1900 bug which never has 29 days here) */
    int maxd = last_day_of_month(ny, nm);
    if (d > maxd) d = maxd;
    return excel_serial(ny, nm, d);
}

/* days in the month of serial s, for EOMONTH */
static long eomonth_serial(long s, int months) {
    int y, m, d; serial_to_ymd(s, &y, &m, &d);
    int total = m - 1 + months;
    int ny = y + total / 12;
    int nm = total % 12;
    if (nm < 0) { nm += 12; ny -= 1; }
    nm += 1;
    int last = last_day_of_month(ny, nm);
    return excel_serial(ny, nm, last);
}

static void f_date(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1,ok2,ok3; int y=(int)wubu_to_num(&a[0],&ok1), m=(int)wubu_to_num(&a[1],&ok2), d=(int)wubu_to_num(&a[2],&ok3);
    if (!ok1||!ok2||!ok3) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, (double)excel_serial(y, m, d));
}
static void f_year(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)y);
}
static void f_month(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)m);
}
static void f_day(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)d);
}
/* WEEKDAY(serial [, return_type]) — 1=Sun..7=Sat (default, Excel type 1) */
static void f_weekday(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int type = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    /* Zeller's congruence for the day of week (0=Sat..6=Fri in this variant) */
    int y,m,d; serial_to_ymd(s,&y,&m,&d);
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100, J = y / 100;
    int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7; /* 0=Sat,1=Sun,...6=Fri */
    int dow; /* 0=Sun..6=Sat */
    if (h == 0) dow = 6; else dow = h - 1;
    int r;
    switch (type) {
        case 1: r = dow + 1; break;            /* 1=Sun..7=Sat */
        case 2: r = (dow == 6) ? 7 : dow + 1; break; /* 1=Mon..7=Sun */
        case 3: r = (dow == 6) ? 6 : dow; break;     /* 0=Mon..6=Sun */
        default: r = dow + 1; break;
    }
    wubuval_set_num(out, (double)r);
}
static void f_edate(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int months=(na>=2)?(int)wubu_to_num(&a[1],&(int){0}):0;
    wubuval_set_num(out, (double)edate_serial(s, months));
}
static void f_eomonth(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)wubu_to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int months=(na>=2)?(int)wubu_to_num(&a[1],&(int){0}):0;
    wubuval_set_num(out, (double)eomonth_serial(s, months));
}
static void f_today(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;(void)na;(void)flat;(void)fn;(void)ranges;
    time_t t = time(NULL); struct tm *lt = localtime(&t);
    wubuval_set_num(out, (double)excel_serial(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday));
}
static void f_now(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;(void)na;(void)flat;(void)fn;(void)ranges;
    time_t t = time(NULL); struct tm *lt = localtime(&t);
    double serial = (double)excel_serial(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    double frac = (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec) / 86400.0;
    wubuval_set_num(out, serial + frac);
}

static void f_pmt(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int o1,o2,o3; double rate=wubu_to_num(&a[0],&o1), nper=wubu_to_num(&a[1],&o2), pv=wubu_to_num(&a[2],&o3);
    if (!o1||!o2||!o3) { wubuval_set_err(out,WERR_VALUE); return; }
    double fv = (na>=4)?wubu_to_num(&a[3],&(int){0}):0.0;
    double type = (na>=5)?wubu_to_num(&a[4],&(int){0}):0.0;
    double pmt;
    if (rate == 0) pmt = -(pv + fv) / nper;
    else {
        double f = pow(1+rate, nper);
        pmt = -(fv * f + pv) / ((1.0/rate - type) * (1.0 - 1.0/f));
    }
    wubuval_set_num(out, pmt);
}
static void f_fv(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int o1,o2,o3; double rate=wubu_to_num(&a[0],&o1), nper=wubu_to_num(&a[1],&o2), pmt=wubu_to_num(&a[2],&o3);
    if (!o1||!o2||!o3) { wubuval_set_err(out,WERR_VALUE); return; }
    double pv = (na>=4)?wubu_to_num(&a[3],&(int){0}):0.0;
    double type = (na>=5)?wubu_to_num(&a[4],&(int){0}):0.0;
    double f = (rate == 0) ? 1.0 : pow(1+rate, nper);
    double fv = -(pv * f + pmt * (1 + rate * type) * (f - 1) / (rate == 0 ? 1 : rate));
    wubuval_set_num(out, fv);
}

void wubu_register_datefin(wubu_func_registrar reg) {
    reg("DATE", f_date);
    reg("TODAY", f_today);
    reg("NOW", f_now);
    reg("YEAR", f_year);
    reg("MONTH", f_month);
    reg("DAY", f_day);
    reg("WEEKDAY", f_weekday);
    reg("EDATE", f_edate);
    reg("EOMONTH", f_eomonth);
    reg("PMT", f_pmt);
    reg("FV", f_fv);
}
