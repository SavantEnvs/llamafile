// Oracle tool: parse argv[1] (text after "data:") with DataUri and print mime, charset, data.
#include <cstdio>
#include <string>
#include <string_view>
#include "datauri.h"
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s URI\n", argv[0]); return 2; }
    DataUri u;
    size_t n = u.parse(argv[1]);
    if (n == std::string_view::npos) { printf("invalid\n"); return 1; }
    std::string d = u.decode();
    std::string mime(u.mime), cs(u.get_param("charset"));
    printf("len=%zu mime=%s charset=%s data=%s\n", n, mime.c_str(), cs.c_str(), d.c_str());
    return 0;
}
