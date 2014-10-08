# BTree


BTree Functions:
```
#define DEFAULT_DB_SIZE   134217728
#define DEFAULT_PAGE_SIZE 4096

typedef struct PagePool {
    int     fd;
    size_t  pageSize = 4096;
    size_t  nPages;
    void   *bitmask;
} PagePool;

typedef struct Page {
    uint8_t   flags;
    uint64_t  page_next;
    void     *data[4096 - 1 - 8];
} Page;

typedef BTreeNode {
    size_t    page;
    uint32_t  nKeys;
    uint8_t   flags;
    size_t   *chld;
    char     *keys;
    size_t   *vals;
} BTreeNode;

/* NP -> Number of pages,
 * PS -> PAGE_SIZE
 */
#define OPTIMAL_SIZE(NP, PS) (NP * (PS - 9) - 2*sizeof(size_t) - 5) \
        / (sizeof(size_t)*2 + 128)

/* Store Cache in List */
typedef struct PageList {
    size_t key;
    struct Page *value;
    struct PageList *next;
    struct PageList *last; /* for fast inserting */
} PageList;

typedef struct DB_TX {
    struct PagePool *pool;
    struct PageList *list;
} DB_TX;

typedef struct DB {
    char             *db_name;
    struct PagePool  *pool;
    struct BTreeNode *top;
} DB;
```

```
struct BTreeNode *btree_create (struct PagePool *pp);
size_t btree_search (struct PagePool *pp, struct BTreeNode *x, void *key);
size_t btree_delete (struct PagePool *pp, struct BTreeNode *x, void *key);
size_t btree_insert (struct PagePool *pp, struct BTreeNode *x, void *key, size_t data_page);
```

```
struct DB_TX *tx_start();                 // - simple cache, not transaction
tx_dump(struct DB *db, struct DB_TX *tx); // - write transaction onto disk
tx_rollback(struct DB_TX *tx);            // - simply empty cache
```

```
struct DB *db_create(char *db_name, size_t size);
size_t db_select(struct DB *db, char *key);
size_t db_insert(struct DB *db, char *key, char *val, int val_len);
size_t db_delete(struct DB *db, void *key);
size_t db_free(struct DB *db);
```

[BitArray](https://github.com/noporpoise/BitArray)  
[antirez append only BTree](https://github.com/antirez/otree)  
[BTree Visualization](https://www.cs.usfca.edu/~galles/visualization/BTree.html)  

[Introduction to Algorithms. Third Edition. Cormen, Leiserson, Rivest](http://umsl.edu/~mfrp9/misc/ia.pdf)  
[Lectures - B-Trees](http://www.cs.umd.edu/~meesh/420/Notes/MountNotes/lecture12-btree.pdf)  
[Lectures - Balanced Search Trees - Overview](http://www.cs.cmu.edu/afs/cs/academic/class/15451-s10/www/lectures/lect0205.pdf)  
Part of:  
[Avrim Blum - Lectures  1-10]( http://www.cs.cmu.edu/afs/cs/academic/class/15451-s10/www/lectures/lects1-10.pdf)  
[Avrim Blum - Lectures 11-20](http://www.cs.cmu.edu/afs/cs/academic/class/15451-s10/www/lectures/lects11-20.pdf)  

[Deletion in B+-Trees](http://ilpubs.stanford.edu:8090/85/1/1995-19.pdf)  
[Deletion without rebalancing in Multiway Search Trees](http://www.cs.princeton.edu/~sssix/papers/relaxed-b-trees-journal.pdf)  
[Cache-Oblivious B-Trees](http://erikdemaine.org/papers/CacheObliviousBTrees_SICOMP/paper.pdf)  
[S(b)-Trees: An Optimal Balancing of Variable Length Keys](http://people.apache.org/~shv/docs/amcs04.pdf)  
[Space Saving Generalization of B-Trees with 2/3 Utilization](http://people.apache.org/~shv/docs/s2tree.pdf)  
[A Modified and Memory Saving Approach to B+ Tree Index for Search of an Image Database based on Chain Codes](http://www.ijcaonline.org/volume9/number3/pxc3871843.pdf)  
[An Efficient B-Tree Layer for Flash-Memory Storage Systems](http://lambda.csail.mit.edu/~chet/papers/others/l/log-structured/rtcsa03_btreeflash.pdf)  
[Don’t Thrash: How to Cache Your Hash on Flash](http://arxiv.org/pdf/1208.0290.pdf)  
[Dynamical Storage Allocation: A survey and Critical Review](http://www.cs.northwestern.edu/~pdinda/ics-s05/doc/dsa.pdf)  
[The Ubiquitous B-Trees](http://people.cs.aau.dk/~simas/aalg06/UbiquitBtree.pdf)  
