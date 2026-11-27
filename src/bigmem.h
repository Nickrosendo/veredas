/*
 * bigmem: memory for the largest block the world keeps, the creature pool
 * (8,000 slots of about 4.4 KB, some 35 MB), asked for in large pages where
 * the system offers them.
 *
 * The processor finds memory through a table of pages, 4 KB each, and keeps
 * the pages it used last in a small cache of its own (the TLB). A tick reads a
 * few fields of thousands of creatures, each in a page of its own, far more
 * pages than that cache holds, so most reads first had to look their page up.
 * In 2 MB pages the same pool is some 18 pages instead of some 8,600 (the
 * cheaper tick study, 2026-11-25). Only where memory is placed changes: what
 * is in it, and so the world, is the same either way.
 */
#ifndef VEREDAS_BIGMEM_H
#define VEREDAS_BIGMEM_H

#include <stddef.h> /* size_t */

/* bytes of memory, not cleared, or NULL when there is none: malloc's
 * promise, and given back with free like any malloc'd block. On Linux a block
 * of at least 2 MB starts on a 2 MB boundary and the system is asked to back
 * it with large pages; elsewhere this is malloc. */
void *bigmem_alloc(size_t bytes);

#endif
