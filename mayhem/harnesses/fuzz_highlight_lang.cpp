// Fuzz each language highlighter directly through its public factory, Highlight::create(),
// the way llamafile/highlight/highlight_test.cpp drives them. The first byte selects the
// language name, the second splits the rest into two feed() calls; then flush().
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include "highlight.h"

static const char *const kLanguages[] = {
    "ada", "asm", "basic", "bnf", "c", "c#", "c++", "cmake", "cobol", "css", "d", "forth",
    "fortran", "go", "haskell", "html", "java", "javascript", "json", "julia", "kotlin", "ld",
    "lisp", "lua", "m4", "make", "markdown", "matlab", "ocaml", "pascal", "perl", "php!",
    "python", "r", "ruby", "rust", "scala", "shell", "sql", "swift", "tcl", "tex", "txt",
    "typescript", "zig",
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;
    const char *lang = kLanguages[data[0] % (sizeof(kLanguages) / sizeof(*kLanguages))];
    Highlight *h = Highlight::create(lang);
    if (!h) return 0;
    std::string_view in((const char *)data + 2, size - 2);
    size_t split = in.empty() ? 0 : data[1] % (in.size() + 1);
    std::string out;
    h->feed(&out, in.substr(0, split));
    h->feed(&out, in.substr(split));
    h->flush(&out);
    delete h;
    return 0;
}
