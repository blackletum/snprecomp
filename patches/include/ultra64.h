#ifndef SNP_PATCHES_ULTRA64_H
#define SNP_PATCHES_ULTRA64_H

// The few N64 types and libultra functions the MIPS patch code uses. Written
// for this project in place of the original SDK headers: only what
// patches/*.c needs, with libultra's names and calling conventions.

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed long s32;
typedef unsigned long u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#ifndef NULL
#define NULL 0
#endif

#define FALSE 0
#define TRUE 1

// Message queues (provided by the runtime; see patches.h's renames).
typedef void* OSMesg;
typedef struct OSMesgQueue OSMesgQueue;
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1
s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flag);

// OSTask types.
#define M_GFXTASK 1
#define M_AUDTASK 2

// Video interface (provided by the runtime; see patches.h's renames).
void osViBlack(u8 active);
void osViSwapBuffer(void* frameBufPtr);

#endif // SNP_PATCHES_ULTRA64_H
