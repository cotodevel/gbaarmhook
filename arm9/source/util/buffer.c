#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>
//#include "../common/Types.h"

#include "buffer.h"


volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4];
volatile u8 __attribute__ ((aligned (1))) palram[0x400];
volatile u8 __attribute__ ((aligned (1))) gbabios[0x4000];
volatile u8 __attribute__ ((aligned (1))) gbaintram[0x8000];
volatile u8 __attribute__ ((aligned (1))) gbaoam[0x400];
volatile u8 __attribute__ ((aligned (1))) gbacaioMem[0x400];
volatile u8 __attribute__ ((aligned (1))) iomem[0x400];
volatile u8 __attribute__ ((aligned (1))) saveram[512*1024]; //512K

volatile u32 buf_wram[(1024*1024)/4];

//disk buffer
volatile u32 /*__attribute__((section(".dtcm")))*/ disk_buf[sectorsize]; 

//const u32 __attribute__ ((aligned (4))) gbaheaderbuf[0x200/4]; //reads must be u32, but allocate the correct size (of missing GBAROM which is 0x200 bytes from start)

//tests
u32 tempbuffer[1024*1]; //1K test
u32 tempbuffer2[1024*1]; //1K test
