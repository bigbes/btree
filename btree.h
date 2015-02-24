#ifndef   BTREE_H
#define   BTREE_H

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#define BTREE_KEY_LEN 128

#define NODE_KEY_POS(NODE, POS)  (NODE->keys + (POS) * BTREE_KEY_LEN)
#define NODE_CHLD_POS(NODE, POS) (NODE->chld + (POS))
#define NODE_VAL_POS(NODE, POS)  (NODE->vals + (POS))

#define NODE_FULL(DB, NODE) (NODE->h->size == DB->btree_degree)
#define NODE_HALF(DB)       floor(DB->btree_degree/2)

typedef ssize_t pageno_t;

enum NodeFlags {
	IS_LEAF = 0x01,
	IS_TOP  = 0x02,
	IS_DATA = 0x04,
/*	____RES = 0x08,*/
/*	____RES = 0x10,*/
/*	____RES = 0x20,*/
/*	____RES = 0x40,*/
/*	____RES = 0x80,*/
};

/**
 * @brief Structure for working with on-disk page pool
 */
struct PagePool {
	int                  fd;
	uint16_t             page_size;
	size_t               pool_size;
	pageno_t             nPages;
	void                *bitmask;
	struct bit_iterator *it;
	struct CacheBase    *cache;
	pthread_t            dumper;
};

struct NodeHeader {
	pageno_t page;
	uint8_t  flags;
	uint32_t size;
	size_t   lsn;
};

struct BTreeNode {
	struct NodeHeader *h;
	pageno_t *chld;
	pageno_t *vals;
	char     *keys;
};

struct DataNode {
	struct NodeHeader *h;
	char  *data;
};

struct DB {
	char             *db_name;
	struct PagePool  *pool;
	struct BTreeNode *top;
	struct WAL       *wal;
	size_t            lsn;
	pageno_t          btree_degree;
};

struct DBC {
	size_t pool_size;
	size_t page_size;
	size_t cache_size;
};

/*
void *node_header_init(struct DB *, struct BTreeNode *, void *);
int node_btree_load   (struct DB *, struct BTreeNode *, pageno_t);
int node_btree_dump   (struct DB *, struct BTreeNode *);
int node_data_load    (struct DB *, struct DataNode *, pageno_t);
int node_data_dump    (struct DB *, struct DataNode *);
int node_free         (struct DB *db, void *node);
int node_deallocate   (struct DB *db, pageno_t pos);
*/

#endif /* BTREE_H */
