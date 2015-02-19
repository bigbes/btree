#include <string.h>
#include <math.h>

#include "node.h"
#include "assert.h"
#include "btree.h"
#include "insert.h"

/**
 * @brief  Insert data into prepared Node
 *
 * @return Status
 */
int btreei_insert_data(struct DB *db, struct BTreeNode *node,
		       void *val, int val_len, size_t pos) {
	struct DataNode dnode = {0};
	node_data_load(db, &dnode, 0);
	memcpy(dnode.data, val, val_len);
	dnode.h->size = val_len;
	node->vals[pos] = dnode.h->page;
	node_data_dump(db, &dnode);
	node_btree_dump(db, node);
	node_free(db, &dnode);
	return 0;
}

/**
 * @brief  Replace data in the Node
 *
 * @return Status
 */
int btreei_replace_data(struct DB *db, struct BTreeNode *node,
		char *val, int val_len, size_t pos) {
	node_deallocate(db, node->vals[pos]);
	btreei_insert_data(db, node, val, val_len, pos);
	return 0;
}

/**
 * @brief Prepare node for inserting data
 */
void btreei_insert_into_node_prepare(struct DB *db, struct BTreeNode *node,
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
int btreei_insert_into_node_sp(struct DB *db, struct BTreeNode *node, char *key,
			size_t data_page, size_t pos, size_t child) {
	btreei_insert_into_node_prepare(db, node, key, pos, child);
	node->vals[pos] = data_page;
	return node_btree_dump(db, node);
}

/**
 * @brief  Insert into B-Tree by string and string
 *
 * @return Status
 */
int btreei_insert_into_node_ss(struct DB *db, struct BTreeNode *node, char *key,
			char *val, int val_len, size_t pos, size_t child) {
	btreei_insert_into_node_prepare(db, node, key, pos, child);
	btreei_insert_data(db, node, val, val_len, pos);
	return node_btree_dump(db, node);
}

/**
 * @brief  B-Tree split operation
 *
 * @return Status
 */
int btreei_split_node(struct DB *db,
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
// 	memset(NODE_KEY_POS(node, middle + 1), 0,
// 	       BTREE_KEY_LEN * (node->h->size - middle - 1));
// 	memset(NODE_VAL_POS(node, middle + 1), 0,
// 	       sizeof(size_t) * (node->h->size - middle - 1));
// 	memset(NODE_CHLD_POS(node, middle + 1), 0,
// 	       sizeof(size_t) * (node->h->size - middle));
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
// 		memset(node->keys, 0, BTREE_KEY_LEN * middle);
// 		memset(node->vals, 0, sizeof(size_t) * middle);
// 		memset(node->chld, 0, sizeof(size_t) * (middle + 1));
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
		btreei_insert_into_node_sp(db, parent,
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
int btreei_insert(struct DB *db, struct BTreeNode *node,
		  char *key, char *val, int val_len) {
	if (node->h->flags & IS_TOP && NODE_FULL(db, node)) /* UNLIKELY */
		btreei_split_node(db, NULL, node);
	size_t pos =  0;
	int    cmp = -1;
	while (pos < node->h->size) {
		cmp = strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN);
		if (cmp >= 0)
			break;
		++pos;
	}
	if (cmp == 0) {
		btreei_replace_data(db, node, val, val_len, pos);
		return 0;
	}
	if (node->h->flags & IS_LEAF) {
		btreei_insert_into_node_ss(db, node, key, val, val_len, pos, 0);
	} else {
		struct BTreeNode child;
		node_btree_load(db, &child, node->chld[pos]);
		if (NODE_FULL(db, (&child))) {
			btreei_split_node(db, node, &child);
			if (strncmp(NODE_KEY_POS(node, pos),
				    key, BTREE_KEY_LEN) > 0) {
				btreei_insert(db, &child, key, val, val_len);
			} else {
				struct BTreeNode rnode;
				node_btree_load(db, &rnode, node->chld[pos+1]);
				btreei_insert(db, &rnode, key, val, val_len);
				node_btree_dump(db, &rnode);
				node_free(db, &rnode);
			}
		} else {
			btreei_insert(db, &child, key, val, val_len);
		}
		node_btree_dump(db, &child);
		node_free(db, &child);
	}
	return 0;
}

