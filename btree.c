#define  _GNU_SOURCE

#include <string.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h> /* bool
		      * true
		      * false
		      */
#define DEBUG
#include "dbg.h"
#include "btree.h"
#include "pagepool.h"


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
 * @brief      Initialize btree node (with loaded, or malloced data)
 *
 * @param[in]  db   Current Database instance
 * @param[out] node Node for initialization
 * @param[in]  page Page number for initialization (0 on new node)
 *
 * @return     Status
 */
int node_btree_load(struct DB *db, struct BTreeNode *node, pageno_t page) {
	void *data = (page > 0 ? pool_read(db->pool, page, 0, 0) : NULL);
	if (data) {
		node->h = (struct NodeHeader *)data;
	} else {
		node->h = (struct NodeHeader *)malloc(db->pool->pageSize);
		node->h->page     = pool_alloc(db->pool);
		node->h->size     = 0;
		node->h->nextPage = 0;
		node->h->flags    = 0;
	}
	node->chld = (void *)node->h + sizeof(struct NodeHeader);
	node->vals = (void *)(node->chld + db->btree_degree + 1);
	node->keys = (void *)(node->vals + db->btree_degree);
	return 0;
}

/**
 * @brief      Initialize data node (with loaded, or malloced data)
 *
 * @param[in]  db   Current Database instance
 * @param[out] node Node for initialization
 * @param[in]  page Page number for initialization (0 on new node)
 *
 * @return     Status
 */
int node_data_load(struct DB *db, struct DataNode *node, pageno_t page) {
	if (page > 0) {
		node->h = pool_read(db->pool, page, 0,
				    sizeof(struct NodeHeader));
		node->data = pool_read(db->pool, page,
				       sizeof(struct NodeHeader),
				       node->h->size);
	} else {
		node->h = (struct NodeHeader *)malloc(sizeof(struct NodeHeader));
		node->h->page     = pool_alloc(db->pool);
		node->h->size     = 0;
		node->h->nextPage = 0;
		node->h->flags = IS_DATA;
	}
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
 * @brief      Free node
 *
 * @param db   DB object
 * @param node (void *)(struct DataNode *) or (void *)(struct BTreeNode *)
 *             Node to be freed
 *
 * @return     Status
 */
int node_pointer_deallocate(struct DB *db, void *node) {
	return node_deallocate(db, ((struct DataNode *)node)->h->page);
}

/**
 * @brief      Free node's memory
 *
 * @param db   DB object
 * @param node (void *)(struct DataNode *) or (void *)(struct BTreeNode *)
 *             Nodes, which memory to be freed
 *
 * @return     Status
 */
void node_free(struct DB *db, void *node) {
	free(((struct DataNode *)node)->h);
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
int db_init(struct DB *db, char *db_name,
		     uint16_t pageSize, pageno_t poolSize) {
	log_info("Creating DB with name %s", db_name);
	log_info("PoolSize: %zd, PageSize %i", poolSize, pageSize);
	memset(db, 0, sizeof(struct DB));
	db->db_name = db_name;
	db->pool = (struct PagePool *)malloc(sizeof(struct PagePool));
	assert(db->pool);
	pool_init(db->pool, db_name, pageSize, poolSize);
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
	return 0;
}
#define NODE_KEY_POS(NODE, POS)  NODE->keys + (POS) * BTREE_KEY_LEN
#define NODE_CHLD_POS(NODE, POS) NODE->chld + (POS)
#define NODE_VAL_POS(NODE, POS)  NODE->vals + (POS)

#define NODE_FULL(DB, NODE) (NODE->h->size == DB->btree_degree)
#define NODE_HALF(DB)       floor(DB->btree_degree/2)

/**
 * @brief  Insert data into prepared Node
 *
 * @return Status
 */
int _btree_insert_data(struct DB *db, struct BTreeNode *node,
		       void *val, int val_len, size_t pos) {
	struct DataNode dnode;
	node_data_load(db, &dnode, 0);
	dnode.data = val;
	dnode.h->size = val_len;
	dnode.h->flags = IS_DATA;
	node->vals[pos] = dnode.h->page;
	node_data_dump(db, &dnode);
	node_btree_dump(db, node);
	node_free(db, &dnode);
	return 0;
}

/**
 * @brief  Replace data int the Node
 *
 * @return Status
 */
int _btree_replace_data(struct DB *db, struct BTreeNode *node,
		char *val, int val_len, size_t pos) {
	node_deallocate(db, node->vals[pos]);
	_btree_insert_data(db, node, val, val_len, pos);
	return 0;
}

/**
 * @brief Prepare node for inserting data
 */
void _btree_insert_into_node_prepare(struct DB *db, struct BTreeNode *node,
				     char *key, size_t pos, size_t child) {
	int isLeaf = node->h->flags & IS_LEAF;
	assert(!(isLeaf ^ !child) || NODE_FULL(db, node));
	memmove(NODE_KEY_POS(node, pos + 1),
		NODE_KEY_POS(node, pos),
		(node->h->size - pos) * BTREE_KEY_LEN);
	memmove(NODE_VAL_POS(node, pos + 1),
		NODE_VAL_POS(node, pos),
		(node->h->size - pos) * sizeof(size_t));
	if (!isLeaf) {
		memmove(NODE_CHLD_POS(node, pos + 2),
			NODE_CHLD_POS(node, pos + 1),
			(node->h->size - pos) * sizeof(size_t));
		node->chld[pos + 1] = child;
	}
	node->h->size += 1;
	strncpy(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN-1);
}

/**
 * @brief  Insert into B-Tree by string and pointer
 *
 * @return Status
 */
int _btree_insert_into_node_sp(struct DB *db, struct BTreeNode *node, char *key,
			size_t data_page, size_t pos, size_t child) {
	_btree_insert_into_node_prepare(db, node, key, pos, child);
	node->vals[pos] = data_page;
	return node_btree_dump(db, node);
}

/**
 * @brief  Insert into B-Tree by string and string
 *
 * @return Status
 */
int _btree_insert_into_node_ss(struct DB *db, struct BTreeNode *node, char *key,
			char *val, int val_len, size_t pos, size_t child) {
	_btree_insert_into_node_prepare(db, node, key, pos, child);
	_btree_insert_data(db, node, val, val_len, pos);
	return node_btree_dump(db, node);
}

/**
 * @brief  B-Tree split operation
 *
 * @return Status
 */
int _btree_split_node(struct DB *db,
		struct BTreeNode *parent,
		struct BTreeNode *node) {
	assert(!(parent && NODE_FULL(db, parent)));
	int isLeaf = node->h->flags & IS_LEAF;
	size_t middle = ceil((double )node->h->size/2) - 1;
	struct BTreeNode right;
	node_btree_load(db, &right, 0);
	if (isLeaf) right.h->flags |= IS_LEAF;
	memcpy(right.keys, NODE_KEY_POS(node, middle + 1),
	       BTREE_KEY_LEN  * (node->h->size - middle - 1));
	memcpy(right.vals, NODE_VAL_POS(node, middle + 1),
	       sizeof(size_t) * (node->h->size - middle - 1));
	memcpy(right.chld, NODE_CHLD_POS(node, middle + 1),
	       sizeof(size_t) * (node->h->size - middle));
#ifdef DEBUG
	memset(NODE_KEY_POS(node, middle + 1), 0,
	       BTREE_KEY_LEN * (node->h->size - middle - 1));
	memset(NODE_VAL_POS(node, middle + 1), 0,
	       sizeof(size_t) * (node->h->size - middle - 1));
	memset(NODE_CHLD_POS(node, middle + 1), 0,
	       sizeof(size_t) * (node->h->size - middle));
#endif
	right.h->size = node->h->size - middle - 1;
	node->h->size = middle + 1;
	if (parent == NULL) { /* It's top node */
		struct BTreeNode left;
		node_btree_load(db, &left, 0);
		if (isLeaf) left.h->flags |= IS_LEAF;
		if (isLeaf) node->h->flags ^= IS_LEAF;
		memcpy(left.keys, node->keys, BTREE_KEY_LEN  * middle);
		memcpy(left.vals, node->vals, sizeof(size_t) * middle);
		memcpy(left.chld, node->chld, sizeof(size_t) * (middle + 1));
#ifdef DEBUG
		memset(node->keys, 0, BTREE_KEY_LEN * middle);
		memset(node->vals, 0, sizeof(size_t) * middle);
		memset(node->chld, 0, sizeof(size_t) * (middle + 1));
#endif
		left.h->size = middle;
		memmove(node->keys, NODE_KEY_POS(node, middle), BTREE_KEY_LEN);
		node->vals[0] = node->vals[middle];
		node->chld[0] = left.h->page;
		node->chld[1] = right.h->page;
		node->h->size = 1;
		node_btree_dump(db, &left);
		node_free(db, &left);
	} else {
		size_t pos =  0;
		while (pos < parent->h->size && node->h->page != parent->chld[pos])
			pos++;
//		assert(pos == parent->h->size);
		_btree_insert_into_node_sp(db, parent,
					   NODE_KEY_POS(node, middle),
				           node->vals[middle], pos,
					   right.h->page);
		--node->h->size;
	}
	node_btree_dump(db, node);
	node_btree_dump(db, &right);
	node_free(db, &right);
	return 0;
}

/**
 * @brief  B-Tree insert operation
 *
 * @return Status
 */
int _btree_insert(struct DB *db, struct BTreeNode *node,
		  char *key, char *val, int val_len) {
	if (node->h->flags & IS_TOP && NODE_FULL(db, node)) /* UNLIKELY */
		_btree_split_node(db, NULL, node);
	size_t pos =  0;
	int    cmp = -1;
	while (pos < node->h->size) {
		cmp = strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN);
		if (cmp >= 0)
			break;
		++pos;
	}
	if (cmp == 0) {
		_btree_replace_data(db, node, val, val_len, pos);
		return 0;
	}
	if (node->h->flags & IS_LEAF) {
		_btree_insert_into_node_ss(db, node, key, val, val_len, pos, 0);
	} else {
		struct BTreeNode child;
		node_btree_load(db, &child, node->chld[pos]);
		if (NODE_FULL(db, (&child))) {
			_btree_split_node(db, node, &child);
			if (strncmp(NODE_KEY_POS(node, pos),
				    key, BTREE_KEY_LEN) > 0) {
				_btree_insert(db, &child, key, val, val_len);
			} else {
				struct BTreeNode rnode;
				node_btree_load(db, &rnode, node->chld[pos+1]);
				_btree_insert(db, &rnode, key, val, val_len);
				node_btree_dump(db, &rnode);
				node_free(db, &rnode);
			}
		} else {
			_btree_insert(db, &child, key, val, val_len);
		}
		node_btree_dump(db, &child);
		node_free(db, &child);
	}
	return 0;
}

/**
 * @brief  DB Insert Wrapper
 *
 * @return Status
 */
int db_insert(struct DB *db, char *key, char *val, int val_len) {
	log_info("Inserting value into DB with key '%s'", key);
	return _btree_insert(db, db->top, key, val, val_len);
}

/**
 * @brief  B-Tree search operation
 *
 * @return Status
 */
pageno_t _btree_search(struct DB *db, struct BTreeNode *node,
		       void *key, size_t *val_len) {
	*val_len = 0;
	size_t pos = 0;
	int cmp = 0;
	while (pos < node->h->size) {
		cmp = strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN);
		if (cmp >= 0)
			break;
		pos++;
	}
	if (pos < node->h->size && cmp == 0)
		return node->vals[pos];
	if (node->h->flags & IS_LEAF)
		return 0;
	struct BTreeNode kid;
	node_btree_load(db, &kid, node->chld[pos]);
	pageno_t ret = _btree_search(db, &kid, key, val_len);
	node_free(db, &kid);
	return ret;
}

/**
 * @brief  DB Search wrapper
 *
 * @return Status
 */
int db_search(struct DB *db, char *key, void **val, size_t *val_len) {
	log_info("Searching value in the DB with key '%s'", key);
	pageno_t ret = _btree_search(db, db->top, key, val_len);
	if (ret == 0) return 0;
	struct DataNode node;
	node_data_load(db, &node, ret);
	*val = node.data;
	*val_len = node.h->size;
	node_free(db, &node);
	return 0;
}

// int _btree_delete_replace_max(struct DB *db, struct BTreeNode *node,
// 		size_t pos, struct BTreeNode *left) {
// 	struct BTreeNode *left_bkp = left;
// 	struct BTreeNode *left_new = NULL;
// 	while (!(left->h->flags & IS_LEAF)) {
// 		left_new = page_node_read(db, left->chld[left->h->size]);
// 		if (left != left_bkp) free(left);
// 		left = left_new;
// 	}
// 	memcpy(NODE_KEY_POS(left, left->h->size-1), NODE_KEY_POS(node, pos), BTREE_KEY_LEN);
// 	node->vals[pos] = left->vals[left->h->size-1];
// 	page_node_write(db, left);
// 	page_node_write(db, left_bkp);
// 	return 0;
// }
//
// int _btree_delete_replace_min(struct DB *db, struct BTreeNode *node,
// 		size_t pos, struct BTreeNode *right) {
// 	struct BTreeNode *right_bkp = right;
// 	struct BTreeNode *right_new = NULL;
// 	while (!(right->h->flags & IS_LEAF)) {
// 		right_new = page_node_read(db, right->chld[0]);
// 		if (right != right_bkp) free(right);
// 		right = right_new;
// 	}
// 	memcpy(NODE_KEY_POS(right, 0), NODE_KEY_POS(node, pos), BTREE_KEY_LEN);
// 	node->vals[pos] = right->vals[right->h->size-1];
// 	page_node_write(db, right);
// 	page_node_write(db, right);
// 	return 0;
// }
//
// int _btree_merge_nodes(struct DB *db, struct BTreeNode *node,
// 		size_t pos, struct BTreeNode *left, struct BTreeNode *right) {
// 	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(left, left->h->size), BTREE_KEY_LEN);
// 	left->vals[left->h->size] = node->vals[pos];
// 	memmove(NODE_KEY_POS(node, pos), NODE_KEY_POS(node, pos + 1),
// 			BTREE_KEY_LEN * (node->h->size - pos - 1));
// 	memmove(NODE_VAL_POS(node, pos), NODE_VAL_POS(node, pos + 1),
// 			sizeof(size_t) * (node->h->size - pos - 1));
// 	memmove(NODE_VAL_POS(node, pos + 1), NODE_VAL_POS(node, pos + 2),
// 			sizeof(size_t) *(node->h->size - pos));
// 	--node->h->size;
// 	++left->h->size;
// 	memcpy(NODE_KEY_POS(left, left->h->size), NODE_KEY_POS(right, 0),
// 			BTREE_KEY_LEN * right->h->size);
// 	memcpy(NODE_VAL_POS(left, left->h->size), NODE_VAL_POS(right, 0),
// 			sizeof(size_t) * right->h->size);
// 	memcpy(NODE_VAL_POS(left, left->h->size - 1), NODE_VAL_POS(right, 0),
// 			sizeof(size_t) * right->h->size);
// 	left->h->size += right->h->size;
// 	right->h->size = 0;
// 	return 0;
// }
//
// int _btree_transfuse_to_left(struct DB *db, struct BTreeNode *node,
// 		size_t pos, struct BTreeNode *to, struct BTreeNode *from) {
// 	/*
// 	 * node->pos_key -> append(to, key)
// 	 * from->begin_key -> node->pos_key
// 	 * from->begin_chld -> append(to, chld)
// 	 * */
// 	memcpy(NODE_KEY_POS(to, to->h->size), NODE_KEY_POS(node, pos),
// 			BTREE_KEY_LEN);
// 	to->vals[to->h->size] = node->vals[pos];
// 	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(from, 0), BTREE_KEY_LEN);
// 	node->vals[pos] = from->vals[0];
// 	to->chld[to->h->size] = from->chld[0];
// 	memcpy(NODE_KEY_POS(from, 0), NODE_KEY_POS(from, 1),
// 			BTREE_KEY_LEN * (from->h->size - 1));
// 	memcpy(NODE_VAL_POS(from, 0), NODE_VAL_POS(from, 1),
// 			sizeof(size_t)* (from->h->size - 1));
// 	memcpy(NODE_CHLD_POS(from, 0), NODE_CHLD_POS(from, 1),
// 			sizeof(size_t)* (from->h->size - 1));
// 	--from->h->size;
// 	++to->h->size;
// 	return 0;
// }
//
// int _btree_transfuse_to_right(struct DB *db, struct BTreeNode *node,
// 		size_t pos, struct BTreeNode *to, struct BTreeNode *from) {
// 	/*
// 	 * node->pos_key -> append(to, key)
// 	 * from->begin_key -> node->pos_key
// 	 * from->begin_chld -> append(to, chld)
// 	 * */
// 	memcpy(NODE_KEY_POS(to, 1), NODE_KEY_POS(to, 0),
// 			BTREE_KEY_LEN * to->h->size);
// 	memcpy(NODE_VAL_POS(to, 1), NODE_VAL_POS(to, 0),
// 			sizeof(size_t)* to->h->size);
// 	memcpy(NODE_CHLD_POS(to, 1), NODE_CHLD_POS(to, 0),
// 			sizeof(size_t)* to->h->size);
//
// 	memcpy(NODE_KEY_POS(to, 0), NODE_KEY_POS(node, pos),
// 			BTREE_KEY_LEN);
// 	to->vals[0] = node->vals[pos];
//
// 	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(from, from->h->size-1), BTREE_KEY_LEN);
// 	node->vals[pos] = from->vals[from->h->size-1];
// 	to->chld[0] = from->chld[from->h->size-1];
// 	--from->h->size;
// 	++to->h->size;
// 	return 0;
// }
//
// int _btree_delete(struct DB *db, struct BTreeNode *node, void *key) {
// 	int pos = 0;
// 	int cmp = 0;
// 	while (pos < node->h->size && ((cmp =
// 			strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN)) < 0))
// 		++pos;
// //	if (cmp < 0) --pos;
// 	int isLeaf = node->h->flags & IS_LEAF;
// 	if (isLeaf && pos < node->h->size && !cmp) {
// 		memmove(NODE_KEY_POS(node, pos), NODE_KEY_POS(node, pos + 1),
// 				BTREE_KEY_LEN * (node->h->size - pos - 1));
// 		memmove(NODE_VAL_POS(node, pos), NODE_VAL_POS(node, pos + 1),
// 				sizeof(size_t) * (node->h->size - pos - 1));
// 	} else if (isLeaf && cmp) {
// 		return 0;
// 	} else if (!isLeaf && !cmp) {
// 		do {
// 			struct BTreeNode *kid_left = NULL;
// 			struct BTreeNode *kid_right = NULL;
// 			kid_left = page_node_read(db, node->chld[pos]);
// 			if (kid_left->h->size > NODE_HALF(db)) {
// 				_btree_delete_replace_max(db, node, pos, kid_left);
// 				_btree_delete(db, kid_left, key);
// 				free(kid_left);
// 				break;
// 			}
// 			kid_right = page_node_read(db, node->chld[pos+1]);
// 			if (kid_right->h->size > NODE_HALF(db)) {
// 				_btree_delete_replace_min(db, node, pos, kid_right);
// 				_btree_delete(db, kid_right, key);
// 				free(kid_left);
// 				free(kid_right);
// 				break;
// 			}
//
// 			/* TODO: May become empty*/
// 			_btree_merge_nodes(db, node, pos, kid_left, kid_right);
// 			page_free(db, kid_right->page);
// 			_btree_delete(db, kid_left, key);
// 			free(kid_left);
// 			free(kid_right);
// 		} while (0);
// 	} else {
// 		struct BTreeNode *kid = page_node_read(db, node->chld[pos]);
// 		if ((kid->h->size) <= NODE_HALF(db)) {
// 			struct BTreeNode *kid_left = NULL;
// 			struct BTreeNode *kid_right = NULL;
// 			do {
// 				kid_left = page_node_read(db, node->chld[pos]);
// 				if (kid_left->h->size > NODE_HALF(db)) {
// 					_btree_transfuse_to_left(db, node, pos,
// 							kid, kid_left);
// 					break;
// 				}
// 				kid_right = page_node_read(db, node->chld[pos+1]);
// 				if (kid_right->h->size > NODE_HALF(db)) {
// 					_btree_transfuse_to_right(db, node, pos,
// 							kid, kid_right);
// 					free(kid_right);
// 					break;
// 				}
// 				_btree_merge_nodes(db, node, pos, kid, kid_right);
// 				page_free(db, kid_right->page);
// 				_btree_delete(db, kid_left, key);
// 				free(kid_right);
// 			} while (0);
// 			if (kid_left->h->size > NODE_HALF(db)) {
// 			} else if (kid_right->h->size > NODE_HALF(db)) {
// 				_btree_transfuse_to_right(db, node, pos, kid, kid_right);
// 			} else {
// 				_btree_merge_nodes(db, node, pos, kid, kid_right);
// 				page_free(db, kid_right->page);
// 				_btree_delete(db, kid_right, key);
// 			}
// 			free(kid_left);
// 			free(kid_right);
// 		}
// 		_btree_delete(db, kid, key);
// 		free(kid);
// 	}
// 	return 0;
// }
//
// int db_delete(struct DB *db, char *key) {
// 	log_info("Deleting value from DB with key '%s'", key);
// 	return _btree_delete(db->pool, db->top, key);
// }
//
int print_node(struct BTreeNode *node) {
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
	print_node(node);
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
	size_t dbsize;
	size_t chunk_size;
};

struct DB *dbcreate(char *file, struct DBC config) {
	struct DB *db = (struct DB *)malloc(sizeof(struct DB));
	db_init(db, file, config.chunk_size, config.dbsize);
	return db;
}

//int main() {
//	struct DB db;
//	db_init(&db, "mydb", 512, 128*1024*104);
//	db_insert(&db, "1234", "Hello, world1", 13);
//	db_insert(&db, "1235", "Hello, world2", 13);
//	db_insert(&db, "1236", "Hello, world3", 13);
//	db_insert(&db, "1237", "Hello, world4", 13);
//	db_insert(&db, "1238", "Hello, world5", 13);
//	db_insert(&db, "1232", "Hello, world6", 13);
//	db_insert(&db, "1231", "Hello, world7", 13);
//	db_insert(&db, "1240", "Hello, world8", 13);
//	db_print(&db);
//	char *data = NULL;
//	size_t val = 0;
//	db_search(&db, "1231", &data, &val);
//	printf("\n%13s\n", data);
//	db_free(&db);
//	return 0;
//}
