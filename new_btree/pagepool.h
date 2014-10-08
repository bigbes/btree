#include "btree_new.h"

/**
 * @brief Structure for working with on-disk page pool
 */
struct PagePool {
	int                  fd;
	pageno_t             pageSize;
	size_t               poolSize
	pageno_t             nPages;
	void                *bitmask;
	struct bit_iterator *it;
};

/*
pageno_t bitmask_pages   (struct PagePool *);
int      bitmask_it_init (struct PagePool *);
int      bitmask_dump    (struct PagePool *);
int      bitmask_load    (struct PagePool *);
int      bitmask_populate(struct PagePool *);
int      bitmask_check   (struct PagePool *, pageno_t);

pageno_t page_find_empty(struct PagePool *);
*/

pageno_t pool_alloc(struct PagePool *);
int      pool_free (struct PagePool *);
void    *pool_read (struct PagePool *, pageno_t);
int      pool_write(struct PagePool *, void *data, size_t size,
		    pageno_t, size_t offset);
int      pool_init (struct PagePool *, char *, uint16_t pageSize, pageno_t);
int      pool_free (struct PagePool *);

BTreeNode *read_btree_node (struct PagePool *, pageno_t);
DataNode  *read_data_node  (struct PagePool *, pageno_t);
int        write_btree_node(struct PagePool *, struct BTreeNode *);
int        write_data_node (struct PagePool *, struct DataNode  *);
