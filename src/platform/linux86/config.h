#define VERSION "SVN"

#include <stdint.h>

#define LINUX 1

#define NOMINMAX

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable output=ddraw */
//#define C_DDRAW 1

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to use opengl display output support */
//#define C_OPENGL 1

/* Define to 1 to enable internal modem support, requires SDL_net */
//#define C_MODEM 1

/* Define to 1 to enable IPX networking support, requires SDL_net */
//#define C_IPX 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

/* The type of cpu this host has */
#define C_TARGETCPU X86
//#define C_TARGETCPU X86_64

/* Define to 1 to use x86 dynamic cpu core */
#define C_DYNAMIC_X86 1

/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
#define C_DYNREC 0

/* Enable memory function inlining in */
#define C_CORE_INLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* Define to 1 to use a x86 assembly fpu core */
#define C_FPU_X86 1

/* Define to 1 to use a unaligned memory access */
#define C_UNALIGNED_MEMORY 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

#define C_HAS_BUILTIN_EXPECT 1

#define C_HAVE_MPROTECT 1

#define C_ATTRIBUTE_FASTCALL 1

#define INLINE inline __attribute__((always_inline))

#define DB_FASTCALL __attribute__((fastcall))

#define GCC_ATTRIBUTE(x) __attribute__ ((x))

#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)

typedef double		Real64;
/* The internal types */
typedef uint8_t		    Bit8u;
typedef int8_t		    Bit8s;
typedef uint16_t		Bit16u;
typedef int16_t		    Bit16s;
typedef uint32_t	    Bit32u;
typedef int32_t	        Bit32s;
typedef uint64_t	    Bit64u;
typedef int64_t	        Bit64s;
typedef uintptr_t	    Bitu;
typedef intptr_t		Bits;

/* Define to 1 to use ALSA for MIDI */
#define HAVE_ALSA 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `asound' library (-lasound). */
#define HAVE_LIBASOUND 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1
