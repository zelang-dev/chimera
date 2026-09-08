/*
 * endian.h
 */

#ifndef __ENDIAN_H__
#define __ENDIAN_H__ 1
#include <endian.h>
/*
 * Endianism determination by Erik Corry. Please email changes/additions!!!
 */

#if !defined(__MIPSEL__) && (defined(MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || defined(__mipsel) || defined(__mipsel__))
#define __MIPSEL__ 1
#endif

#if !defined(__MIPSEB__) && (defined(MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__) || defined(__mipseb) || defined(__mipseb__) || defined(_MIPSEB))
#define __MIPSEB__ 1
#endif

#if !defined(__SPARC__) && (defined(SPARC) || defined(__SPARC) || defined(__SPARC__) || defined(__sparc) || defined(__sparc__))
#define __SPARC__ 1
#endif

#if !defined(__alpha__) && (defined(ALPHA) || defined(__ALPHA) || defined(__ALPHA__) || defined(__alpha))
#define __alpha__ 1
#endif

#if !defined(__680x0__) && (defined(__680x0) || defined(__680x0__) || defined(__mc68000__))
#define __680x0__ 1
#endif

#if !defined(__AIX__) && (defined(AIX) || defined(_AIX) || defined(__AIX) || defined(__AIX__))
#define __AIX__ 1
#endif

#if !defined(__RS6000__) && (defined(__AIX__) || defined(RS6000) || defined(_RS6000) || defined(__RS6000) || defined(__RS6000__))
#define __RS6000__ 1
#endif

#if !defined(__HPUX__) && (defined(HPUX) || defined(_HPUX) || defined(__HPUX) || defined(__HPUX__))
#define __HPUX__ 1
#endif
#if !defined(__HPUX__) && (defined(hpux) || defined(_hpux) || defined(__hpux) || defined(__hpux__))
#define __HPUX__ 1
#endif

#if !defined(__VAX__) && (defined(VAX) || defined (__VAX))
#define __VAX__ 1
#endif

#if defined(__i386__) || defined(__VAX__) || defined(__MIPSEL__) || defined(__alpha__) || defined(__QNX__)
#undef  CHIMERA_BIG_ENDIAN
#define CHIMERA_LITTLE_ENDIAN
#endif

#if defined(__RS6000__) || defined(__SPARC__) || defined(__680x0__) || defined(__HPUX__) || defined(__MIPSEB__) || defined(__convex__)
#undef  CHIMERA_LITTLE_ENDIAN
#define CHIMERA_BIG_ENDIAN
#endif

#if !defined(CHIMERA_LITTLE_ENDIAN) && !defined(CHIMERA_BIG_ENDIAN)
/*
 * If you hit this and you don't know what symbols your system defines,
 * look in the compiler manual. Try to find a symbol that identifies the
 * processor, rather than the OS or compiler. If you have gcc on a Unix
 * system, the following will tell you what symbols it defines:
 *
 * ln -s /dev/null null.c
 * gcc -ansi -dM -E null.c
 *
 * If you have a system that does not have a clear integer endianism you
 * are going to have severe portability problems.
 *
 */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define CHIMERA_LITTLE_ENDIAN
#else
#define CHIMERA_BIG_ENDIAN
#endif
#endif

#ifdef __alpha__
#define SIXTYFOUR_BIT
#endif

#endif /* __ENDIAN_H__ */
