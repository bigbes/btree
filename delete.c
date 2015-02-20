#include <string.h>
#include <math.h>

#include "node.h"
#include "assert.h"
#include "btree.h"
#include "delete.h"

static int btreei_delete_replace_max(struct DB *db, struct BTreeNode *node,
		size_t pos, struct BTreeNode *left) {
	struct BTreeNode *left_bkp = left;
	struct BTreeNode  left_new;
	while (!(left->h->flags & IS_LEAF)) {
		node_btree_load(db, &left_new, left->chld[left->h->size]);
		//if (left != left_bkp) free(left);
		left = &left_new;
	}
	memcpy(NODE_KEY_POS(left, left->h->size-1), NODE_KEY_POS(node, pos), BTREE_KEY_LEN);
	node->vals[pos] = left->vals[left->h->size-1];
	node_btree_dump(db, left);
	node_btree_dump(db, left_bkp);
	return 0;
}

static int btreei_delete_replace_min(struct DB *db, struct BTreeNode *node,
		              size_t pos, struct BTreeNode *right) {
	struct BTreeNode *right_bkp = right;
	struct BTreeNode  right_new;
	while (!(right->h->flags & IS_LEAF)) {
		node_btree_load(db, &right_new, right->chld[0]);
		//if (right != right_bkp) free(right);
		right = &right_new;
	}
	memcpy(NODE_KEY_POS(right, 0), NODE_KEY_POS(node, pos), BTREE_KEY_LEN);
	node->vals[pos] = right->vals[right->h->size-1];
	node_btree_dump(db, right);
	node_btree_dump(db, right_bkp);
	return 0;
}

static int btreei_merge_nodes(struct DB *db, struct BTreeNode *node, size_t pos,
		       struct BTreeNode *left, struct BTreeNode *right) {
	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(left, left->h->size), BTREE_KEY_LEN);
	left->vals[left->h->size] = node->vals[pos];
	memmove(NODE_KEY_POS(node, pos), NODE_KEY_POS(node, pos + 1),
			BTREE_KEY_LEN * (node->h->size - pos - 1));
	memmove(NODE_VAL_POS(node, pos), NODE_VAL_POS(node, pos + 1),
			sizeof(size_t) * (node->h->size - pos - 1));
	memmove(NODE_VAL_POS(node, pos + 1), NODE_VAL_POS(node, pos + 2),
			sizeof(size_t) *(node->h->size - pos));
	--node->h->size;
	++left->h->size;
	memcpy(NODE_KEY_POS(left, left->h->size), NODE_KEY_POS(right, 0),
			BTREE_KEY_LEN * right->h->size);
	memcpy(NODE_VAL_POS(left, left->h->size), NODE_VAL_POS(right, 0),
			sizeof(size_t) * right->h->size);
	memcpy(NODE_VAL_POS(left, left->h->size - 1), NODE_VAL_POS(right, 0),
			sizeof(size_t) * right->h->size);
	left->h->size += right->h->size;
	right->h->size = 0;
	return 0;
}

static int btreei_transfuse_to_left(struct DB *db, struct BTreeNode *node,
		size_t pos, struct BTreeNode *to, struct BTreeNode *from) {
	/*
	 * node->pos_key -> append(to, key)
	 * from->begin_key -> node->pos_key
	 * from->begin_chld -> append(to, chld)
	 * */
	memcpy(NODE_KEY_POS(to, to->h->size), NODE_KEY_POS(node, pos),
			BTREE_KEY_LEN);
	to->vals[to->h->size] = node->vals[pos];
	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(from, 0), BTREE_KEY_LEN);
	node->vals[pos] = from->vals[0];
	to->chld[to->h->size] = from->chld[0];
	memcpy(NODE_KEY_POS(from, 0), NODE_KEY_POS(from, 1),
			BTREE_KEY_LEN * (from->h->size - 1));
	memcpy(NODE_VAL_POS(from, 0), NODE_VAL_POS(from, 1),
			sizeof(size_t)* (from->h->size - 1));
	memcpy(NODE_CHLD_POS(from, 0), NODE_CHLD_POS(from, 1),
			sizeof(size_t)* (from->h->size - 1));
	--from->h->size;
	++to->h->size;
	return 0;
}

static int btreei_transfuse_to_right(struct DB *db, struct BTreeNode *node,
		size_t pos, struct BTreeNode *to, struct BTreeNode *from) {
	/*
	 * node->pos_key -> append(to, key)
/	 * from->begin_key -> node->pos_key
	 * from->begin_chld -> append(to, chld)
	 * */
	memcpy(NODE_KEY_POS(to, 1), NODE_KEY_POS(to, 0),
			BTREE_KEY_LEN * to->h->size);
	memcpy(NODE_VAL_POS(to, 1), NODE_VAL_POS(to, 0),
			sizeof(size_t)* to->h->size);
	memcpy(NODE_CHLD_POS(to, 1), NODE_CHLD_POS(to, 0),
			sizeof(size_t)* to->h->size);
	
	memcpy(NODE_KEY_POS(to, 0), NODE_KEY_POS(node, pos),
			BTREE_KEY_LEN);
	to->vals[0] = node->vals[pos];
	
	memcpy(NODE_KEY_POS(node, pos), NODE_KEY_POS(from, from->h->size-1), BTREE_KEY_LEN);
	node->vals[pos] = from->vals[from->h->size-1];
	to->chld[0] = from->chld[from->h->size-1];
	--from->h->size;
	++to->h->size;
	return 0;
}

int btreei_delete(struct DB *db, struct BTreeNode *node, void *key) {
	int pos = 0;
	int cmp = 0;
	while (pos < node->h->size && ((cmp =
			strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN)) < 0))
		++pos;
//	if (cmp < 0) --pos;
	int isLeaf = node->h->flags & IS_LEAF;
	if (isLeaf && pos < node->h->size && !cmp) {
		memmove(NODE_KEY_POS(node, pos), NODE_KEY_POS(node, pos + 1),
				BTREE_KEY_LEN * (node->h->size - pos - 1));
		memmove(NODE_VAL_POS(node, pos), NODE_VAL_POS(node, pos + 1),
				sizeof(size_t) * (node->h->size - pos - 1));
	} else if (isLeaf && cmp) {
		return 0;
	} else if (!isLeaf && !cmp) {
		do {
			struct BTreeNode kid_left, kid_right;
			node_btree_load(db, &kid_left, node->chld[pos]);
			if (kid_left.h->size > NODE_HALF(db)) {
				btreei_delete_replace_max(db, node, pos, &kid_left);
				btreei_delete(db, &kid_left, key);
				//free(kid_left);
				break;
			}
			node_btree_load(db, &kid_right, node->chld[pos+1]);
			if (kid_right.h->size > NODE_HALF(db)) {
				btreei_delete_replace_min(db, node, pos, &kid_right);
				btreei_delete(db, &kid_right, key);
				//free(kid_left);
				//free(kid_right);
				break;
			}
//
			/* TODO: May become empty*/
			btreei_merge_nodes(db, node, pos, &kid_left, &kid_right);
			node_deallocate(db, kid_right.h->page);
			btreei_delete(db, &kid_left, key);
			//free(kid_left);
			//free(kid_right);
		} while (0);
	} else {
		struct BTreeNode kid;
		node_btree_load(db, &kid, node->chld[pos]);
		if ((kid.h->size) <= NODE_HALF(db)) {
			struct BTreeNode kid_left, kid_right;
			do {
				node_btree_load(db, &kid_left, node->chld[pos]);
				if (kid_left.h->size > NODE_HALF(db)) {
					btreei_transfuse_to_left(db, node, pos,
							         &kid, &kid_left);
					break;
				}
				node_btree_load(db, &kid_right, node->chld[pos+1]);
				if (kid_right.h->size > NODE_HALF(db)) {
					btreei_transfuse_to_right(db, node, pos,
							          &kid, &kid_right);
					// free(kid_right);
					break;
				}
				btreei_merge_nodes(db, node, pos, &kid, &kid_right);
				node_deallocate(db, kid_right.h->page);
				btreei_delete(db, &kid_left, key);
				// free(kid_right);
			} while (0);
			if (kid_left.h->size > NODE_HALF(db)) {
			} else if (kid_right.h->size > NODE_HALF(db)) {
				btreei_transfuse_to_right(db, node, pos, &kid, &kid_right);
			} else {
				btreei_merge_nodes(db, node, pos, &kid, &kid_right);
				node_deallocate(db, kid_right.h->page);
				btreei_delete(db, &kid_right, key);
			}
			// free(kid_left);
			// free(kid_right);
		}
		btreei_delete(db, &kid, key);
		// free(kid);
	}
	return 0;
}
