#ifndef bufferGBAdefs
#define bufferGBAdefs

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();

//filesystem
#include "fsfatlayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "specific_shared.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"
#include "gbaemu4ds_fat_ext.h"

#ifdef __cplusplus 
extern "C" {
#endif

extern volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4]; //genuine reference - wram that is 32bit per chunk
extern volatile u8 __attribute__ ((aligned (1))) palram[0x400];
extern volatile u8 __attribute__ ((aligned (1))) gbabios[0x4000];
extern volatile u8 __attribute__ ((aligned (1))) gbaintram[0x8000];
extern volatile u8 __attribute__ ((aligned (1))) gbaoam[0x400];
extern volatile u8 __attribute__ ((aligned (1))) gbacaioMem[0x400];
extern volatile u8 __attribute__ ((aligned (1))) iomem[0x400];
extern volatile u8 __attribute__ ((aligned (1))) saveram[512*1024];

extern volatile u32 buf_wram[(1024*1024)/4];

//tests
extern u32 tempbuffer[1024*1]; //1K test
extern u32 tempbuffer2[1024*1]; //1K test


extern volatile u32 disk_buf[chucksize];

#ifdef __cplusplus
}
#endif

#endif