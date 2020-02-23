#ifndef __crc32_h__
#define __crc32_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "fatfslayerTGDS.h"

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int Crc32_ComputeFile( FILE *file, unsigned long *outCrc32 );
extern unsigned long Crc32_ComputeBuf( unsigned long inCrc32, const void *buf,size_t bufLen );
extern int Crc32_ComputeFile( FILE *file, unsigned long *outCrc32 );

#ifdef __cplusplus
}
#endif
