#include <assert.h>

#include "dbg.h"
#include "btree.h"
#include "cache.h"
#include "pagepool.h"
#include "wal.h"

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
	int page_new = (page == 0 ? 1 : 0);
	if (page_new) page = pool_alloc(db->pool);
	node->h = (struct NodeHeader *)cache_page_get(db->pool->cache, page);
	node->chld = (void *)node->h + sizeof(struct NodeHeader);
	node->vals = (void *)(node->chld + (db->btree_degree + 1));
	node->keys = (void *)(node->vals + db->btree_degree);
	if (page_new) memset(node->h, 0, db->pool->page_size);
	node->h->page = page;
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
	int page_new = (page == 0 ? 1 : 0);
	if (page_new) page = pool_alloc(db->pool);
	node->h = (struct NodeHeader *)cache_page_get(db->pool->cache, page);
	node->data = (void *)node->h + sizeof(struct NodeHeader);
	if (page_new) memset(node->h, 0, db->pool->page_size);
	node->h->page = page;
	node->h->flags = IS_DATA;
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
	wal_write_append(db, node->h->page);
	node->h->lsn = (db->lsn)++;

	node->h->flags |= CACHE_DIRTY;
	return 0;
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
	wal_write_append(db, node->h->page);
	node->h->lsn = (db->lsn)++;
	struct CacheElem *elem = cachei_page_get(db->pool->cache, node->h->page);
	if (elem)
		elem->flag |= CACHE_DIRTY;
	else
		log_err("can't find");
	return 0;
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

