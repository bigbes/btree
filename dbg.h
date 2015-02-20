#ifndef __dbg_h__
#define __dbg_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef NDEBUG
#  define debug(M, ...)
#  define log_info(M, ...)
#else
#  define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#  define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...)  fprintf(stderr, "[ERROR] (%s:%d: errno: %s (%d)) " M "\n", \
				 __FILE__, __LINE__, clean_errno(), errno, ##__VA_ARGS__)
#define log_warn(M, ...) fprintf(stderr, "[WARN] (%s:%d: errno: %s (%d)) " M "\n", \
				 __FILE__, __LINE__, clean_errno(), errno, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto error; }
#define sentinel(M, ...)          { log_err(M, ##__VA_ARGS__); errno=0; goto error; }

#define check_mem(A, SZ)  check(A, "Failed to allocate %zd bytes", (SZ))
#define check_diskw(A, SZ) check((A) != -1, "Failed to write %zd bytes to disk (wrote %zd)", (SZ), (A))
#define check_diskr(A, SZ) check((A) != -1, "Failed to read %zd bytes from disk (read %zd)", (SZ), (A))
#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__); errno=0; goto error; }

#define check_diskpw(A, SZ, P) check((A) != -1, "Failed to write %zd bytes to disk (page %zd, wrote %zd)", (SZ), (P), (A))
#define check_diskpr(A, SZ, P) check((A) != -1, "Failed to read %zd bytes from disk (page %zd, read %zd)", (SZ), (P), (A))

#endif
