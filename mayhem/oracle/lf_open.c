// Oracle tool: open a model path through llamafile_open_gguf() (plain GGUF, zip, or
// archive@member) and print the entry's name size and bytes, so test.sh can check them.
#include <stdio.h>
#include "llamafile.h"
void lf_shim_gc_release(void);
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s PATH\n", argv[0]); return 2; }
    struct llamafile *f = llamafile_open_gguf(argv[1], "rb");
    if (!f) { printf("open failed\n"); return 1; }
    printf("size=%zu\n", llamafile_size(f));
    char buf[4096];
    long r;
    while ((r = llamafile_read(f, buf, sizeof(buf))) > 0) fwrite(buf, 1, (size_t)r, stdout);
    printf("\n");
    llamafile_close(f);
    lf_shim_gc_release();
    return 0;
}
