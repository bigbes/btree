#include "pagepool.h"
#include "dbg.h"       /* log_err
			* log_info
			*/
#include "bit/bit.h"   /* bit_set
			* bit_test
			* bit_clear
			* struct bit_iterator
			* bit_iterator_init
			* bit_iterator_next
			*/
#include <math.h>      /* ceil
			*/
#include <errno.h>     /* strerror
			* errno
			*/


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
	bit_iterator_init(pp->it, (const void *)pp->bitmask,
			  pp->pageSize*pp->nPages, false);
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
static inline ssize_t bitmask_load(struct PagePool *pp) {
	log_info("Load bitmask");
	ssize_t ans = pread(pp->fd, pp->bitmask, pp->pageSize * bitmask_pages(pp), 0);
	bitmask_it_init(pp);
	return ans;
}

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
int pool_free(struct PagePool *pp, pageno_t pos) {
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
void *pool_read(struct PagePool *pp, pageno_t pos) {
	log_info("Reading Node %zd", num);
	void *block = malloc(pp->pageSize);
	assert(block);
	int retval = pread(pp->fd, block, pp->pageSize, pos * pp->pageSize);
	if (retval == -1) {
		log_err("Reading %zd bytes from page %zd failed\n", pp->pageSize, num);
		log_err("Error while reading %d: %s\n", errno, strerror(errno));
		free(node);
		return NULL;
	}
	return block;
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
	assert(size + offset < pp->pageSize);
	log_info("Dumping Node %zd", page);
	int retval = pwrite(pp->fd, data, size, pos * pp->pageSize + offset);
	if (retval == -1) {
		log_err("Writing %zd bytes to page %zd failed\n", to_write, page);
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
	pp->fd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	assert(pp->fd != -1);
	pp->pageSize = pageSize;
	pp->poolSize = poolSize;
	pp->nPages = ceil(poolSize/pageSize);
	posix_fallocate(pp->fd, 0, poolSize);
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
	free(pp);
	return 0;
}
