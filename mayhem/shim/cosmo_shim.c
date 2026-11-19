// Runtime half of mayhem/shim/cosmo.h, plus the GPU-probe symbols llamafile.c references
// (a native fuzz build has no GPU backends).
#include <stdbool.h>
#include <stdlib.h>
static void **g_gc;
static size_t g_gc_n, g_gc_cap;
void *gc(void *p) {
    if (!p) return p;
    if (g_gc_n == g_gc_cap) {
        size_t cap = g_gc_cap ? g_gc_cap * 2 : 16;
        void **n = (void **)realloc(g_gc, cap * sizeof(void *));
        if (!n) abort();
        g_gc = n; g_gc_cap = cap;
    }
    g_gc[g_gc_n++] = p;
    return p;
}
void lf_shim_gc_release(void) {
    while (g_gc_n) free(g_gc[--g_gc_n]);
}
bool llamafile_has_metal(void) { return false; }
bool llamafile_has_cuda(void) { return false; }
bool llamafile_has_amd_gpu(void) { return false; }
bool llamafile_has_vulkan(void) { return false; }

// Cosmopolitan's <ctype.h> classifiers are plain range checks, defined for every int. The
// highlighters rely on that: they pass decoded code points (wchar_t, often > 255) straight
// to isblank()/isalpha()/... glibc's versions index a 384-entry table and read out of bounds
// for such values, which would make every non-ASCII input "crash" in this native build but
// not in the shipped (cosmo) binary. Defining cosmo's semantics here, in the executable,
// takes precedence over libc.so for the whole program.
#undef isascii
#undef isalpha
#undef isdigit
#undef isalnum
#undef isxdigit
#undef isspace
#undef isblank
#undef isupper
#undef islower
#undef iscntrl
#undef isprint
#undef isgraph
#undef ispunct
#undef tolower
#undef toupper
int isascii(int c) { return 0 <= c && c <= 0x7f; }
int isupper(int c) { return 'A' <= c && c <= 'Z'; }
int islower(int c) { return 'a' <= c && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isdigit(int c) { return '0' <= c && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f'); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (0 <= c && c <= 0x1f) || c == 0x7f; }
int isprint(int c) { return 0x20 <= c && c <= 0x7e; }
int isgraph(int c) { return 0x21 <= c && c <= 0x7e; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }
int toupper(int c) { return islower(c) ? c - ('a' - 'A') : c; }
