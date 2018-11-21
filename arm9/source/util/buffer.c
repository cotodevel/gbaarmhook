#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include "gbaemu4ds_fat_ext.h"
#include "buffer.h"


volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4];
volatile u8 __attribute__ ((aligned (1))) palram[0x400];
volatile u8 __attribute__ ((aligned (1))) gbabios[0x4000];
volatile u8 __attribute__ ((aligned (1))) gbaintram[0x8000];
volatile u8 __attribute__ ((aligned (1))) gbaoam[0x400];
volatile u8 __attribute__ ((aligned (1))) gbacaioMem[0x400];
volatile u8 __attribute__ ((aligned (1))) iomem[0x400];
volatile u8 __attribute__ ((aligned (1))) saveram[128*1024]; //128K

//disk buffer
volatile u32 disk_buf[chucksize]; 

//tests
u32 tempbuffer[1024*1]; //1K test
u32 tempbuffer2[1024*1]; //1K test
