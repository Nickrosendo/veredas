/*
 * What every build shares about content packs, whichever pack (or none) is
 * linked: a pack defines only content_get.
 */
#include "content.h"

#include <string.h> /* strcmp */

bool content_has(const char *needs)
{
    return needs[0] == '\0' || strcmp(needs, content_get()->name) == 0;
}
