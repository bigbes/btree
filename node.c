#include <assert.h>

#include "dbg.h"
#include "btree.h"
#include "cache.h"
#include "pagepool.h"

/**
 * @brief      Initialize btree node (cached version)
 *
 * @param[in]  db   Current Database instance
 * @param[out] node Node for initialization
 * @param[in]  page Page number for initialization (0 on new node)
 *
 * @return     Status
 */
int node_btree_load(struct DB *db, struct BTreeNode *node, pageno_t page) {
	if (page == 0) page = pool_alloc(db->pool);
	void *data = cache_page_get(db->pool->cache, page);
	node->h = (struct NodeHeader *)data;
	node->h->page = page;
	node->chld = (void *)node->h + sizeof(struct NodeHeader);
	node->vals = (void *)(node->chld + (db->btree_degree + 1));
	node->keys = (void *)(node->vals + db->btree_degree);
	return 0;
}

/**
 * @brief      Initialize data node (cached version)
 *
 * @param[in]  db   Current Database instance
 * @param[out] node Node for initialization
 * @param[in]  page Page number for initialization (0 on new node)
 *
 * @return     Status
 */
int node_data_load(struct DB *db, struct DataNode *node, pageno_t page) {
	if (page == 0) page = pool_alloc(db->pool);
	node->h = (struct NodeHeader *)cache_page_get(db->pool->cache, page);
	node->h->page = page;
	node->h->flags = IS_DATA;
	node->data = (void *)node->h + sizeof(struct NodeHeader);
	return 0;
}

/**
 * @brief      Dump btree node to disk
 *
 * @param db   Current Database instance
 * @param node Node for dumping
 *
 * @return     Status
 */
int node_btree_dump(struct DB *db, struct BTreeNode *node) {
	log_info("Dumping BTreeNode %zd", node->h->page);
	pageno_t page = node->h->page;
	size_t to_write = db->pool->pageSize;
	return pool_write(db->pool, node->h, to_write, page, 0);
}

/**
 * @brief      Dump data node to disk
 *
 * @param db   Current Database instance
 * @param node Node for dumping
 *
 * @return     Status
 */
int node_data_dump(struct DB *db, struct DataNode *node) {
	log_info("Dumping DataNode %zd", node->h->page);
	size_t complete = 0;
	size_t to_write = node->h->size + sizeof(struct NodeHeader);
	if ((void *)node->data != (void *)node->h + sizeof(struct NodeHeader))
		to_write = sizeof(struct NodeHeader);
	complete += pool_write(db->pool, node->h, to_write, node->h->page, 0);
	if ((void *)node->data != (void *)node->h + sizeof(struct NodeHeader)) {
		complete += pool_write(db->pool, node->data, node->h->size,
				       node->h->page, sizeof(struct NodeHeader));
	}
	return complete;
}

/**
 * @brief     Free node at position pos
 *
 * @param db  DB object
 * @param pos Position to be freed
 *
 * @return    Status
 */
int node_deallocate(struct DB *db, pageno_t pos) {
	return pool_dealloc(db->pool, pos);
}

/**
 * @brief      Return node to cache
 *
 * @param db   DB object
 * @param node (void *)(struct DataNode *) or (void *)(struct BTreeNode *)
 *             Nodes, which memory to be freed
 *
 * @return     Status
 */
void node_free(struct DB *db, void *node) {
	cache_page_free(db->pool->cache, ((struct BTreeNode *)(node))->h->page);
}

