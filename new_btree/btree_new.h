#define BTREE_MAX_KEY_LEN 128

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

struct DB {
	char  *db_name;
	struct PagePool *pool;
	struct BTreeNode *top;
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
	struct NodeHeader *header;
	char    *data;
};

inline uint32_t btree_node_max_capacity(pp) {
	double size  = pp->pageSize;
	double size -= sizeof(struct NodeHeader) - sizeof();
	double size /= 2 * sizeof(pageno_t) + BTREE_MAX_KEY_LEN;
	return (uint32_t )size;
}

inline uint32_t data_node_max_capacity(struct PagePool *pp) {
	return (uint32_t )(pp->pageSize - sizeof(struct NodeHeader));
}

/*
void *node_header_init(struct DB *, struct BTreeNode *, void *);
int node_btree_load   (struct DB *, struct BTreeNode *, pageno_t);
int node_btree_dump   (struct DB *, struct BTreeNode *);
int node_data_load    (struct DB *, struct DataNode *, pageno_t);
int node_data_dump    (struct DB *, struct DataNode *);
int node_free         (struct DB *db, pageno_t pos)
int node_pointer_free (struct DB *db, void *node)
*/
