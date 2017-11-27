#define VERSION "SVN"

/* Define to 1 to enable internal debugger, requires libcurses */
//#define C_DEBUG 0

#define DISABLE_JOYSTICK

/* Define to 1 to enable output=ddraw */
//#define C_DDRAW 1 

/* Define to 1 to enable screenshots, requires libpng */
//#define C_SSHOT 1

/* Define to 1 to use opengl display output support */
//#define C_OPENGL 1

/* Define to 1 to enable internal modem support, requires SDL_net */
//#define C_MODEM 1

/* Define to 1 to enable IPX networking support, requires SDL_net */
//#define C_IPX 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

#define DB_HAVE_CLOCK_GETTIME

#define C_TARGETCPU ARMV7LE

/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
#define C_DYNREC 1

/* Enable memory function inlining in */
#define C_CORE_INLINE 0

/* Define to 1 to use a unaligned memory access */
//#define C_UNALIGNED_MEMORY 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

/* Define to 1 if you want serial passthrough support. */
//#define C_DIRECTSERIAL 1

#define C_HAS_ATTRIBUTE 1

/* Determines if the compilers supports __builtin_expect for branch
prediction. */
#define C_HAS_BUILTIN_EXPECT 1

#define C_HAVE_MPROTECT 1

#define C_ATTRIBUTE_FASTCALL 1

#define C_ATTRIBUTE_ALWAYS_INLINE 0

#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif

#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif

#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif


#ifndef ANDROID
#define ANDROID
#endif

typedef         double		Real64;
/* The internal types */
typedef  unsigned char		Bit8u;
typedef    signed char		Bit8s;
typedef unsigned short		Bit16u;
typedef   signed short		Bit16s;
typedef  unsigned long		Bit32u;
typedef    signed long		Bit32s;
typedef unsigned long long int Bit64u;
typedef signed long long int Bit64s;
typedef unsigned int		Bitu;
typedef signed int			Bits;

