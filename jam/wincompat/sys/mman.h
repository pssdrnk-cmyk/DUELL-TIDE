/* JAM: Windows (llvm-mingw) stand-in for <sys/mman.h>. The host tests map anonymous shared memory to read back what a
 * fork()ed child wrote: MAP_SHARED is a pagefile-backed section view, which a cloned child (sys/wait.h) shares with its parent;
 * MAP_PRIVATE is plain zeroed memory. */
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

extern void *__stdcall jam_CreateMap(void *, void *, unsigned long, unsigned long, unsigned long, const char *) __asm__("CreateFileMappingA");
extern void *__stdcall jam_MapView(void *, unsigned long, unsigned long, unsigned long, size_t) __asm__("MapViewOfFile");
extern int __stdcall jam_Unmap(const void *) __asm__("UnmapViewOfFile");
extern int __stdcall jam_Close2(void *) __asm__("CloseHandle");
static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    (void)addr; (void)prot; (void)off;
    if (!(flags & MAP_ANONYMOUS) || fd != -1)
        return MAP_FAILED;                  /* file mappings: no host test needs one */
    if (flags & MAP_SHARED) {
        void *m = jam_CreateMap((void *)-1, 0, 4 /* PAGE_READWRITE */, (unsigned long)((unsigned long long)len >> 32), (unsigned long)len, 0);
        void *p = m ? jam_MapView(m, 0xF001F /* FILE_MAP_ALL_ACCESS */, 0, 0, len) : 0;
        if (m) jam_Close2(m);                           /* the view keeps the section alive */
        return p ? p : MAP_FAILED;
    }
    void *p = calloc(1, len ? len : 1);
    return p ? p : MAP_FAILED;
}
static inline int munmap(void *addr, size_t len)
{
    (void)len;
    if (jam_Unmap(addr)) return 0;                      /* a shared view; otherwise it came from calloc */
    free(addr);
    return 0;
}
#endif
