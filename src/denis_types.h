#ifndef DENIS_TYPES_H_
#define DENIS_TYPES_H_

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

#ifndef __cplusplus
typedef enum { FALSE, TRUE } bool;
#else

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#endif

#if defined(DEBUG)
#define ASSERT(x)				\
	do							\
	{							\
	  if (!(x))					\
		  *(s8*)0 = 0;		\
	} while(false)

#else
#define ASSERT(x)
#endif

#define KILOBYTE(num) ((num) * (u64)1024)
#define MEGABYTE(num) (KILOBYTE(num) * (u64)1024)
#define GIGABYTE(num) (MEGABYTE(num) * (u64)1024)

#endif
