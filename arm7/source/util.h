#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

//LUT table for fast-seeking 32bit depth unsigned values
extern const  u8 minilut[0x10];

//divswi output
extern u32 divswiarr[0x3];

extern u32 bios_irqhandlerstub_C;

#ifdef __cplusplus
extern "C" {
#endif

//lookup calls
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);

//hardware environment Interruptable Bits for ARM7
//IF
extern u32	 nds_ifmasking;

//IE
extern u32	 nds_iemasking;

//IME
extern u32 	 nds_imemasking;

//CPSR from asm (hardware) cpu NZCV
extern u32 	 cpsrasm;

//bios calls from arm7 firmware
//void swiDelay();
//void swiSleep();
//void swiChangeSoundBias();
extern u32 nds7_div(int x1, int y1);

//irq

//r0    	0=Return immediately if an old flag was already set (NDS9: bugged!)
//			1=Discard old flags, wait until a NEW flag becomes set
//r1 	    Interrupt flag(s) to wait for (same format as IE/IF registers)
extern u32 nds7intrwait(u8 behaviour,u32 if_towaitfor);

extern u32 process_interrupts(u32 if_masks);
extern u32 irqbiosinstall();
extern u32 setirq(u32 irqtoset);

#ifdef __cplusplus
}
#endif