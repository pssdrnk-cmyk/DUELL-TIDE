/* JAM: Windows (llvm-mingw) stand-in for <sys/mman.h>. The host tests map anonymous shared memory only to read back what a
 * fork()ed child wrote. Windows has no fork() (sys/wait.h stops the test there), so the mapping is plain zeroed memory of this
 * process: every check before the first fork() sees what it would on Linux and macOS. */
#ifndef JAM_WINCOMPAT_SYS_MMAN_H
#define JAM_WINCOMPAT_SYS_MMAN_H
#include <stdlib.h>
#include <sys/types.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)

static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    (void)addr; (void)prot; (void)off;
    if (!(flags & MAP_ANONYMOUS) || fd != -1)
        return MAP_FAILED;                  /* file mappings: no host test needs one */
    void *p = calloc(1, len ? len : 1);
    return p ? p : MAP_FAILED;
}
static inline int munmap(void *addr, size_t len) { (void)len; free(addr); return 0; }
#endif
