#define  _GNU_SOURCE

#include <string.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>  /* assert
		      */
#include <stdbool.h> /* bool
		      * true
		      * false
		      */

#define DEBUG
#include "dbg.h"
#include "node.h"
#include "btree.h"
#include "cache.h"
#include "pagepool.h"

#include "insert.h"
#include "search.h"
#include "delete.h"

uint32_t btree_node_max_capacity(struct DB *db) {
	double size  = db->pool->pageSize;
	size -= sizeof(struct NodeHeader) - sizeof(pageno_t);
	size /= 2 * sizeof(pageno_t) + BTREE_KEY_LEN;
	return (uint32_t )floor(size);
}

uint32_t data_node_max_capacity(struct DB *db) {
	return (uint32_t )(db->pool->pageSize - sizeof(struct NodeHeader));
}

/**
 * @brief          Initialize DB object
 *
 * @param[out] db       Object to initialize
 * @param[in]  db_name  File for use in Database
 * @param[in]  pageSize Size of pages to use in PagePool
 * @param[in]  poolSize Size of pool to use in PagePool
 *
 * @return         Status
 */
int db_init(struct DB *db, char *db_name, uint16_t pageSize, pageno_t poolSize, size_t cacheSize) {
	log_info("Creating DB with name %s", db_name);
	log_info("PoolSize: %zd, PageSize %i", poolSize, pageSize);
	memset(db, 0, sizeof(struct DB));
	db->db_name = db_name;
	db->pool = (struct PagePool *)malloc(sizeof(struct PagePool));
	assert(db->pool);
	db->cache = (struct CacheBase *)malloc(sizeof(struct CacheBase));
	assert(db->cache);
	pool_init(db->pool, db_name, pageSize, poolSize);
	cache_init(db->cache, db->pool, cacheSize);
	db->top = (struct BTreeNode *)malloc(sizeof(struct BTreeNode));
	assert(db->top);
	db->btree_degree = btree_node_max_capacity(db);
	node_btree_load(db, db->top, 0);
	db->top->h->flags = IS_TOP | IS_LEAF;
	return 0;
}

/**
 * @brief    Free DB object
 *
 * @param db Object to free
 *
 * @return   Status
 */
int db_free(struct DB *db) {
	if (db) {
		if (db->top) {
			node_free(db, db->top);
			free(db->top);
			db->top = NULL;
		}
	}
	if (db->pool) {
		pool_free(db->pool);
		db->pool = NULL;
	}
	if (db->cache) {
		cache_free(db->cache);
		free(db->cache);
		db->cache = NULL;
	}
	return 0;
}

/**
 * @brief  DB Search wrapper
 *
 * @return Status
 */
int db_search(struct DB *db, char *key, void **val, size_t *val_len) {
	log_info("Searching value in the DB with key '%s'", key);
	pageno_t ret = btreei_search(db, db->top, key, val_len);
	if (ret == 0) return 0;
	struct DataNode node;
	node_data_load(db, &node, ret);
	*val = strndup(node.data, node.h->size);
	*val_len = node.h->size;
	node_free(db, &node);
	return 0;
}

/**
 * @brief  DB Insert Wrapper
 *
 * @return Status
 */
int db_insert(struct DB *db, char *key, char *val, int val_len) {
	log_info("Inserting value into DB with key '%s'", key);
	return btreei_insert(db, db->top, key, val, val_len);
}

int db_delete(struct DB *db, char *key) {
	log_info("Deleting value from DB with key '%s'", key);
	return btreei_delete(db, db->top, key);
}

int node_print(struct BTreeNode *node) {
#ifdef DEBUG
	printf("--------------------------------------\n");
	printf("PageNo: %03zd, ", node->h->page);
	printf("Size: %d, Flags: ", node->h->size);
	if (node->h->flags & IS_LEAF) printf("IS_LEAF");
	printf("\n");
	int i = 0;
	for (i = 0; i < node->h->size; ++i) {
		printf("Key: %s, Value: %zd", NODE_KEY_POS(node, i), node->vals[i]);
		if (!(node->h->flags & IS_LEAF))
			printf(", Child: %zd", node->chld[i]);
		printf("\n");
	}
	if (!(node->h->flags & IS_LEAF))
		printf("Last Child: %zd\n", node->chld[node->h->size]);
	printf("--------------------------------------\n");
#endif
	return 0;
}

int print_tree(struct DB *db, struct BTreeNode *node) {
	node_print(node);
	if (node->h->flags & IS_LEAF)
		return 0;
	int i = 0;
	for (i = 0; i < node->h->size + 1; ++i) {
		assert(node->chld[i] != 0);
		struct BTreeNode n;
		node_btree_load(db, &n, node->chld[i]);
		print_tree(db, &n);
	}
	return 0;
}

int db_print(struct DB *db) {
	printf("=====================================================\n");
	int retcode =  print_tree(db, db->top);
	printf("=====================================================\n");
	return retcode;
}

void db_close(struct DB *db) {
	return;
}

int db_del(struct DB *db, void *key, size_t key_len) {
	return 0;
}

int db_get(struct DB *db, void *key, size_t key_len,
	   void **val, size_t *val_len) {
	return db_search(db, key, val, val_len);
}

int db_put(struct DB *db, void *key, size_t key_len,
	   void *val, size_t val_len) {
	return db_insert(db, key, val, val_len);
}

struct DBC {
	size_t db_size;
	size_t chunk_size;
	size_t cache_size;
};

struct DB *dbcreate(char *file, struct DBC *config) {
	struct DB *db = (struct DB *)malloc(sizeof(struct DB));
	db_init(db, file, config->chunk_size, config->db_size, config->cache_size);
	return db;
}

int main() {
	struct DB db;
	db_init(&db, "mydb", 512, 128*1024*1024, 16*1024*1024);
	cache_print(db.cache);
	db_insert(&db, "1234", "Hello, world1", 13);
	db_insert(&db, "1235", "Hello, world2", 13);
	db_insert(&db, "1236", "Hello, world3", 13);
	db_insert(&db, "1237", "Hello, world4", 13);
	db_insert(&db, "1238", "Hello, world5", 13);
	cache_print(db.cache);
	db_insert(&db, "1232", "Hello, world6", 13);
	db_insert(&db, "1231", "Hello, world7", 13);
	db_insert(&db, "1240", "Hello, world8", 13);
	db_insert(&db, "1232", "Hello, world6", 13);
	db_insert(&db, "1251", "Hello, world7", 13);
	db_insert(&db, "1252", "Hello, world8", 13);
	db_insert(&db, "1253", "Hello, world6", 13);
	db_insert(&db, "1254", "Hello, world7", 13);
	cache_print(db.cache);
	db_insert(&db, "1255", "Hello, world8", 13);
	db_insert(&db, "1253", "Hello, world6", 13);
	db_insert(&db, "1254", "Hello, world7", 13);
	db_insert(&db, "1255", "Hello, world8", 13);
	db_insert(&db, "1256", "Hello, world8", 13);
	db_insert(&db, "1257", "Hello, world7", 13);
	db_insert(&db, "1258", "Hello, world8", 13);
	db_insert(&db, "1259", "Hello, world6", 13);
	cache_print(db.cache);
	db_insert(&db, "1260", "Hello, world8", 13);
	db_insert(&db, "1210", "Hello, world7", 13);
	db_insert(&db, "1209", "Hello, world8", 13);
	db_insert(&db, "1208", "Hello, world6", 13);
	db_insert(&db, "1207", "Hello, world8", 13);
	db_insert(&db, "1206", "Hello, world6", 13);
	db_insert(&db, "1205", "Hello, world8", 13);
	cache_print(db.cache);
	db_insert(&db, "1204", "Hello, world7", 13);
	db_insert(&db, "1203", "Hello, world8", 13);
	db_insert(&db, "1202", "Hello, world6", 13);
	db_insert(&db, "1208", "Hello, world8", 13);
	db_insert(&db, "1209", "Hello, world6", 13);
	db_insert(&db, "121000", "Hello, world8", 13);
	/*db_delete(&db, "121000");*/
	db_print(&db);
	cache_print(db.cache);
	char *data = NULL;
	size_t val = 0;
	db_search(&db, "1231", (void **)&data, &val);
	printf("\n%13s\n", data);
	db_free(&db);
	return 0;
}
