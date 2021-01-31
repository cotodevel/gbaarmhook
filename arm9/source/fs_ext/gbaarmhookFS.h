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

extern u8 stream_readu8(u32 pos, FILE * curFileHandle);
extern u16 stream_readu16(u32 pos, FILE * curFileHandle);
extern u32 stream_readu32(u32 pos, FILE * curFileHandle);
extern FILE * globalfileHandle;
extern FILE * opengbarom(const char * filename, char * fopenArg);
extern u32 closegbarom(FILE * curFileHandle);
extern u32 readu32gbarom(u32 offset, FILE * curFileHandle);
extern u16 readu16gbarom(u32 offset, FILE * curFileHandle);
extern u8 readu8gbarom(u32 offset, FILE * curFileHandle);

extern int writeu16gbarom(int offset, u16 * buf_in, int size_elem, FILE * curFileHandle);
extern int writeu32gbarom(int offset, u32 * buf_in, int size_elem, FILE * curFileHandle);
extern int getfilesizegbarom(FILE * curFileHandle);

#ifdef __cplusplus
}
#endif


