#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include "util.h"
#include "opcode.h"

//hardware environment Interruptable Bits for ARM7
//IF
u32  nds_ifmasking;

//IE
u32  nds_iemasking;

//IME
u32  nds_imemasking;

// returns unary(decimal) ammount of bits using the Hamming Weight approach 
//8 bit depth Lookuptable 
//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
const u8 minilut[0x10] = {
	0,	1,	1,	2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,		//0n
};

u8 lutu16bitcnt(u16 x){
	return (minilut[x &0xf] + minilut[(x>>4) &0xf] + minilut[(x>>8) &0xf] + minilut[(x>>12) &0xf]);
}

u8 lutu32bitcnt(u32 x){
	return (lutu16bitcnt(x & 0xffff) + lutu16bitcnt(x >> 16));
}

//output:
	//DIV
	//MOD
	//ABS
//array for div/divarm swi opcodes (on cpu_opc.s)
//u32 divswiarr[0x3];

//IO processing with callbacks
u32 process_interrupts(u32 nds_ifmasking){

//IME is cleared before this 

//process callbacks (IEregister & IFregister)
switch(*(u32*)(0x04000210) & nds_ifmasking){
	case(1<<0): //LCD V-BLANK				
		//video_render((u32*)(u8*)gba.vidram);
		//nds_ifmasking=stru32inlasm(0x04000214,0x0, 1<<0);
		sendwordipc(0xe);
	break;
	
	case(1<<1):	//LCD H-BLANK
		//sound_render();
		sendwordipc(0xf);
	break;
	
	case(1<<2):	//LCD VCOUNTER MATCH
		//cpu_refreshvcount();
	break;
	
	case(1<<3):	//LCD TIMER0 overflow
	
	break;
	
	case(1<<4):	//LCD TIMER1 overflow
	
	break;
	
	case(1<<5):	//LCD TIMER2 overflow
	
	break;
	
	case(1<<6):	//LCD TIMER3 overflow
	
	break;
	
	case(1<<7):	//NDS7 only: SIO/RCNT/RTC (Real Time Clock)
	
	break;
	
	case(1<<8):	//DMA	0
	
	break;
	
	case(1<<9):	//DMA	1
	
	break;
	
	case(1<<10)://DMA	2
	
	break;
	
	case(1<<11)://DMA	3
	
	break;
	
	case(1<<12)://KEYPAD
	
	break;
	
	case(1<<13)://GBA External IRQ
	
	break;
	
	/*
	case(1<<14)://unused
	
	break;
	
	case(1<<15)://unused
	
	break;
	*/
	case(1<<16)://IPC SYNC
	
	break;
	
	case(1<<17)://IPC Send FIFO Empty
	
	break;
	
	case(1<<18)://IPC Recv FIFO Not Empty
	
	break;
	
	case(1<<19):// NDS-Slot Game Card Data Transfer Completion
	
	break;
	
	case(1<<20):// NDS-Slot Game Card IREQ_MC (onboard bus irq)
	
	break;
	
	/* 	case(1<<21)://unused
	
		break;	*/

	case(1<<22)://NDS7 only: Screens unfolding
	
	break;
	
	case(1<<23)://NDS7 only: SPI bus
	
	break;
	
	case(1<<24)://NDS7 only: Wifi
	
	break;
}

return nds_ifmasking;
}

