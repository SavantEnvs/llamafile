// Fuzz llamafile's image handling (llamafile/image.cpp) — the /upload and image-prompt path:
// magic sniffing (get_image_type / is_image), stb_image decode + resize + ANSI half-block
// rendering (print_image) and data-URI encoding (convert_image_to_uri).
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <unistd.h>
#include "image.h"

static int g_devnull = -1;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc; (void)argv;
    g_devnull = open("/dev/null", O_WRONLY);
    if (g_devnull == -1) abort();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::string_view img((const char *)data, size);
    lf::ImageType t = lf::get_image_type(img);
    (void)lf::get_image_mime(t);
    if (lf::is_image(img)) {
        lf::print_image(g_devnull, img, 80);
        std::string uri;
        lf::convert_image_to_uri(&uri, img);
    }
    return 0;
}
