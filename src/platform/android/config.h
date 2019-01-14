#define VERSION "SVN"

//OS specific

#ifndef ANDROID
#define ANDROID
#endif

#define DB_HAVE_CLOCK_GETTIME
#define C_HAVE_MPROTECT 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

//#define DISABLE_JOYSTICK

//compiler

#define C_ATTRIBUTE_ALWAYS_INLINE 0

#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif

#define DB_FASTCALL __attribute__((fastcall))
#define GCC_ATTRIBUTE(x) __attribute__ ((x))

/* Determines if the compilers supports __builtin_expect for branch
prediction. */
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)

//Hardware

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

#if defined(__arm__) || defined(__aarch64__)
	#define C_DYNREC 1
	#define C_UNALIGNED_MEMORY 1
	#define C_CORE_INLINE 0

	#ifdef __aarch64__		
		#define C_TARGETCPU ARMV8LE
	#else
		#define C_TARGETCPU ARMV7LE
	#endif
#endif

#include <stdint.h>

typedef double		Real64;
/* The internal types */
typedef uint8_t		Bit8u;
typedef int8_t		Bit8s;
typedef uint16_t	Bit16u;
typedef int16_t		Bit16s;
typedef uint32_t	Bit32u;
typedef int32_t		Bit32s;
typedef uint64_t	Bit64u;
typedef int64_t		Bit64s;
typedef uintptr_t	Bitu;
typedef intptr_t	Bits;

