// mayhem/shim/cosmo.h — minimal stand-in for Cosmopolitan's <cosmo.h> so llamafile's own
// sources (llamafile/*.c, llamafile/highlight/*.cpp, datauri/image) build natively with
// clang + libFuzzer. Only the handful of cosmo extensions those files use are provided.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
// cosmo's gc() frees its argument when the calling function returns. Here it records the
// pointer; the harness calls lf_shim_gc_release() after each input so memory does not grow.
void *gc(void *p);
void lf_shim_gc_release(void);
static inline int memcasecmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; ++i) {
        int c = (x[i] | (x[i] >= 'A' && x[i] <= 'Z' ? 32 : 0)) - (y[i] | (y[i] >= 'A' && y[i] <= 'Z' ? 32 : 0));
        if (c) return c;
    }
    return 0;
}
static inline bool startswith(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
static inline char *GetProgramExecutableName(void) { return (char *)"/proc/self/exe"; }
#ifndef _SC_GRANSIZE
#define _SC_GRANSIZE _SC_PAGESIZE
#endif
typedef int errno_t;
static inline int IsWindows(void) { return 0; }
static inline int IsXnu(void) { return 0; }
static inline int makedirs(const char *p, unsigned m) { (void)p; (void)m; return -1; }
static inline int timespec_cmp(struct timespec a, struct timespec b) {
    return a.tv_sec != b.tv_sec ? (a.tv_sec > b.tv_sec) - (a.tv_sec < b.tv_sec)
                                : (a.tv_nsec > b.tv_nsec) - (a.tv_nsec < b.tv_nsec);
}
#ifdef __cplusplus
}
#endif
#define __notice(sym, str) static const char sym[] __attribute__((unused)) = str
// cosmopolitan libc/str/thompike.h (ISC) — UTF-8 decoding helpers
#include "libc/intrin/bsr.h"
#ifndef ThomPikeCont
#define ThomPikeCont(x)     (0200 == (0300 & (x)))
#define ThomPikeByte(x)     ((x) & (((1 << ThomPikeMsb(x)) - 1) | 3))
#define ThomPikeLen(x)      (7 - ThomPikeMsb(x))
#define ThomPikeMsb(x)      ((255 & (x)) < 252 ? bsr(255 & ~(x)) : 1)
#define ThomPikeMerge(x, y) ((x) << 6 | (077 & (y)))
#endif
#ifndef MIN
#define MIN(X, Y) ((Y) > (X) ? (X) : (Y))
#endif
#ifndef MAX
#define MAX(X, Y) ((Y) < (X) ? (X) : (Y))
#endif
// cosmo/BSD tcgetwinsize(); glibc only has the TIOCGWINSZ ioctl.
#include <sys/ioctl.h>
#include <termios.h>
static inline int tcgetwinsize(int fd, struct winsize *ws) { return ioctl(fd, TIOCGWINSZ, ws); }
#include <stdarg.h>
#include <stdlib.h>
#include "libc/serialize.h"
// cosmo tpenc() (only for sources that use cosmo's; highlight_markdown.cpp defines its own,
// so build.sh enables this with -DLF_SHIM_TPENC per file): Thompson-Pike (UTF-8, up to 6 bytes) encode of c, first byte in the low byte.
#ifdef LF_SHIM_TPENC
static inline uint64_t tpenc(uint32_t c) {
    if (c < 0x80) return c;
    int n = c < 0x800 ? 2 : c < 0x10000 ? 3 : c < 0x200000 ? 4 : c < 0x4000000 ? 5 : 6;
    unsigned char b[6];
    for (int i = n - 1; i > 0; --i) { b[i] = 0x80 | (c & 0x3f); c >>= 6; }
    b[0] = (unsigned char)((0xff00 >> n) | c);
    uint64_t w = 0;
    for (int i = n - 1; i >= 0; --i) w = (w << 8) | b[i];
    return w;
}
#endif
