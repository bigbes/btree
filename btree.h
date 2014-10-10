#ifndef   BTREE_H
#define   BTREE_H

#include <stddef.h>
#include <stdint.h>

#define BTREE_KEY_LEN 128

typedef size_t pageno_t;

enum NodeFlags {
	IS_LEAF = 0x01,
	IS_TOP  = 0x02,
	IS_DATA = 0x04,
	IS_CONT = 0x08,
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
	uint16_t             pageSize;
	size_t               poolSize;
	pageno_t             nPages;
	void                *bitmask;
	struct bit_iterator *it;
};

struct NodeHeader {
	pageno_t page;
	uint8_t  flags;
	uint32_t size;
	pageno_t nextPage;
};

struct BTreeNode {
	struct NodeHeader *h;
	pageno_t *chld;
	pageno_t *vals;
	char     *keys;
};

struct DataNode {
	struct NodeHeader *h;
	char    *data;
};

struct DB {
	char             *db_name;
	struct PagePool  *pool;
	struct BTreeNode *top;
	pageno_t          btree_degree;
};

/*
void *node_header_init(struct DB *, struct BTreeNode *, void *);
int node_btree_load   (struct DB *, struct BTreeNode *, pageno_t);
int node_btree_dump   (struct DB *, struct BTreeNode *);
int node_data_load    (struct DB *, struct DataNode *, pageno_t);
int node_data_dump    (struct DB *, struct DataNode *);
int node_free         (struct DB *db, void *node)
int node_deallocate         (struct DB *db, pageno_t pos)
int node_pointer_deallocate (struct DB *db, void *node)
*/

#endif /* BTREE_H */
