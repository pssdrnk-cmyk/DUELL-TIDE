/* JAM: Windows (llvm-mingw) stand-in for <sys/wait.h> with a working fork(). Windows has no fork(), but ntdll's
 * RtlCloneUserProcess (what the NT "POSIX subsystem" used) clones the calling process copy-on-write: the child continues
 * right after the call with the parent's memory, open CRT descriptors (pipes, files) and the sections mapped by
 * sys/mman.h's MAP_SHARED, and the parent gets the child's process handle. Only the calling thread is cloned (the tests
 * that fork use one thread), and the child has no connection to the console subsystem, so it must end with
 * NtTerminateProcess (_exit below; exit() is routed there too) and stays with plain CRT I/O on inherited handles.
 * If the clone fails the test stops with exit code 77 ("skipped: needs fork"), as before. */
#ifndef JAM_WINCOMPAT_SYS_WAIT_H
#define JAM_WINCOMPAT_SYS_WAIT_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define JAM_NO_FORK_EXIT 77
#define WNOHANG 1
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0)
#define WTERMSIG(s) ((s) & 0x7f)

typedef struct { unsigned long Length; void *Process, *Thread, *Pid, *Tid; char pad[256]; } jam_clone_info;
extern void *__stdcall jam_ModH(const char *) __asm__("GetModuleHandleA");
extern void *__stdcall jam_ProcAddr(void *, const char *) __asm__("GetProcAddress");
/* (ntdll's two calls are looked up when needed: the link line of the tests has no -lntdll) */
static inline void *jam_nt(const char *name) { return jam_ProcAddr(jam_ModH("ntdll.dll"), name); }
#define jam_RtlClone ((long (__stdcall *)(unsigned long, void *, void *, void *, jam_clone_info *))jam_nt("RtlCloneUserProcess"))
#define jam_NtTerm ((long (__stdcall *)(void *, long))jam_nt("NtTerminateProcess"))
extern unsigned long __stdcall jam_WaitObj(void *, unsigned long) __asm__("WaitForSingleObject");
extern int __stdcall jam_ExitCode(void *, unsigned long *) __asm__("GetExitCodeProcess");
extern unsigned long __stdcall jam_GetCwd(unsigned long, unsigned short *) __asm__("GetCurrentDirectoryW");
extern int __stdcall jam_SetCwd(const unsigned short *) __asm__("SetCurrentDirectoryW");
extern int __stdcall jam_CloseH(void *) __asm__("CloseHandle");
extern void *__stdcall jam_CurProc(void) __asm__("GetCurrentProcess");

#define JAM_MAXKIDS 256
static struct { pid_t pid; void *h; } jam_kids[JAM_MAXKIDS];

static inline __attribute__((noreturn)) void jam_kill_self(int code)
{
    fflush(NULL);
    jam_NtTerm(jam_CurProc(), code);
    for (;;) ;
}
static int jam_in_child;
#define _exit(c) (jam_in_child ? jam_kill_self(c) : _exit(c))
#define exit(c) (jam_in_child ? jam_kill_self(c) : exit(c))

static inline pid_t fork(void)
{
    jam_clone_info info = { 0 };
    unsigned short cwd[1024];
    int i;
    long r;
    fflush(NULL);
    cwd[0] = 0;
    jam_GetCwd(1024, cwd);              /* the child's current-directory handle is not inherited: relative paths fail until it is set again */
    for (i = 0; i < JAM_MAXKIDS && jam_kids[i].h; i++) ;
    r = jam_RtlClone(2 /* inherit handles */, 0, 0, 0, &info);
    if (r == 297 /* STATUS_PROCESS_CLONED: this is the child */) {
        for (i = 0; i < JAM_MAXKIDS; i++) jam_kids[i].h = 0;
        jam_in_child = 1;
        if (cwd[0]) {                    /* (setting the folder it is already in leaves the dead handle: step out and back) */
            static const unsigned short up[] = { 'C', ':', 92 /* backslash */, 0 };
            jam_SetCwd(up);
            jam_SetCwd(cwd);
        }
        return 0;
    }
    if (r != 0 || i >= JAM_MAXKIDS) {
        fputs("JAM: this test needs fork(), which Windows could not give here: skipped\n", stderr);
        _exit(JAM_NO_FORK_EXIT);
    }
    jam_kids[i].pid = (pid_t)(size_t)info.Pid;
    jam_kids[i].h = info.Process;
    jam_CloseH(info.Thread);
    return jam_kids[i].pid;
}
static inline pid_t waitpid(pid_t pid, int *st, int opt)
{
    int i;
    for (;;) {
        int any = 0;
        for (i = 0; i < JAM_MAXKIDS; i++) {
            unsigned long code;
            if (!jam_kids[i].h || (pid > 0 && jam_kids[i].pid != pid)) continue;
            any = 1;
            if (jam_WaitObj(jam_kids[i].h, 0) == 0) {
                pid_t p = jam_kids[i].pid;
                jam_ExitCode(jam_kids[i].h, &code);
                if (st) *st = code >= 0xC0000000ul ? 11 : (int)((code & 0xff) << 8);
                jam_CloseH(jam_kids[i].h); jam_kids[i].h = 0;
                return p;
            }
        }
        if (!any) return -1;
        if (opt & WNOHANG) return 0;
        for (i = 0; i < JAM_MAXKIDS; i++)                       /* block on the wanted child, then loop to reap it */
            if (jam_kids[i].h && (pid <= 0 || jam_kids[i].pid == pid)) { jam_WaitObj(jam_kids[i].h, pid > 0 ? 0xFFFFFFFFul : 20); break; }
    }
}
static inline pid_t wait(int *st) { return waitpid(-1, st, 0); }
#endif
