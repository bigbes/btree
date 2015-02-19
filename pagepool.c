#include "pagepool.h"
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
	fst.fst_flags = F_ALLOCATECONTIG;
	fst.fst_posmode = F_PEOFPOSMODE;
	fst.fst_offset = 0;
	fst.fst_length = length;
	fst.fst_bytesalloc = 0;
	return fcntl( fd, F_PREALLOCATE, &fst );
#else
	# warning no known method to preallocate files on this platform
	return -1;
#endif
}


/**
 * Get number of bitmask pages
 */
static inline pageno_t bitmask_pages(struct PagePool *pp) {
	pageno_t ans = pp->pageSize * CHAR_BIT;
	ans = ceil(((double )pp->nPages) / ans);
	return ans;
}

/**
 * Init byte iterator
 */
static int bitmask_it_init(struct PagePool *pp) {
	if (!pp->it)
		pp->it = (struct bit_iterator *)malloc(sizeof(struct bit_iterator));
	assert(pp->it);
	bit_iterator_init(pp->it, (const void *)pp->bitmask,
			  pp->pageSize*pp->nPages, false);
	return 0;
}

/**
 * Dump bitmask pages to the disk
 */
static inline int bitmask_dump(struct PagePool *pp) {
	log_info("Dump bitmask");
	return pwrite(pp->fd, pp->bitmask, pp->pageSize * bitmask_pages(pp), 0);
}

/**
 * Load bitmask pages from the disk
 */
/*static inline ssize_t bitmask_load(struct PagePool *pp) {
	log_info("Load bitmask");
	ssize_t ans = pread(pp->fd, pp->bitmask, pp->pageSize * bitmask_pages(pp), 0);
	bitmask_it_init(pp);
	return ans;
}*/

/**
 * Check page (used or not)
 */
static inline bool bitmask_check(struct PagePool *pp, pageno_t n) {
	return bit_test(pp->bitmask, n);
}

/**
 * Populate bitmask
 * Mark metadata page and BM page
 */
static int bitmask_populate(struct PagePool *pp) {
	pageno_t pagenum = bitmask_pages(pp);
	pp->bitmask = (void *)calloc(bitmask_pages(pp) * pp->pageSize, 1);
	while (pagenum > 0) bit_set(pp->bitmask, --pagenum);
	bitmask_it_init(pp);
	bitmask_dump(pp);
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
void *pool_read(struct PagePool *pp, pageno_t pos, size_t offset, size_t size) {
	assert(size + offset <= pp->pageSize);
	log_info("Reading Node %zd", pos);
	if (!size) size = pp->pageSize;
	void *block = malloc(size);
	assert(block);
	int retval = pread(pp->fd, block, size, pos * pp->pageSize + offset);
	if (retval == -1) {
		log_err("Reading %zd bytes from page %zd failed\n", size, pos);
		log_err("Error while reading %d: %s\n", errno, strerror(errno));
		//free(block);
		return NULL;
	}
	return block;
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
int pool_read_into(struct PagePool *pp, pageno_t pos, void *buffer) {
	log_info("Reading Node %zd into buffer", pos);
	int retval = pread(pp->fd, buffer, pp->pageSize, pos * pp->pageSize);
	if (retval == -1) {
		log_err("Reading %i bytes from page %zd failed\n", pp->pageSize, pos);
		log_err("Error while reading %d: %s\n", errno, strerror(errno));
		return -1;
	}
	return 0;
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
	assert(size + offset <= pp->pageSize);
	int retval = pwrite(pp->fd, data, size, pos * pp->pageSize + offset);
	if (retval == -1) {
		log_err("Writing %zd bytes to page %zd failed\n", size, pos);
		log_err("Error while writing %d: %s\n", errno, strerror(errno));
		return -1;
	}
	return retval;
}


/**
 * @brief      Initialize PagePool object
 *
 * @param[out] pp       PagePool object to instanciate
 * @param[in]  name     Name of file to be used for storage
 * @param[in]  pageSize Size of page
 * @param[in]  poolSize Size of file
 *
 * @return     Status
 */
int pool_init(struct PagePool *pp, char *name, uint16_t pageSize, pageno_t poolSize) {
	memset(pp, 0, sizeof(struct PagePool));
	pp->fd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	assert(pp->fd != -1);
	pp->pageSize = pageSize;
	pp->poolSize = poolSize;
	pp->nPages = ceil(poolSize/pageSize);
	preallocateFile(pp->fd, poolSize);
//	fallocate(pp->fd, 0, 0, poolSize);
//	posix_fallocate(pp->fd, 0, poolSize);
	bitmask_populate(pp);
	return 0;
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
		close(pp->fd);
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
	free(pp);
	return 0;
}
