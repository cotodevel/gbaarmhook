#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

//linker (bin) objects
extern unsigned int rom_pl_bin;
extern unsigned int rom_pl_bin_size;
extern unsigned int __ewram_end;

//stack info from ld
//__sp_svc	=	__dtcm_top - 0x08 - 0x400;
//__sp_irq	=	__sp_svc - 0x400;
//__sp_usr	=	__sp_irq - 0x400;
//__sp_abort	=	__sp_usr - 0x400;

extern u32 SPswi __attribute__((section(".vectors")));
extern u32 SPirq __attribute__((section(".vectors")));
extern u32 SPusr __attribute__((section(".vectors")));
extern u32 SPdabt __attribute__((section(".vectors")));

//DTCM TOP full memory
extern u32 dtcm_top_ld __attribute__((section(".vectors")));
//DTCM TOP reserved by compiler/user memory
extern u32 dtcm_end_alloced __attribute__((section(".dtcm")));


typedef int (*intfuncptr)();
typedef u32 (*u32funcptr)();
typedef void (*voidfuncptr)();

extern u32 cpucore_tick;

//patches for ARM code
extern u32 PATCH_BOOTCODE();
extern u32 PATCH_START();
extern u32 PATCH_HOOK_START();
extern u32 NDS7_RTC_PROCESS();
extern u32 PATCH_ENTRYPOINT[4];

//r0: new_instruction / r1: cpsr
//arg1 is cpsr <new mode> inter from branch calls, execute then send curr_cpsr back
extern u32 emulatorgba();	
extern char temppath[255 * 2];
extern char biospath[255 * 2];
extern char savepath[255 * 2];
extern char patchpath[255 * 2];

#ifdef __cplusplus
}
#endif