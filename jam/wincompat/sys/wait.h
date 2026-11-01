/* JAM: Windows (llvm-mingw) stand-in for <sys/wait.h>. Windows has no fork(). A test that needs a child process
 * stops at its first fork() with exit code 77 ("skipped: needs fork") instead of hanging on a pipe no child will
 * ever write. Everything that runs in one process is unaffected. */
#ifndef JAM_WINCOMPAT_SYS_WAIT_H
#define JAM_WINCOMPAT_SYS_WAIT_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define JAM_NO_FORK_EXIT 77
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) (s)
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
#define WNOHANG 1

static inline pid_t fork(void)
{
    fflush(stdout);
    fputs("JAM: this test needs fork(), which Windows does not have: skipped\n", stderr);
    exit(JAM_NO_FORK_EXIT);
}
static inline pid_t waitpid(pid_t pid, int *st, int opt) { (void)pid; (void)opt; if (st) *st = 1; return -1; }
static inline pid_t wait(int *st) { return waitpid(-1, st, 0); }
#endif
