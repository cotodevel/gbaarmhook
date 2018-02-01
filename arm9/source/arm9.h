//arm9 main libs
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
//#include <dswifi9.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>

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

#ifdef __cplusplus
}
#endif