// Fuzz llamafile's RFC 2397 data: URI parser (llamafile/datauri.cpp), which the chatbot runs
// on prompt text to pull out inline images. Like extract_data_uris.cpp, it scans the text for
// each "data:" occurrence, parses from there, decodes the payload (base64 or percent-encoded)
// and queries the parameters. base64_error is caught exactly as extract_data_uris.cpp does.
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include "base64.hpp"
#include "datauri.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::string_view s((const char *)data, size);
    size_t pos = 0;
    while ((pos = s.find("data:", pos)) != std::string_view::npos) {
        DataUri uri;
        size_t n = uri.parse(s.substr(pos + 5));
        if (n != std::string_view::npos) {
            // extract_data_uris.cpp, the product caller, catches base64_error from decode().
            try {
                std::string decoded = uri.decode();
            } catch (const base64_error &) {
            }
            (void)uri.has_param("charset");
            (void)uri.get_param("charset");
            (void)uri.has_param("base64");
        }
        pos += 5;
    }
    return 0;
}
