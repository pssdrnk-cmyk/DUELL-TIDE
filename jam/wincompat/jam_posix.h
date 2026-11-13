/* SPDX-License-Identifier: GPL-3.0-only */
/* JAM: force-included by work/shims/cc on Windows (llvm-mingw). The few POSIX calls the host tests use that
 * MinGW spells differently, and POSIX text I/O: files and stdout in binary mode, so "\n" stays "\n" (generated files and
 * golden lists compare byte for byte with the ones made on Linux and macOS). Nothing here changes what the firmware
 * sources compute. */
#ifndef JAM_POSIX_H
#define JAM_POSIX_H
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>

#define pipe(fd) _pipe((fd), 1 << 16, _O_BINARY)

/* alarm(): a test's own watchdog (hang_test.c). Windows has none: it never fires here, jam/check.sh's time limit stands in */
#include <signal.h>
#ifndef SIGALRM
#define SIGALRM 14
#endif
static inline unsigned jam_alarm(unsigned s) { (void)s; return 0u; }
#define alarm(s) jam_alarm(s)

__attribute__((constructor)) static void jam_binary_io(void)
{
    _fmode = _O_BINARY;
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
}
#endif
