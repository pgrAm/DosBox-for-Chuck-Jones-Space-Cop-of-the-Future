#ifndef WIN32
	#define WIN32    1
#endif

#define VERSION "SVN"

#define NOMINMAX

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

#define C_DIRECTSERIAL 1

/* The type of cpu this host has */
#if defined(_WIN64)
	#define C_TARGETCPU X86_64

	/* Define to 1 to use x86 dynamic cpu core */
	#define C_DYNAMIC_X86 0

	/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
	#define C_DYNREC 1
#else
	#define C_TARGETCPU X86

	/* Define to 1 to use a x86 assembly fpu core */
	#define C_FPU_X86 1

	/* Define to 1 to use x86 dynamic cpu core */
	#define C_DYNAMIC_X86 1

	/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
	#define C_DYNREC 0
#endif

/* Enable memory function inlining in */
#define C_CORE_INLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* Define to 1 to use a unaligned memory access */
#define C_UNALIGNED_MEMORY 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

/* Define to 1 if you want serial passthrough support. */
//#define C_DIRECTSERIAL 1

#ifdef __clang__
	#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

#define GCC_ATTRIBUTE(x) /* attribute not supported */
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)

#define INLINE __forceinline
#define DB_FASTCALL __fastcall

#if defined(_MSC_VER) && (_MSC_VER >= 1400) 
#pragma warning(disable : 4996) 
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