//memory buffers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/system.h>

#include "..\disk\stream_disk.h"

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

//disk buffer
extern volatile u32 /*__attribute__((section(".dtcm")))*/ disk_buf[sectorsize];

//tests
extern u32 tempbuffer[1024*1]; //1K test
extern u32 tempbuffer2[1024*1]; //1K test

#ifdef __cplusplus
}
#endif