/*
 * The public build's content pack: none. `make CONTENT=...` leaves this file
 * out and links the pack's own content_get instead (see content.h).
 */
#include "content.h"

#include <stddef.h> /* NULL */

const Content *content_get(void)
{
    /* static: one Content that lives as long as the program, so the pointer
     * returned stays good after this function returns. A plain local would
     * be gone by then. */
    static const Content none = {"", NULL, 0};
    return &none;
}
