// Fuzz llamafile_open_gguf() — the weights-container opener every llamafile engine routes
// model loading through (llamafile/llamafile.c + llamafile/zip.c). The input is written to a
// scratch file and opened exactly as a model path would be: a raw GGUF file, or a zip /
// .llamafile whose EOCD / zip64 locator, central directory, extra fields and local header are
// parsed by hand. A second open uses the `archive@member.gguf` form, which selects the entry
// by name. The opened entry is then read, seeked and closed through the public API.
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "llamafile.h"

void lf_shim_gc_release(void);

static char g_path[4096];
static char g_member_path[4200];

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc; (void)argv;
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_path, sizeof(g_path), "%s/llamafile_fuzz_XXXXXX", tmp);
    int fd = mkstemp(g_path);  // name has no '@', so it is opened as a plain path
    if (fd == -1) { perror("mkstemp"); abort(); }
    close(fd);
    snprintf(g_member_path, sizeof(g_member_path), "%s@model.gguf", g_path);
    return 0;
}

static void exercise(struct llamafile *f) {
    if (!f) return;
    size_t size = llamafile_size(f);
    char buf[4096];
    long total = 0, r;
    while ((r = llamafile_read(f, buf, sizeof(buf))) > 0) total += r;
    if (size > 8) {
        llamafile_seek(f, size / 2, SEEK_SET);
        (void)llamafile_tell(f);
        (void)llamafile_read(f, buf, 8);
        llamafile_seek(f, 0, SEEK_SET);
        (void)llamafile_read(f, buf, 4);
    }
    void *content = llamafile_content(f);
    if (content && size) {
        volatile unsigned char sink = ((unsigned char *)content)[0] ^ ((unsigned char *)content)[size - 1];
        (void)sink;
    }
    (void)total;
    llamafile_close(f);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    int fd = open(g_path, O_WRONLY | O_TRUNC);
    if (fd == -1) abort();
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) abort();
        off += (size_t)w;
    }
    close(fd);
    exercise(llamafile_open_gguf(g_path, "rb"));
    exercise(llamafile_open_gguf(g_member_path, "rb"));
    lf_shim_gc_release();
    return 0;
}
