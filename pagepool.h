#ifndef   PAGEPOOL_H
#define   PAGEPOOL_H

#include "btree.h"

/*
pageno_t bitmask_pages   (struct PagePool *);
int      bitmask_it_init (struct PagePool *);
int      bitmask_dump    (struct PagePool *);
int      bitmask_load    (struct PagePool *);
int      bitmask_populate(struct PagePool *);
int      bitmask_check   (struct PagePool *, pageno_t);

pageno_t page_find_empty(struct PagePool *);
*/

pageno_t pool_alloc  (struct PagePool *);
int      pool_dealloc(struct PagePool *,    pageno_t);
void    *pool_read   (struct PagePool *,    pageno_t, size_t, size_t);
int      pool_read_into (struct PagePool *, pageno_t, void *);
int      pool_write(struct PagePool *, void *, size_t, pageno_t, size_t);
int      pool_init (struct PagePool *, char *, uint16_t pageSize, pageno_t);
int      pool_free (struct PagePool *);

#endif /* PAGEPOOL_H */
