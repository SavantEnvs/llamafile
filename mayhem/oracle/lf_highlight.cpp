// Oracle tool: highlight stdin with Highlight::create(argv[1]) and write the colorized text.
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>
#include "highlight.h"
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s LANG\n", argv[0]); return 2; }
    Highlight *h = Highlight::create(argv[1]);
    if (!h) { fprintf(stderr, "no highlighter for %s\n", argv[1]); return 1; }
    std::string in((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    std::string out;
    h->feed(&out, in);
    h->flush(&out);
    fwrite(out.data(), 1, out.size(), stdout);
    delete h;
    return 0;
}
