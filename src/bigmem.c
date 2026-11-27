/* madvise and MADV_HUGEPAGE are not POSIX: _DEFAULT_SOURCE, before any
 * header, asks glibc for its own extras as well. Only this file needs it. */
#define _DEFAULT_SOURCE

#include "bigmem.h"

#include <stdlib.h> /* malloc, posix_memalign */

/* The browser build (Emscripten) has neither the pages nor madvise. */
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sys/mman.h> /* madvise */
#endif

void *bigmem_alloc(size_t bytes)
{
#if defined(__linux__) && !defined(__EMSCRIPTEN__) && defined(MADV_HUGEPAGE)
    const size_t large = (size_t)2 << 20; /* 2 MB, a large page on x86-64 */
    if (bytes >= large) {
        /* Whole large pages: rounded up, so the last one is not shared with
         * another block. posix_memalign's memory is freed with free. */
        size_t whole = (bytes + large - 1) / large * large;
        void *block = NULL;
        if (posix_memalign(&block, large, whole) != 0) {
            return NULL;
        }
        /* Only advice: if the system has no large page to give, the block
         * is ordinary memory and everything still works. The answer is not
         * needed, so it is cast away. */
        (void)madvise(block, whole, MADV_HUGEPAGE);
        return block;
    }
#endif
    return malloc(bytes);
}
