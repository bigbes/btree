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
#include "meta.h"
#include "node.h"
#include "btree.h"
#include "cache.h"
#include "pagepool.h"
#include "wal.h"
#include "dumper.h"

#include "insert.h"
#include "search.h"
#include "delete.h"

uint32_t btree_node_max_capacity(struct DB *db) {
	double size  = db->pool->page_size;
	size -= sizeof(struct NodeHeader) - sizeof(pageno_t);
	size /= 2 * sizeof(pageno_t) + BTREE_KEY_LEN;
	return (uint32_t )floor(size);
}

uint32_t data_node_max_capacity(struct DB *db) {
	return (uint32_t )(db->pool->page_size - sizeof(struct NodeHeader));
}

static int dbi_init(struct DB *db, char *db_name, uint16_t page_size,
	     pageno_t pool_size, size_t cache_size) {
	memset(db, 0, sizeof(struct DB));
	db->db_name = db_name;

	db->pool = (struct PagePool *)malloc(sizeof(struct PagePool));
	check_mem(db->pool, sizeof(struct PagePool));
	
	db->top = (struct BTreeNode *)malloc(sizeof(struct BTreeNode));
	check_mem(db->top, sizeof(struct BTreeNode));

	db->wal = (struct WAL *)malloc(sizeof(struct WAL));
	check_mem(db->wal, sizeof(struct WAL));

	db->lsn = 0;
	
	return 0;
error:
	exit(-1);
}

/**
 * @brief          Initialize DB object
 *
 * @param[out] db       Object to initialize
 * @param[in]  db_name  File for use in Database
 * @param[in]  page_size Size of pages to use in PagePool
 * @param[in]  pool_size Size of pool to use in PagePool
 *
 * @return         Status
 */
int db_init(struct DB *db, char *db_name, uint16_t page_size,
	    pageno_t pool_size, size_t cache_size) {
	log_info("Creating DB with name %s", db_name);
	log_info("PoolSize: %zd, PageSize %i", pool_size, page_size);

	dbi_init(db, db_name, page_size, pool_size, cache_size);
	pool_init_new(db->pool, db_name, page_size, pool_size, cache_size);
	db->btree_degree = btree_node_max_capacity(db);

	
	wal_init(db, db->wal);
	dumper_init(db, db->pool);
	
	node_btree_load(db, db->top, 0);
	db->top->h->flags = IS_TOP | IS_LEAF;

	struct Metadata md = {pool_size, page_size, db->top->h->page};
	meta_dump(db_name, &md);

	return 0;
}

int db_load(struct DB *db, char *db_name, size_t cache_size) {
	log_info("Loading DB with name %s", db_name);

	struct Metadata md = {0,0,0};
	meta_load(db_name, &md);
	log_info("PoolSize: %zd, PageSize %zd", md.pool_size, md.page_size);

	dbi_init(db, db_name, md.page_size, md.pool_size, cache_size);
	pool_init_old(db->pool, db_name, md.page_size, md.pool_size, cache_size);
	db->btree_degree = btree_node_max_capacity(db);
	node_btree_load(db, db->top, md.header_page);

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
		dumper_free(db->pool);
		if (db->pool) {
			pool_free(db->pool);
			db->pool = NULL;
		}
		if (db->wal) {
			wal_free(db->wal);
			db->wal = NULL;
		}
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

/* ######################## DEBUG ######################## */
int node_print(struct BTreeNode *node) {
#ifdef DEBUG
	printf("--------------------------------------\n");
	printf("PageNo: %03zd, ", node->h->page);
	printf("Size: %d, Flags: ", node->h->size);
	if (node->h->flags & IS_LEAF) printf("IS_LEAF");
	printf("\n");
	printf("LSN: %zd\n", node->h->lsn);
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
		node_free(db, &n);
	}
	return 0;
}

int db_print(struct DB *db) {
	printf("=====================================================\n");
	int retcode =  print_tree(db, db->top);
	printf("=====================================================\n");
	return retcode;
}
/* ######################## DEBUG ######################## */

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

struct DB *dbcreate(char *file, struct DBC *config) {
	struct DB *db = (struct DB *)calloc(1, sizeof(struct DB));
	int db_exists = access(file, F_OK);
	int dbmeta_exists = meta_check(file);
	if (db_exists == 0) {
		check(dbmeta_exists == 0, "No metafile exists, but DB exists. Exiting");
		db_load(db, file, config->cache_size);
	} else {
		db_init(db, file, config->page_size, config->pool_size, config->cache_size);
	}
	return db;
error:
	exit(-1);
}

int main() {
	//struct DBC dbc = {128*1024*1024, 2046, 16*1024*1024};
	//struct DB *dbn = dbcreate("mydb", &dbc);
	//struct DB db = *dbn;
	struct DB db;
	db_init(&db, "mydb", 2048, 128*1024*1024, 16*1024*1024);
	cache_print(db.pool->cache);
	db_print(&db);
	db_insert(&db, "1234", "Hello, world1", 13);
	db_insert(&db, "1235", "Hello, world2", 13);
	db_insert(&db, "1236", "Hello, world3", 13);
	db_insert(&db, "1237", "Hello, world4", 13);
	db_insert(&db, "1238", "Hello, world5", 13);
	cache_print(db.pool->cache);
	db_insert(&db, "1232", "Hello, world6", 13);
	db_insert(&db, "1231", "Hello, world7", 13);
	db_insert(&db, "1240", "Hello, world8", 13);
	db_insert(&db, "1232", "Hello, world6", 13);
	db_insert(&db, "1251", "Hello, world7", 13);
	db_insert(&db, "1252", "Hello, world8", 13);
	db_insert(&db, "1253", "Hello, world6", 13);
	db_insert(&db, "1254", "Hello, world7", 13);
	cache_print(db.pool->cache);
	db_insert(&db, "1255", "Hello, world8", 13);
	db_insert(&db, "1253", "Hello, world6", 13);
	db_insert(&db, "1254", "Hello, world7", 13);
	db_insert(&db, "1255", "Hello, world8", 13);
	db_insert(&db, "1256", "Hello, world8", 13);
	db_insert(&db, "1257", "Hello, world7", 13);
	db_insert(&db, "1258", "Hello, world8", 13);
	db_insert(&db, "1259", "Hello, world6", 13);
	cache_print(db.pool->cache);
	db_insert(&db, "1260", "Hello, world8", 13);
	db_insert(&db, "1210", "Hello, world7", 13);
	db_insert(&db, "1209", "Hello, world8", 13);
	db_insert(&db, "1208", "Hello, world6", 13);
	db_insert(&db, "1207", "Hello, world8", 13);
	db_insert(&db, "1206", "Hello, world6", 13);
	db_insert(&db, "1205", "Hello, world8", 13);
	cache_print(db.pool->cache);
	db_insert(&db, "1204", "Hello, world7", 13);
	db_insert(&db, "1203", "Hello, world8", 13);
	db_insert(&db, "1202", "Hello, world6", 13);
	db_insert(&db, "1208", "Hello, world8", 13);
	db_insert(&db, "1209", "Hello, world6", 13);
	db_insert(&db, "121000", "Hello, world8", 13);
	/*db_delete(&db, "121000");*/
	db_print(&db);
	cache_print(db.pool->cache);
	char *data = NULL;
	size_t val = 0;
	db_search(&db, "1231", (void **)&data, &val);
	printf("\n%13s\n", data);
	db_free(&db);
	return 0;
}
