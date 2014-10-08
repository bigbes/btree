
/*
 * 4096 - 4 - 1 - (2*K + 1) * 8 - (K * 128)
 */

/* Must be Odd number */
#define BTREE_KEY_CNT 3
#define BTREE_KEY_LEN 128
#define BTREE_VAL_LEN 4092
#define BTREE_CHLD_CNT (BTREE_KEY_CNT+1)

#define DEFAULT_SIZEOF_PAGE      4096
#define DEFAULT_PREALLOCATE_SIZE 128*1024*1024
#define PAGE_NUMBER (PREALLOCATE_SIZE/SIZEOF_PAGE)
#define PAGES_FOR_BITMAP PAGE_NUMBER/(SIZEOF_PAGE * 8)

/*
typedef struct {
	uint32_t flags;
	uint32_t cachesize;
	uint32_t psize;
	int32_t(*compare)(const DBT *key1, const DBT *key2);
	size_t (*prefix) (const DBT *key1, const DBT *key2);
} BTREEINFO;
*/

enum NodeFlags {
	IS_LEAF = 0x01,
/*	____RES = 0x02,*/
/*	____RES = 0x04,*/
/*	____RES = 0x08,*/
/*	____RES = 0x10,*/
/*	____RES = 0x20,*/
/*	____RES = 0x40,*/
/*	____RES = 0x80,*/
};

struct PagePool {
	int fd;
	size_t pageSize;
	size_t nPages;
	void *bitmask;
};

struct BTreeNode {
	size_t   page;
	size_t   parentPage;
	uint32_t nKeys;
	uint8_t  flags;
	size_t   chld [BTREE_CHLD_CNT];
	char     keys [BTREE_KEY_CNT * BTREE_KEY_LEN];
	size_t   vals [BTREE_KEY_CNT];
};

struct DB {
	char  *db_name;
	struct PagePool *pool;
	struct BTreeNode *top;
};

struct DataPageMeta{
	size_t dataSize;
	size_t nextPage;
};
