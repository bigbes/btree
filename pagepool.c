#include "pagepool.h"
#include "cache.h"
#include "dbg.h"       /* log_err
			* log_info
			*/
#include <bit.h>       /* bit_set
			* bit_test
			* bit_clear
			* struct bit_iterator
			* bit_iterator_init
			* bit_iterator_next
			*/
#include <math.h>      /* ceil */
#include <errno.h>     /* strerror
			* errno
			*/
#include <stdlib.h>    /* malloc
			* calloc
			*/
#include <assert.h>    /* assert */
#include <unistd.h>    /* pread
			* pwrite
			*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>     /* flags */

#include "wal.h"

#ifdef DEBUG
	#define malloc( calloc(1,
#endif

static int preallocateFile(int fd, uint64_t length) {
#ifdef HAVE_FALLOCATE
	return fallocate( fd, 0, offset, length );
#elif defined(__linux__)
	return posix_fallocate( fd, 0, length );
#elif defined(__APPLE__)
	fstore_t fst;
	fst.fst_flags = F_ALLOCATEALL;
	fst.fst_posmode = F_PEOFPOSMODE;
	fst.fst_offset = 0;
	fst.fst_length = length;
	fst.fst_bytesalloc = 0;
	return fcntl(fd, F_PREALLOCATE, &fst);
#else
	# warning no known method to preallocate files on this platform
	return -1;
#endif
}


/**
 * Get number of bitmask pages
 */
static inline pageno_t bitmask_pages(struct PagePool *pp) {
	pageno_t ans = pp->page_size * CHAR_BIT;
	ans = ceil(((double )pp->nPages) / ans);
	return ans;
}

/**
 * Init byte iterator
 */
static int bitmask_it_init(struct PagePool *pp) {
	if (!pp->it)
		pp->it = (struct bit_iterator *)malloc(sizeof(struct bit_iterator));
	check_mem(pp->it, sizeof(struct bit_iterator));
	bit_iterator_init(pp->it, (const void *)pp->bitmask,
			  pp->page_size*pp->nPages, false);
	return 0;
error:
	exit(-1);
}

/**
 * Dump bitmask pages to the disk
 */
static inline int bitmask_dump(struct PagePool *pp) {
	log_info("Dump bitmask");
	ssize_t retval = pwrite(pp->fd, pp->bitmask, pp->page_size * bitmask_pages(pp), 0);
	check_diskw(retval, pp->page_size * bitmask_pages(pp));
	return retval;
error:
	exit(-1);
}

/**
 * Load bitmask pages from the disk
 */
static inline ssize_t bitmask_load(struct PagePool *pp) {
	log_info("Load bitmask");
	ssize_t retval = pread(pp->fd, pp->bitmask, pp->page_size * bitmask_pages(pp), 0);
	check_diskr(retval, pp->page_size * bitmask_pages(pp));
	bitmask_it_init(pp);
	return retval;
error:
	exit(-1);
}

/**
 * Check page (used or not)
 */
static inline bool bitmask_check(struct PagePool *pp, pageno_t n) {
	return bit_test(pp->bitmask, n);
}

/**
 * Initialize bitmask
 * Mark metadata page and BM page
 */
static int bitmask_init(struct PagePool *pp) {
	pageno_t pagenum = bitmask_pages(pp);
	while (pagenum > 0) bit_set(pp->bitmask, --pagenum);
	bitmask_dump(pp);
	bitmask_it_init(pp);
	return 0;
}

/**
 * Find empty page
 * Returns 0 when can't find empty page
 */
static pageno_t page_find_empty(struct PagePool *pp) {
	pageno_t pos = bit_iterator_next(pp->it);
	if (pos == SIZE_MAX) bitmask_it_init(pp);
	pos = bit_iterator_next(pp->it);
	return (pos == SIZE_MAX ? 0 : pos);
}

/**
 * @brief    Find empty page and reserve it
 *
 * @param pp PagePool instance
 *
 * @return   number of allocated page
 */
pageno_t pool_alloc(struct PagePool *pp) {
	pageno_t pos = page_find_empty(pp);
	if (pos == -1) return 0;
	log_info("Allocating page %zd", pos);
	bit_set(pp->bitmask, pos);
	bitmask_dump(pp);
	return pos;
}

/**
 * @brief     Free previously allocate page
 *
 * @param pp  PagePool instance
 * @param pos Number of page to be freed
 *
 * @return    Status
 */
int pool_dealloc(struct PagePool *pp, pageno_t pos) {
	log_info("Freeing page %zd", pos);
	if (bitmask_check(pp, pos)) return -1;
	bit_clear(pp->bitmask, pos);
	bitmask_dump(pp);
	return 0;
}

/**
 * @brief     Read page content from disk
 *
 * @param pp  PagePool instance
 * @param pos Page to be read
 *
 * @return    Pointer to allocated memory with page content
 *            NULL on error
 */
int pool_read(struct PagePool *pp, pageno_t pos, void *buffer) {
	log_info("Reading Node %zd into buffer", pos);
	ssize_t retval = pread(pp->fd, buffer, pp->page_size, pos * pp->page_size);
	check_diskpr(retval, pp->page_size, pos);
	return 0;
error:
	exit(-1);
}

/**
 * @brief      Write page content to disk
 *
 * @param pp   PagePool instance
 * @param data Pointer to the page content
 * @param size Size of this data
 * @param pos  Page to be written
 *
 * @return     Status
 */
int pool_write(struct PagePool *pp, void *data, size_t size,
	       pageno_t pos, size_t offset) {
	assert(size + offset <= pp->page_size);
	ssize_t retval = pwrite(pp->fd, data, size, pos * pp->page_size + offset);
	check_diskpw(retval, pp->page_size, pos);
	return retval;
error:
	exit(-1);
}


/**
 * @brief      Initialize PagePool object
 *
 * @param[out] pp       PagePool object to instanciate
 * @param[in]  name     Name of file to be used for storage
 * @param[in]  page_size Size of page
 * @param[in]  pool_size Size of file
 *
 * @return     Status
 */
static int pooli_init(struct PagePool *pp, char *name, uint16_t page_size,
	              pageno_t pool_size, size_t cache_size) {
	memset(pp, 0, sizeof(struct PagePool));
	pp->page_size = page_size;
	pp->pool_size = pool_size;
	pp->nPages = ceil(pool_size/page_size);

	pp->cache = (struct CacheBase *)malloc(sizeof(struct CacheBase));
	check_mem(pp->cache, sizeof(struct CacheBase));
	cache_init(pp->cache, pp, cache_size);

	pp->bitmask = (void *)calloc(bitmask_pages(pp) * pp->page_size, 1);
	check_mem(pp->bitmask, bitmask_pages(pp) * pp->page_size);

	return 0;
error:
	exit(-1);
}

int pool_init_new(struct PagePool *pp, char *name, uint16_t page_size,
	          pageno_t pool_size, size_t cache_size) {
	pooli_init(pp, name, page_size, pool_size, cache_size);

	pp->fd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	check(pp->fd != -1, "Can't open file '%s' for read/write", name);
	check(preallocateFile(pp->fd, pool_size) != -1, "Can't preallocate file '%s'", name);

	bitmask_init(pp);
	return 0;
error:
	exit(-1);
}

int pool_init_old(struct PagePool *pp, char *name, uint16_t page_size,
	          pageno_t pool_size, size_t cache_size) {
	pooli_init(pp, name, page_size, pool_size, cache_size);

	pp->fd = open(name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	check(pp->fd != -1, "Can't open file '%s' for read/write", name);

	bitmask_load(pp);
	return 0;
error:
	exit(-1);
}

/**
 * @brief    Free Pool object
 *
 * @param pp Object to free
 *
 * @return   Status
 */
int pool_free(struct PagePool *pp) {
	if (pp->fd) {
		pp->fd = close(pp->fd);
		check(pp->fd != -1, "Failed to close file descriptor for PagePool");
		pp->fd = 0;
	}
	if (pp->bitmask) {
		free(pp->bitmask);
		pp->bitmask = NULL;
	}
	if (pp->it) {
		free(pp->it);
		pp->it = NULL;
	}
	if (pp->cache) {
		cache_free(pp->cache);
		free(pp->cache);
		pp->cache = NULL;
	}
	free(pp);
	return 0;
error:
	exit(-1);
}
