// Fuzz the chatbot's output highlighter exactly as llamafile/chatbot_repl.cpp wires it:
// HighlightMarkdown wrapped in a ColorBleeder, fed model output token by token, then
// flushed. Markdown fenced code blocks dispatch to every language highlighter via
// Highlight::create(lang). The first input byte picks the chunk size so token streaming
// (state carried across feed() calls) is exercised too.
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include "highlight.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    size_t chunk = (data[0] % 16) + 1;
    std::string_view in((const char *)data + 1, size - 1);
    HighlightMarkdown md;
    ColorBleeder bleeder(&md);
    std::string out;
    for (size_t i = 0; i < in.size(); i += chunk)
        bleeder.feed(&out, in.substr(i, chunk));
    bleeder.flush(&out);
    return 0;
}
