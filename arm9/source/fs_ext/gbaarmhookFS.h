#ifndef __armv4coreFS_h__
#define __armv4coreFS_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "fatfslayerTGDS.h"

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern u8 stream_readu8(u32 pos);
extern u16 stream_readu16(u32 pos);
extern u32 stream_readu32(u32 pos);
extern FILE * globalfileHandle;
extern int globalfileSize;
extern FILE * opengbarom(const char * filename, char * fopenArg);
extern u32 closegbarom();
extern u32 readu32gbarom(u32 offset);
extern u16 readu16gbarom(u32 offset);
extern u8 readu8gbarom(u32 offset);

extern u16 writeu16gbarom(int offset,u16 * buf_in,int size_elem);
extern u32 writeu32gbarom(int offset,u32 * buf_in,int size_elem);
extern int getfilesizegbarom();

#ifdef __cplusplus
}
#endif


