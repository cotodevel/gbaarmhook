#ifndef biosGBAdefs
#define biosGBAdefs

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();
#include <string.h>

extern s16 sinetable[256];

#ifdef __cplusplus
extern "C"{
#endif

u32 bios_cpureset();						//swi 0
u32 bios_registerramreset(u32); 			//swi 1
u32 bios_cpuhalt();							//swi 2
u32 bios_stopsleep();						//swi 3
u32 bios_div();								//swi 6
u32 bios_divarm();							//swi 7
u32 bios_sqrt();							//swi 8
u32 bios_arctan();							//swi 9
u32 bios_arctan2();							//swi 0xa
u32 bios_cpuset();							//swi 0xb
u32 bios_cpufastset();						//swi 0xc
u32 bios_getbioschecksum();					//swi 0xd
u32 bios_bgaffineset();						//swi 0xe
u32 bios_objaffineset();					//swi 0xf
u32 bios_bitunpack();						//swi 0x10
u32 bios_lz77uncompwram();					//swi 0x11
u32 bios_lz77uncompvram();					//swi 0x12
u32 bios_huffuncomp();						//swi 0x13
u32 bios_rluncompwram();					//swi 0x14
u32 bios_rluncompvram();					//swi 0x15
u32 bios_diff8bitunfilterwram();			//swi 0x16
u32 bios_diff8bitunfiltervram();			//swi 0x17
u32 bios_diff16bitunfilter();				//swi 0x18
u32 bios_midikey2freq();					//swi 0x1f
u32 bios_snddriverjmptablecopy();			//swi 0x2a

#ifdef __cplusplus
}
#endif

#endif