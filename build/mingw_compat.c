/*
 * mingw_compat.c — MinGW → MSVC/UCRT symbol shims for Odin static linking
 *
 * lld-link (Odin's bundled linker) resolves symbols against MSVC/UCRT import
 * libs.  The Situation static archive was compiled with MinGW GCC, which
 * references a handful of symbols that exist in MSVC CRT under different names
 * or are absent entirely.  This shim is compiled with GCC and archived into
 * mingw_compat.lib, then passed to Odin's -extra-linker-flags so lld-link
 * finds them.
 *
 * Compile:
 *   gcc -c build/mingw_compat.c -o build/mingw_compat.o -O2
 *   ar rcs build/dll/mingw_compat.lib build/mingw_compat.o
 *
 * Symbols provided:
 *   sincosf / sincos          — absent from MSVC CRT, computed via sinf/cosf
 *   strdup                    — POSIX name; MSVC has _strdup
 *   write                     — POSIX name; MSVC has _write
 */

#include <math.h>
#include <string.h>
#include <io.h>    /* _write */
#include <stdlib.h> /* _strdup */

/* sincosf: compute sin and cos in one call.
   MSVC CRT lacks this; MinGW provides it natively. */
void sincosf(float x, float *s, float *c) {
    *s = sinf(x);
    *c = cosf(x);
}

/* sincos: double-precision variant */
void sincos(double x, double *s, double *c) {
    *s = sin(x);
    *c = cos(x);
}

/* strdup: POSIX name; MSVC only exposes _strdup */
char *strdup(const char *s) {
    return _strdup(s);
}

/* write: POSIX name; MSVC only exposes _write */
int write(int fd, const void *buf, unsigned int count) {
    return _write(fd, buf, count);
}
