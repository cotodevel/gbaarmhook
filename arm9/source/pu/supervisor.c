//CPU opcodes (that do not rely on translator part, but GBA opcodes) here!

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include "supervisor.h"

#include "pu.h"
#include "opcode.h"
#include "util.h"
#include "buffer.h"
#include "translator.h"
#include "bios.h"

//filesystem
#include "fsfatlayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "ipcfifoTGDSUser.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

#include "settings.h"
#include "keypadTGDS.h"
#include "timerTGDS.h"
#include "dmaTGDS.h"

extern struct GBASystem gba;

//fifo
extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

//Stack for GBA
u8 __attribute__((section(".dtcm"))) gbastck_usr[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_fiq[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_irq[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_svc[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_abt[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_und[0x200];
//u8 __attribute__((section(".dtcm"))) gbastck_sys[0x200]; //stack shared with usr

//Stack Pointer for GBA
u32  __attribute__((section(".dtcm"))) * gbastackptr;

//current CPU mode working set of registers
u32  __attribute__((section(".dtcm"))) gbavirtreg_cpu[0x10]; //placeholder for actual CPU mode registers

//r13, r14 for each mode (sp and lr)
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13usr[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14usr[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13fiq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14fiq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13irq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14irq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13svc[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14svc[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14abt[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13abt[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14und[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13und[0x1];
//u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1]; //stack shared with usr
//u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1];

//and FIQ(32) which is r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_fiq[0x5];

//and cpu<mode> all the other backup registers when restoring from FIQ r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_cpubup[0x5];

//////////////////////////////////////////////opcodes that must be virtualized, go here.

//bx and normal branches use a branching stack
//40 u32 slots for branchcalls
u32  __attribute__((section(".dtcm"))) branch_stack[17*32]; //32 slots for branch calls

//NDS Interrupts
//irq to be set
u32 setirq(u32 irqtoset){
	//enable VBLANK IRQ
	if(irqtoset == (1<<0)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<3));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<3);
		//printf("set vb irq");
	}
	//enable HBLANK IRQ
	else if (irqtoset == (1<<1)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<4));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<4);
		//printf("set hb irq");
	}
	//enable VCOUNTER IRQ
	else if (irqtoset == (1<<2)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<5));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<5);
		//printf("set vcnt irq");
	}
	
return 0;
}


//IO opcodes
u32 __attribute__ ((hot)) ndscpu_refreshvcount(){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif


return 0;
}


u32 __attribute__ ((hot)) gbacpu_refreshvcount(){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif
			
	if( (gba.GBAVCOUNT) == (gba.GBADISPSTAT >> 8) ) { //V-Count Setting (LYC) == VCOUNT = IRQ VBLANK
		gba.GBADISPSTAT |= 4;
		UPDATE_REG(0x04, gba.GBADISPSTAT);		//UPDATE_REG(0x04, gba->DISPSTAT);

		if(gba.GBADISPSTAT & 0x20) {
			gbavirt_ifmasking |= 4;
			UPDATE_REG(0x202, gbavirt_ifmasking);		 //UPDATE_REG(0x202, gba->IF);
		}
	} 
	else {
		gba.GBADISPSTAT &= 0xFFFB;
		UPDATE_REG(0x4, gba.GBADISPSTAT); 		//UPDATE_REG(0x4, gba->DISPSTAT);
	}
	if((gba.layerenabledelay) >0){
		gba.layerenabledelay--;
			if (gba.layerenabledelay==1)
				gba.layerenable = gba.layersettings & gba.GBADISPCNT;
	}
return 0;
}

//debug!
u32 debuggeroutput(){
u32 lnk_ptr;
		__asm__ volatile (
		"mov %[lnk_ptr],#0;" "\n\t"
		"add %[lnk_ptr],%[lnk_ptr], lr" "\n\t"//"sub lr, lr, #8;" "\n\t"
		"sub %[lnk_ptr],%[lnk_ptr],#0x8" 
		:[lnk_ptr] "=r" (lnk_ptr)
		);
printf("\n LR callback trace at %x \n", (unsigned int)lnk_ptr);
return 0;
}

//src_address / addrpatches[] (blacklisted) / addrfixes[] (patched ret)
const u32 __attribute__ ((hot)) addresslookup(u32 srcaddr, u32 * blacklist, u32 * whitelist){

//this must go in pairs otherwise nds ewram ranges might wrongly match gba addresses
int i=0;
for (i=0;i<16;i+=2){

	if( (srcaddr>=(u32)(*(blacklist+((u32)i)))) &&  (srcaddr<=(u32)(*(blacklist+((u32)i+1)))) )
	{
		srcaddr=(u32)( (*(whitelist+(i))));
		break;
		//srcaddr=i; //debug
		//return i; //correct offset
	}
}
return srcaddr;
}
//GBA CPU opcodes
//cpu interrupt
u32 cpuirq(u32 cpumode){

		//Enter cpu<mode>
		updatecpuflags(1,cpsrvirt,cpumode);
		gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]; //gba->reg[14].I = PC;
		
		if(!armstate)//if(!savedState)
			gbavirtreg_cpu[0xe]+=2; //gba->reg[14].I += 2;
		armstate=0;//true
		gba.armirqenable = false;
		
		//refresh jump opcode in biosprotected vector
		gba.biosprotected[0] = 0x02;
		gba.biosprotected[1] = 0xc0;
		gba.biosprotected[2] = 0x5e;
		gba.biosprotected[3] = 0xe5;

		//set PC
		gbavirtreg_cpu[0xf]=0x18;
		
		//swi_virt(swinum);
		
		//Restore CPU<mode>
		updatecpuflags(1,cpsrvirt,spsr_last&0x1F);

return 0;
}

//fetch current address
u32 armfetchpc(u32 address){

	if(armstate==0)
		address=cpuread_word(address);
	else
		address=cpuread_hword(address);
		
return address;
}


//prefetch next opcode
u32 armnextpc(u32 address){
	
	if(armstate==0)
		address=cpuread_word(address+0x4);
	else
		address=cpuread_hword(address+0x4);
	
return address;
}

//readjoypad
u32 systemreadjoypad(int which){
	//scanKeys();
	u32 res = keysPressed(); //getkeys
		// disallow L+R or U+D of being pressed at the same time
		if((res & 48) == 48)
		res &= ~16;
		if((res & 192) == 192)
		res &= ~128;
	return res;
}
//coto
//cpuupdateticks()
int __attribute((hot)) cpuupdateticks(){

	int cpuloopticks = gba.lcdticks;
	if(gba.soundticks < cpuloopticks)
		cpuloopticks = gba.soundticks;

	else if(gba.timer0on && (gba.timer0ticks < cpuloopticks)) {
		cpuloopticks = gba.timer0ticks;
	}
	else if(gba.timer1on && !(gba.GBATM1CNT & 4) && (gba.timer1ticks < cpuloopticks)) {
		cpuloopticks = gba.timer1ticks;
	}
	else if(gba.timer2on && !(gba.GBATM2CNT & 4) && (gba.timer2ticks < cpuloopticks)) {
		cpuloopticks = gba.timer2ticks;
	}
	else if(gba.timer3on && !(gba.GBATM3CNT & 4) && (gba.timer3ticks < cpuloopticks)) {
		cpuloopticks = gba.timer3ticks;
	}

	else if (gba.switicks) {
		if (gba.switicks < cpuloopticks)
			cpuloopticks = gba.switicks;
	}

	else if (gba.irqticks) {
		if (gba.irqticks < cpuloopticks)
			cpuloopticks = gba.irqticks;
	}

	return cpuloopticks;
}

//CPULoop
u32 cpuloop(int ticks){
	int clockticks=0;
	int timeroverflow=0;
	
	//summary: cpuupdateticks() == refresh thread time per hw slot
	
	// variable used by the CPU core
	gba.cpubreakloop = false;
	gba.cpunextevent = cpuupdateticks(); //CPUUpdateTicks(); (CPU prefetch cycle lapsus)
	if(gba.cpunextevent > ticks)
		gba.cpunextevent = ticks;
	
	//coto
	//for(;;) { //our loop is already HBLANK -> hw controlled loop is nice
		
		//prefetch part
		u32 fetchnextpc=0;
		//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
		switch(armstate){
			//ARM code
			case(0):{
				//#ifdef DEBUGEMU
				//	printf("(ARM)PREFETCH ACCESS!! ***\n");
				//#endif
				fetchnextpc=armnextpc(rom+4); 
				//#ifdef DEBUGEMU
				//	printf("(ARM)PREFETCH ACCESS END !!*** \n");
				//#endif
				
			}
			break;	
			//thumb code
			case(1):{
				//#ifdef DEBUGEMU
				//	printf("(THMB)PREFETCH ACCESS!! ***\n");
				//#endif
				fetchnextpc=armnextpc(rom+2);
				//#ifdef DEBUGEMU
				//	printf("(THMB)PREFETCH ACCESS END !!*** \n");
				//#endif	
			}
			break;
		}
		//idles in this state because swi idle and CPU enabled
		if(!gba.holdstate && !gba.switicks) {
			if ((/*armNextPC*/ fetchnextpc & 0x0803FFFF) == 0x08020000)
				gba.busprefetchcount=0x100;
		}
		//end prefetch part
		
		
		
		clockticks = cpuupdateticks(); //CPUUpdateTicks(); (CPU cycle lapsus)
		cputotalticks += clockticks; //ori: gba.cputotalticks += clockticks;
	
		//if there is a new event: remaining ticks is the new thread time
		if(cputotalticks >= gba.cpunextevent){	//ori: if(gba.cputotalticks >= gba.cpunextevent){
		
			int remainingticks = cputotalticks - gba.cpunextevent; //ori: int remainingticks = gba.cputotalticks - gba.cpunextevent;

			if (gba.switicks){
				gba.switicks-=clockticks;
				if (gba.switicks<0)
					gba.switicks = 0;
			}
			
			clockticks = gba.cpunextevent;
			
			cputotalticks = 0;	//ori: gba.cputotalticks = 0;
			gba.cpudmahack = false;
			updateLoop:
			if (gba.irqticks){
				gba.irqticks -= clockticks;
				if (gba.irqticks<0)
					gba.irqticks = 0;
			}
			gba.lcdticks -= clockticks;
			if(gba.lcdticks <= 0) {
				if(gba.GBADISPSTAT & 1) { // V-BLANK
					// if in V-Blank mode, keep computing... (v-counter flag match = 1)
					if(gba.GBADISPSTAT & 2) {
						gba.lcdticks += 1008;
							//gba.GBAVCOUNT++; //hardware vcounter
						UPDATE_REG(0x06, gba.GBAVCOUNT);	//UPDATE_REG(0x06, VCOUNT);
						gba.GBADISPSTAT &= 0xFFFD;
						UPDATE_REG(0x04, gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba.GBADISPSTAT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					} 
					else{
						gba.lcdticks += 224;
						gba.GBADISPSTAT |= 2;
						UPDATE_REG(0x04 , gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba.GBADISPSTAT);
						if(gba.GBADISPSTAT & 16) {
							gbavirt_ifmasking |= 2;	//IF |= 2;
							UPDATE_REG(0x202, gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
						}
					}
					if(gba.GBAVCOUNT >= 228){ //Reaching last line
						gba.GBADISPSTAT &= 0xFFFC;
						UPDATE_REG(0x04, gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba.GBADISPSTAT);
						gba.GBAVCOUNT = 0;
						UPDATE_REG(0x06, gba.GBAVCOUNT);	//UPDATE_REG(0x06, VCOUNT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					}
				}
				else{
					if(gba.GBADISPSTAT & 2) {
						// if in H-Blank, leave it and move to drawing mode
							//gba.GBAVCOUNT++; //hardware vcounter
						UPDATE_REG(0x06 , gba.GBAVCOUNT);	//UPDATE_REG(0x06, VCOUNT);
						gba.lcdticks += 1008;
						gba.GBADISPSTAT &= 0xFFFD;
						if(gba.GBAVCOUNT == 160) { //last hblank line
							gba.count++;
							u32 joy = 0;	// update joystick information
							
							joy = systemreadjoypad(-1);
							gba.GBAP1 = 0x03FF ^ (joy & 0x3FF);
							if(gba.cpueepromsensorenabled)
								//systemUpdateMotionSensor(); //coto: not now :p
							UPDATE_REG(0x130 , gba.GBAP1);	//UPDATE_REG(0x130, P1);
							u16 P1CNT =	cpuread_hwordfast(((u32)gba.iomem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
							// this seems wrong, but there are cases where the game
							// can enter the stop state without requesting an IRQ from
							// the joypad.
							if((P1CNT & 0x4000) || gba.stopstate) {
								u16 p1 = (0x3FF ^ gba.GBAP1) & 0x3FF;
								if(P1CNT & 0x8000) {
									if(p1 == (P1CNT & 0x3FF)) {
										gbavirt_ifmasking |= 0x1000;
										UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
									}
								} 
								else {
									if(p1 & P1CNT) {
										gbavirt_ifmasking |= 0x1000;
										UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
									}
								}
							}
							
							gba.GBADISPSTAT |= 1; 		//V-BLANK flag set (period 160..226)
							gba.GBADISPSTAT &= 0xFFFD;
							UPDATE_REG(0x04 , gba.GBADISPSTAT);		//UPDATE_REG(0x04, gba.GBADISPSTAT);
							if(gba.GBADISPSTAT & 0x0008) {
								gbavirt_ifmasking |= 1;
								UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
							}
							
							//CPUCheckDMA(1, 0x0f); //add later
							
							//framenummer++; //add later
						}
						UPDATE_REG(0x04 , gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba.GBADISPSTAT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					} 
					else{
						// entering H-Blank
						gba.GBADISPSTAT |= 2;
						UPDATE_REG(0x04 , gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba.GBADISPSTAT);
						gba.lcdticks += 224;
							//CPUCheckDMA(2, 0x0f); //add later
						if(gba.GBADISPSTAT & 16) {
							gbavirt_ifmasking |= 2;
							UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
						}
					}
				}
			}//end if there's new delta lcd ticks to process..!

			if(!gba.stopstate) {
				if(gba.timer0on) {
					gba.timer0ticks -= clockticks;
					if(gba.timer0ticks <= 0) {
						gba.timer0ticks += (0x10000 - gba.timer0reload) << gba.timer0clockreload;
						timeroverflow |= 1;
						//soundTimerOverflow(0);
						if(gba.GBATM0CNT & 0x40) {
							gbavirt_ifmasking |= 0x08;
							UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
						}
					}
					gba.GBATM0D = 0xFFFF - (gba.timer0ticks >> gba.timer0clockreload);
					UPDATE_REG(0x100 , gba.GBATM0D);	//UPDATE_REG(0x100, TM0D); 
				}
				if(gba.timer1on) {
					if(gba.GBATM1CNT & 4) {
						if(timeroverflow & 1) {
							gba.GBATM1D++;
							if(gba.GBATM1D == 0) {
								gba.GBATM1D += gba.timer1reload;
								timeroverflow |= 2;
								//soundTimerOverflow(1);
								if(gba.GBATM1CNT & 0x40) {
									gbavirt_ifmasking |= 0x10;
									UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
								}
							}
							UPDATE_REG(0x104 , gba.GBATM1D);	//UPDATE_REG(0x104, TM1D);
						}
					} 
					else{
						gba.timer1ticks -= clockticks;
						if(gba.timer1ticks <= 0) {
							gba.timer1ticks += (0x10000 - gba.timer1reload) << gba.timer1clockreload;
							timeroverflow |= 2; 
							//soundTimerOverflow(1);
							if(gba.GBATM1CNT & 0x40) {
								gbavirt_ifmasking |= 0x10;
								UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
							}
						}
						gba.GBATM1D = 0xFFFF - (gba.timer1ticks >> gba.timer1clockreload);
						UPDATE_REG(0x104 , gba.GBATM1D);	//UPDATE_REG(0x104, TM1D); 
					}
				}
				if(gba.timer2on) {
					if(gba.GBATM2CNT & 4) {
						if(timeroverflow & 2){
							gba.GBATM2D++;
							if(gba.GBATM2D == 0) {
								gba.GBATM2D += gba.timer2reload;
								timeroverflow |= 4;
								if(gba.GBATM2CNT & 0x40) {
									gbavirt_ifmasking |= 0x20;
									UPDATE_REG(0x202 , gbavirt_ifmasking);// UPDATE_REG(0x202, IF);
								}
							}
							UPDATE_REG(0x108 , gba.GBATM2D);// UPDATE_REG(0x108, TM2D);
						}
					}
					else{
						gba.timer2ticks -= clockticks;
						if(gba.timer2ticks <= 0) {
							gba.timer2ticks += (0x10000 - gba.timer2reload) << gba.timer2clockreload;
							timeroverflow |= 4; 
							if(gba.GBATM2CNT & 0x40) {
								gbavirt_ifmasking |= 0x20;
								UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
							}
						}
						gba.GBATM2D = 0xFFFF - (gba.timer2ticks >> gba.timer2clockreload);
						UPDATE_REG(0x108 , gba.GBATM2D);	//UPDATE_REG(0x108, TM2D); 
					}
				}
				if(gba.timer3on) {
					if(gba.GBATM3CNT & 4) {
						if(timeroverflow & 4) {
							gba.GBATM3D++;
							if(gba.GBATM3D == 0) {
								gba.GBATM3D += gba.timer3reload;
								if(gba.GBATM3CNT & 0x40) {
									gbavirt_ifmasking |= 0x40;
									UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
								}
							}
							UPDATE_REG(0x10c , gba.GBATM3D);	//UPDATE_REG(0x10C, TM3D);
						}
					}
					else{
						gba.timer3ticks -= clockticks;
						if(gba.timer3ticks <= 0) {
							gba.timer3ticks += (0x10000 - gba.timer3reload) << gba.timer3clockreload; 
							if(gba.GBATM3CNT & 0x40) {
								gbavirt_ifmasking |= 0x40;
								UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
							}
						}
						gba.GBATM3D = 0xFFFF - (gba.timer3ticks >> gba.timer3clockreload);
						UPDATE_REG(0x10c , gba.GBATM3D);	//UPDATE_REG(0x10C, TM3D); 
					}
				}
			}//end if not idle (CPU toggle StopState)

			timeroverflow = 0;
			
			ticks -= clockticks;
			gba.cpunextevent = cpuupdateticks(); //CPUUpdateTicks();
			if(gba.cpudmatickstoupdate > 0) {
				if(gba.cpudmatickstoupdate > gba.cpunextevent)
					clockticks = gba.cpunextevent;
				else
					clockticks = gba.cpudmatickstoupdate;
				gba.cpudmatickstoupdate -= clockticks;
				if(gba.cpudmatickstoupdate < 0)
					gba.cpudmatickstoupdate = 0;
				gba.cpudmahack = true;
				goto updateLoop;
			}
			
			if(gbavirt_ifmasking && (gbavirt_imemasking & 1) && gba.armirqenable){
				int res = gbavirt_ifmasking & gbavirt_iemasking;
				if(gba.stopstate)
					res &= 0x3080;
				if(res) {
					if (gba.intstate){
						if (!gba.irqticks){
							
							cpuirq(0x12); //CPUInterrupt();
							
							gba.intstate = false;
							gba.holdstate = false;
							gba.stopstate = false;
							gba.holdtype = 0;
						}
					}
					else{
						if (!gba.holdstate){
							gba.intstate = true;
							gba.irqticks=7;
						if (gba.cpunextevent> gba.irqticks)
							gba.cpunextevent = gba.irqticks;
						}
						else{
							cpuirq(0x12); //CPUInterrupt();
							gba.holdstate = false;
							gba.stopstate = false;
							gba.holdtype = 0;
						}
					}
					// Stops the SWI Ticks emulation if an IRQ is executed
					//(to avoid problems with nested IRQ/SWI)
					if (gba.switicks)
						gba.switicks = 0;
				}
			}
			
			if(remainingticks > 0){
				if(remainingticks > gba.cpunextevent)
					clockticks = gba.cpunextevent;
				else
					clockticks = remainingticks;
				remainingticks -= clockticks;
				if(remainingticks < 0)
					remainingticks = 0;
				goto updateLoop;
			}
			if (gba.timeronoffdelay)
				//applyTimer(); //add this 
			if(gba.cpunextevent > ticks)
				gba.cpunextevent = ticks;
			if(ticks <= 0 || gba.cpubreakloop)
				//break;
				return 0;
		}//end if there's a new event
	
	//}//endmain looper //coto

return 0;
}


//hardware < - > virtual environment update CPU stats

//IO address, u32 value
u32 __attribute__ ((hot)) cpu_updateregisters(u32 address, u16 value){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

switch(address){
	case 0x00:{ 
		// we need to place the following code in { } because we declare & initialize variables in a case statement
		if((value & 7) > 5) {
			// display modes above 0-5 are prohibited
			gba.GBADISPCNT = (value & 7);
		}
		bool change = (0 != ((gba.GBADISPCNT ^ value) & 0x80)); //forced vblank? (1 means delta time vram access)
		bool changeBG = (0 != ((gba.GBADISPCNT ^ value) & 0x0F00));
		u16 changeBGon = ((~gba.GBADISPCNT) & value) & 0x0F00; // these layers are being activated
		
		gba.GBADISPCNT = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
		UPDATE_REG(0x00 , gba.GBADISPCNT); //UPDATE_REG(0x00, gba.GBADISPCNT);

		if(changeBGon) {
			gba.layerenabledelay = 4;
			gba.layerenable = gba.layersettings & value & (~changeBGon);
		}
		else {
			gba.layerenable = gba.layersettings & value;
			cpuupdateticks();	// CPUUpdateTicks();
		}

		gba.windowon = (gba.layerenable & 0x6000) ? true : false;
		if(change && !((value & 0x80))) {
			if(!(gba.GBADISPSTAT & 1)) {
				gba.lcdticks = 1008;
				//      VCOUNT = 0;
				//      UPDATE_REG(0x06, VCOUNT);
				gba.GBADISPSTAT &= 0xFFFC;
				UPDATE_REG(0x04 , gba.GBADISPSTAT); //UPDATE_REG(0x04, gba->DISPSTAT);
				//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT(gba);
			}
		//        (*renderLine)();
		}
		// we only care about changes in BG0-BG3
		if(changeBG) {
		}
	break;
    }
	case 0x04:
		gba.GBADISPSTAT = (value & 0xFF38) | (gba.GBADISPSTAT & 7);
		UPDATE_REG(0x04 , gba.GBADISPSTAT);	//UPDATE_REG(0x04, gba->DISPSTAT);
    break;
	case 0x06:
		// not writable
    break;
	case 0x08:
		gba.GBABG0CNT = (value & 0xDFCF);
		UPDATE_REG(0x08 , gba.GBABG0CNT);	//UPDATE_REG(0x08, gba->BG0CNT);
    break;
	case 0x0A:
		gba.GBABG1CNT = (value & 0xDFCF);
		UPDATE_REG(0x0A , gba.GBABG1CNT);	//UPDATE_REG(0x0A, gba->BG1CNT);
    break;
	case 0x0C:
		gba.GBABG2CNT = (value & 0xFFCF);
		UPDATE_REG(0x0C , gba.GBABG2CNT);	//UPDATE_REG(0x0C, gba->BG2CNT);
    break;
	case 0x0E:
		gba.GBABG3CNT = (value & 0xFFCF);
		UPDATE_REG(0x0E , gba.GBABG3CNT);	//UPDATE_REG(0x0E, gba->BG3CNT);
    break;
	case 0x10:
		gba.GBABG0HOFS = value & 511;
		UPDATE_REG(0x10 , gba.GBABG0HOFS);	//UPDATE_REG(0x10, gba->BG0HOFS);
    break;
	case 0x12:
		gba.GBABG0VOFS = value & 511;	
		UPDATE_REG(0x12 , gba.GBABG0VOFS);	//UPDATE_REG(0x12, gba->BG0VOFS);
    break;
	case 0x14:
		gba.GBABG1HOFS = value & 511;
		UPDATE_REG(0x14 , gba.GBABG1HOFS);	//UPDATE_REG(0x14, gba->BG1HOFS);
    break;
	case 0x16:
		gba.GBABG1VOFS = value & 511;
		UPDATE_REG(0x16 , gba.GBABG1VOFS);	//UPDATE_REG(0x16, gba->BG1VOFS);
    break;
	case 0x18:
		gba.GBABG2HOFS = value & 511;
		UPDATE_REG(0x18 , gba.GBABG2HOFS);	//UPDATE_REG(0x18, gba->BG2HOFS);
    break;
	case 0x1A:
		gba.GBABG2VOFS = value & 511;
		UPDATE_REG(0x1A , gba.GBABG2VOFS);	//UPDATE_REG(0x1A, gba->BG2VOFS);
    break;
	case 0x1C:
		gba.GBABG3HOFS = value & 511;
		UPDATE_REG(0x1C , gba.GBABG3HOFS);	//UPDATE_REG(0x1C, gba->BG3HOFS);
    break;
	case 0x1E:
		gba.GBABG3VOFS = value & 511;
		UPDATE_REG(0x1E , gba.GBABG3VOFS);	//UPDATE_REG(0x1E, gba->BG3VOFS);
    break;
	case 0x20:
		gba.GBABG2PA = value;
		UPDATE_REG(0x20 , gba.GBABG2PA);	//UPDATE_REG(0x20, gba->BG2PA);
    break;
	case 0x22:
		gba.GBABG2PB = value;
		UPDATE_REG(0x22 , gba.GBABG2PB);	//UPDATE_REG(0x22, gba->BG2PB);
    break;
	case 0x24:
		gba.GBABG2PC = value;
		UPDATE_REG(0x24 , gba.GBABG2PC);	//UPDATE_REG(0x24, gba->BG2PC);
    break;
	case 0x26:
		gba.GBABG2PD = value;
		UPDATE_REG(0x26 , gba.GBABG2PD);	//UPDATE_REG(0x26, gba->BG2PD);
    break;
	case 0x28:
		gba.GBABG2X_L = value;
		UPDATE_REG(0x28 , gba.GBABG2X_L);	//UPDATE_REG(0x28, gba->BG2X_L);
		gba.gfxbg2changed |= 1;
    break;
	case 0x2A:
		gba.GBABG2X_H = (value & 0xFFF);
		UPDATE_REG(0x2A , gba.GBABG2X_H);	//UPDATE_REG(0x2A, gba->BG2X_H);
		gba.gfxbg2changed |= 1;
    break;
	case 0x2C:
		gba.GBABG2Y_L = value;
		UPDATE_REG(0x2C , gba.GBABG2Y_L);	//UPDATE_REG(0x2C, gba->BG2Y_L);
		gba.gfxbg2changed |= 2;
    break;
	case 0x2E:
		gba.GBABG2Y_H = value & 0xFFF;
		UPDATE_REG(0x2E , gba.GBABG2Y_H);	//UPDATE_REG(0x2E, gba->BG2Y_H);
		gba.gfxbg2changed |= 2;
    break;
	case 0x30:
		gba.GBABG3PA = value;
		UPDATE_REG(0x30 , gba.GBABG3PA);		//UPDATE_REG(0x30, gba->BG3PA);
    break;
	case 0x32:
		gba.GBABG3PB = value;
		UPDATE_REG(0x32 , gba.GBABG3PB);		//UPDATE_REG(0x32, gba->BG3PB);
    break;
	case 0x34:
		gba.GBABG3PC = value;
		UPDATE_REG(0x34 , gba.GBABG3PC);		//UPDATE_REG(0x34, gba->BG3PC);
    break;
	case 0x36:
		gba.GBABG3PD = value;
		UPDATE_REG(0x36 , gba.GBABG3PD);		//UPDATE_REG(0x36, gba->BG3PD);
    break;
	case 0x38:
		gba.GBABG3X_L = value;
		UPDATE_REG(0x38 , gba.GBABG3X_L);		//UPDATE_REG(0x38, gba->BG3X_L);
		gba.gfxbg3changed |= 1;
    break;
	case 0x3A:
		gba.GBABG3X_H = value & 0xFFF;
		UPDATE_REG(0x3A , gba.GBABG3X_H);		//UPDATE_REG(0x3A, gba->BG3X_H);
		gba.gfxbg3changed |= 1;
    break;
	case 0x3C:
		gba.GBABG3Y_L = value;
		UPDATE_REG(0x3C , gba.GBABG3Y_L);		//UPDATE_REG(0x3C, gba->BG3Y_L);
		gba.gfxbg3changed |= 2;
    break;
	case 0x3E:
		gba.GBABG3Y_H = value & 0xFFF;
		UPDATE_REG(0x3E , gba.GBABG3Y_H);		//UPDATE_REG(0x3E, gba->BG3Y_H);
		gba.gfxbg3changed |= 2;
    break;
	case 0x40:
		gba.GBAWIN0H = value;
		UPDATE_REG(0x40 , gba.GBAWIN0H);			//UPDATE_REG(0x40, gba->WIN0H);
    break;
	case 0x42:
		gba.GBAWIN1H = value;
		UPDATE_REG(0x42 , gba.GBAWIN1H);			//UPDATE_REG(0x42, gba->WIN1H);
    break;
	case 0x44:
		gba.GBAWIN0V = value;
		UPDATE_REG(0x44 , gba.GBAWIN0V);			//UPDATE_REG(0x44, gba->WIN0V);
    break;
	case 0x46:
		gba.GBAWIN1V = value;
		UPDATE_REG(0x46 , gba.GBAWIN1V);			//UPDATE_REG(0x46, gba->WIN1V);
    break;
	case 0x48:
		gba.GBAWININ = value & 0x3F3F;
		UPDATE_REG(0x48 , gba.GBAWININ);			//UPDATE_REG(0x48, gba->WININ);
    break;
	case 0x4A:
		gba.GBAWINOUT = value & 0x3F3F;
		UPDATE_REG(0x4A , gba.GBAWINOUT);		//UPDATE_REG(0x4A, gba->WINOUT);
    break;
	case 0x4C:
		gba.GBAMOSAIC = value;
		UPDATE_REG(0x4C , gba.GBAMOSAIC);		//UPDATE_REG(0x4C, gba->MOSAIC);
    break;
	case 0x50:
		gba.GBABLDMOD = value & 0x3FFF;
		UPDATE_REG(0x50 , gba.GBABLDMOD);		//UPDATE_REG(0x50, gba->BLDMOD);
		gba.fxon = ((gba.GBABLDMOD>>6)&3) != 0;
    break;
	case 0x52:
		gba.GBACOLEV = value & 0x1F1F;
		UPDATE_REG(0x52 , gba.GBACOLEV);			//UPDATE_REG(0x52, gba.GBACOLEV);
    break;
	case 0x54:
		gba.GBACOLY = value & 0x1F;
		UPDATE_REG(0x54 , gba.GBACOLY);			//UPDATE_REG(0x54, gba->COLY);
    break;
	case 0x60:
	case 0x62:
	case 0x64:
	case 0x68:
	case 0x6c:
	case 0x70:
	case 0x72:
	case 0x74:
	case 0x78:
	case 0x7c:
	case 0x80:
	case 0x84:
		//soundEvent(gba, address&0xFF, (u8)(value & 0xFF));	//sound not yet!
		//soundEvent(gba, (address&0xFF)+1, (u8)(value>>8));
    break;
	case 0x82:
	case 0x88:
	case 0xa0:
	case 0xa2:
	case 0xa4:
	case 0xa6:
	case 0x90:
	case 0x92:
	case 0x94:
	case 0x96:
	case 0x98:
	case 0x9a:
	case 0x9c:
	case 0x9e:
		//soundEvent(gba, address&0xFF, value);	//sound not yet!
    break;
	case 0xB0:
		gba.GBADM0SAD_L = value;
		UPDATE_REG(0xB0 , gba.GBADM0SAD_L);	//UPDATE_REG(0xB0, gba->DM0SAD_L);
    break;
	case 0xB2:
		gba.GBADM0SAD_H = value & 0x07FF;
		UPDATE_REG(0xB2 , gba.GBADM0SAD_H);	//UPDATE_REG(0xB2, gba->DM0SAD_H);
    break;
	case 0xB4:
		gba.GBADM0DAD_L = value;
		UPDATE_REG(0xB4 , gba.GBADM0DAD_L);	//UPDATE_REG(0xB4, gba->DM0DAD_L);
    break;
	case 0xB6:
		gba.GBADM0DAD_H = value & 0x07FF;
		UPDATE_REG(0xB6 , gba.GBADM0DAD_H);	//UPDATE_REG(0xB6, gba->DM0DAD_H);
    break;
	case 0xB8:
		gba.GBADM0CNT_L = value & 0x3FFF;
		UPDATE_REG(0xB8 , 0);	//UPDATE_REG(0xB8, 0);
    break;
	case 0xBA:{
		bool start = ((gba.GBADM0CNT_H ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		gba.GBADM0CNT_H = value;
		UPDATE_REG(0xBA , gba.GBADM0CNT_H);	//UPDATE_REG(0xBA, gba->DM0CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma0source = gba.GBADM0SAD_L | (gba.GBADM0SAD_H << 16);
			gba.dma0dest = gba.GBADM0DAD_L | (gba.GBADM0DAD_H << 16);
			//CPUCheckDMA(gba, 0, 1); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xBC:
		gba.GBADM1SAD_L = value;
		UPDATE_REG(0xBC , gba.GBADM1SAD_L);	//UPDATE_REG(0xBC, gba->DM1SAD_L);
    break;
	case 0xBE:
		gba.GBADM1SAD_H = value & 0x0FFF;
		UPDATE_REG(0xBE , gba.GBADM1SAD_H);	//UPDATE_REG(0xBE, gba->DM1SAD_H);
    break;
	case 0xC0:
		gba.GBADM1DAD_L = value;
		UPDATE_REG(0xC0 , gba.GBADM1DAD_L);	//UPDATE_REG(0xC0, gba->DM1DAD_L);
    break;
	case 0xC2:
		gba.GBADM1DAD_H = value & 0x07FF;
		UPDATE_REG(0xC2 , gba.GBADM1DAD_H);	//UPDATE_REG(0xC2, gba->DM1DAD_H);
    break;
	case 0xC4:
		gba.GBADM1CNT_L = value & 0x3FFF;
		UPDATE_REG(0xC4 , gba.GBADM1CNT_L);	//UPDATE_REG(0xC4, 0);
    break;
	case 0xC6:{
		bool start = ((gba.GBADM1CNT_H ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		gba.GBADM1CNT_H = value;
		UPDATE_REG(0xC6 , gba.GBADM1CNT_H);	//UPDATE_REG(0xC6, gba->DM1CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma1source = gba.GBADM1SAD_L | (gba.GBADM1SAD_H << 16);
			gba.dma1dest = gba.GBADM1DAD_L | (gba.GBADM1DAD_H << 16);
			//CPUCheckDMA(gba, 0, 2); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xC8:
		gba.GBADM2SAD_L = value;
		UPDATE_REG(0xC8 , gba.GBADM2SAD_L);	//UPDATE_REG(0xC8, gba->DM2SAD_L);
    break;
	case 0xCA:
		gba.GBADM2SAD_H = value & 0x0FFF;
		UPDATE_REG(0xCA , gba.GBADM2SAD_H);	//UPDATE_REG(0xCA, gba->DM2SAD_H);
    break;
	case 0xCC:
		gba.GBADM2DAD_L = value;
		UPDATE_REG(0xCC , gba.GBADM2DAD_L);	//UPDATE_REG(0xCC, gba->DM2DAD_L);
    break;
	case 0xCE:
		gba.GBADM2DAD_H = value & 0x07FF;
		UPDATE_REG(0xCE , gba.GBADM2DAD_H);	//UPDATE_REG(0xCE, gba->DM2DAD_H);
    break;
	case 0xD0:
		gba.GBADM2CNT_L = value & 0x3FFF;
		UPDATE_REG(0xD0 , 0);	//UPDATE_REG(0xD0, 0);
    break;
	case 0xD2:{
		bool start = ((gba.GBADM2CNT_H ^ value) & 0x8000) ? true : false;
		
		value &= 0xF7E0;
		
		gba.GBADM2CNT_H = value;
		UPDATE_REG(0xD2 , gba.GBADM2CNT_H);	//UPDATE_REG(0xD2, gba->DM2CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma2source = gba.GBADM2SAD_L | (gba.GBADM2SAD_H << 16);
			gba.dma2dest = gba.GBADM2DAD_L | (gba.GBADM2DAD_H << 16);

			//CPUCheckDMA(gba, 0, 4); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xD4:
		gba.GBADM3SAD_L = value;
		UPDATE_REG(0xD4 , gba.GBADM3SAD_L);	//UPDATE_REG(0xD4, gba->DM3SAD_L);
    break;
	case 0xD6:
		gba.GBADM3SAD_H = value & 0x0FFF;
		UPDATE_REG(0xD6 , gba.GBADM3SAD_H);	//UPDATE_REG(0xD6, gba->DM3SAD_H);
    break;
	case 0xD8:
		gba.GBADM3DAD_L = value;
		UPDATE_REG(0xD8 , gba.GBADM3DAD_L);	//UPDATE_REG(0xD8, gba.GBADM3DAD_L);
    break;
	case 0xDA:
		gba.GBADM3DAD_H = value & 0x0FFF;
		UPDATE_REG(0xDA , gba.GBADM3DAD_H);	//UPDATE_REG(0xDA, gba->DM3DAD_H);
    break;
	case 0xDC:
		gba.GBADM3CNT_L = value;
		UPDATE_REG(0xDC , 0);	//UPDATE_REG(0xDC, 0);
    break;
	case 0xDE:{
		bool start = ((gba.GBADM3CNT_H ^ value) & 0x8000) ? true : false;
		
		value &= 0xFFE0;
		
		gba.GBADM3CNT_H = value;
		UPDATE_REG(0xDE , gba.GBADM3CNT_H);	//UPDATE_REG(0xDE, gba->DM3CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma3source = gba.GBADM3SAD_L | (gba.GBADM3SAD_H << 16);
			gba.dma3dest = gba.GBADM3DAD_L | (gba.GBADM3DAD_H << 16);
			//CPUCheckDMA(gba, 0, 8); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0x100:
		gba.timer0reload = value;
    break;
	case 0x102:
		gba.timer0value = value;
		//gba.timerOnOffDelay|=1; //added delta before activating timer?
		gba.cpunextevent = cputotalticks; //ori: gba.cputotalticks;
    break;
	case 0x104:
		gba.timer1reload = value;
    break;
	case 0x106:
		gba.timer1value = value;
		//gba->timerOnOffDelay|=2;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;
	case 0x108:
		gba.timer2reload = value;
    break;
	case 0x10A:
		gba.timer2value = value;
		//gba.timerOnOffDelay|=4;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;
	case 0x10C:
		gba.timer3reload = value;
    break;
	case 0x10E:
		gba.timer3value = value;
		//gba->timerOnOffDelay|=8;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;

	case 0x130:
		gba.GBAP1 |= (value & 0x3FF);
		UPDATE_REG(0x130 , gba.GBAP1);	//UPDATE_REG(0x130, gba->P1);
	break;

	case 0x132:
		UPDATE_REG(0x132 , value & 0xC3FF);	//UPDATE_REG(0x132, value & 0xC3FF);
	break;

	case 0x200:
		gbavirt_iemasking = (value & 0x3FFF);
		UPDATE_REG(0x200 , gbavirt_iemasking);	//UPDATE_REG(0x200, gba->IE);
		if ((gbavirt_imemasking & 1) && (gbavirt_ifmasking & gbavirt_iemasking) && (i_flag==true))
			gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks; //acknowledge cycle & program flow
	break;
	case 0x202:
		gbavirt_ifmasking ^= (value & gbavirt_ifmasking);
		UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, gba.IF);
    break;
	case 0x204:{
		gba.memorywait[0x0e] = gba.memorywaitseq[0x0e] = gamepakramwaitstate[value & 3];

		gba.memorywait[0x08] = gba.memorywait[0x09] = gamepakwaitstate[(value >> 2) & 3];
		gba.memorywaitseq[0x08] = gba.memorywaitseq[0x09] =
		gamepakwaitstate0[(value >> 4) & 1];

		gba.memorywait[0x0a] = gba.memorywait[0x0b] = gamepakwaitstate[(value >> 5) & 3];
		gba.memorywaitseq[0x0a] = gba.memorywaitseq[0x0b] =
		gamepakwaitstate1[(value >> 7) & 1];

		gba.memorywait[0x0c] = gba.memorywait[0x0d] = gamepakwaitstate[(value >> 8) & 3];
		gba.memorywaitseq[0x0c] = gba.memorywaitseq[0x0d] =
		gamepakwaitstate2[(value >> 10) & 1];
		
		/* //not now
		for(i = 8; i < 15; i++) {
			gba.memorywait32[i] = gba.memorywait[i] + gba.memorywaitseq[i] + 1;
			gba.memorywaitseq32[i] = gba.memorywaitseq[i]*2 + 1;
		}*/

		if((value & 0x4000) == 0x4000) {
			gba.busprefetchenable = true;
			gba.busprefetch = false;
			gba.busprefetchcount = 0;
		} 
		else {
			gba.busprefetchenable = false;
			gba.busprefetch = false;
			gba.busprefetchcount = 0;
		}
		UPDATE_REG(0x204 , value & 0x7FFF); //UPDATE_REG(0x204, value & 0x7FFF);

	}
    break;
	case 0x208:
		gbavirt_imemasking = value & 1;
		UPDATE_REG(0x208 , gbavirt_imemasking); //UPDATE_REG(0x208, gba->IME);
		if ((gbavirt_imemasking & 1) && (gbavirt_ifmasking & gbavirt_iemasking) && (i_flag==true))
			gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;
	case 0x300:
	if(value != 0)
		value &= 0xFFFE;
		UPDATE_REG(0x300 , value); 	//UPDATE_REG(0x300, value);
    break;
	default:
		UPDATE_REG((address&0x3FE) , value);		//UPDATE_REG(address&0x3FE, value);
    break;
  }

return 0;
}

//direct GBA CPU reads
u32 virtread_word(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u32)*((u32*)address);
}

u16 virtread_hword(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u16)*((u16*)address);
}

u8 virtread_byte(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u8)*((u8*)address);
}

//direct GBA CPU writes
u32 virtwrite_word(u32 address,u32 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u32*)address)=data;
return 0;
}

u16 virtwrite_hword(u32 address,u16 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u16*)address)=data;
return 0;
}

u8 virtwrite_byte(u32 address,u8 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u8*)address)=data;
return 0;
}

//hamming weight read from memory CPU opcodes

//#define CPUReadByteQuick(gba, addr)
//(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]

u8 __attribute__ ((hot)) cpuread_bytefast(u8 address){
#ifdef DEBUGEMU
debuggeroutput();
#endif
return  ldru8extasm(
		(u32)&(gba).map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask],
		0x0);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadHalfWordQuick(gba, addr)
//READ16LE(((u16*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
  
u16 __attribute__ ((hot)) cpuread_hwordfast(u16 address){
#ifdef DEBUGEMU
debuggeroutput();
#endif

return  ldru16extasm(
		(u32)&(gba).map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask],
		0x0);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadMemoryQuick(gba, addr) 
//READ32LE(((u32*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
		
u32 __attribute__ ((hot)) cpuread_wordfast(u32 address){
#ifdef DEBUGEMU
debuggeroutput();
#endif

return  ldru32extasm(
		((u32)&gba.map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask]),
		0x0);
}

////////////////////////////////////VBA CPU read mechanism/////////////////////////////////////////////// //coto
//u8 CPUGBA reads
//old: CPUReadByte(GBASystem *gba, u32 address);
u8 __attribute__ ((hot)) cpuread_byte(u32 address){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

switch(address >> 24) {
	case 0:
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xf), 32,0);	//PC fetch
		if ((dummyreg) >> 24) {
			if(address < 0x4000) {
				#ifdef DEBUGEMU
					printf("byte: gba.bios protected read! (%x) \n",(unsigned int)address);
				#endif
				//return gba->biosProtected[address & 3];
				return u8read((u32)gba.biosprotected,(address & 3));
			} 
			else 
				goto unreadable;
		}
	#ifdef DEBUGEMU
		printf("byte: gba.bios read! (%x) \n",(unsigned int)address);
	#endif
	
		//return gba->bios[address & 0x3FFF];
		return u8read((u32)gba.bios,(address & 0x3FFF));
	break;
	case 2:
		#ifdef DEBUGEMU
			printf("byte: gba.workram read! (%x) \n",(unsigned int)address);
		#endif
		
		//return gba->workRAM[address & 0x3FFFF];
		return u8read((u32)gba.workram,(address & 0x3FFFF));
	break;
	case 3:
		#ifdef DEBUGEMU
			printf("byte: gba.intram read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->internalRAM[address & 0x7fff];
		return u8read((u32)gba.intram,(address & 0x7fff));
	break;
	case 4:
		if((address < 0x4000400) && gba.ioreadable[address & 0x3ff]){
			#ifdef DEBUGEMU
				printf("byte: gba.IO read! (%x) \n",(unsigned int)address);
			#endif
			//return gba->ioMem[address & 0x3ff];
			return u8read((u32)gba.iomem,(address & 0x3ff));
		}
		else 
			goto unreadable;
	break;
	case 5:
		#ifdef DEBUGEMU
			printf("byte: gba.palram read! (%x) \n",(unsigned int)address);
		#endif
		
		//return gba->paletteRAM[address & 0x3ff];
		return u8read((u32)gba.palram,(address & 0x3ff));
	break;
	case 6:
		address = (address & 0x1ffff);
		if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		#ifdef DEBUGEMU
			printf("byte: gba.vidram read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->vram[address];
		return u8read((u32)gba.vidram,address);
	break;
	case 7:
		#ifdef DEBUGEMU
			printf("byte: gba.oam read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->oam[address & 0x3ff];
		return u8read((u32)gba.oam,(address & 0x3ff));
	break;
	case 8: case 9: case 10: case 11: case 12:
		//coto
		#ifndef ROMTEST
			return (u8)(stream_readu8(address % 0x08000000)); //gbareads SHOULD NOT be aligned
		#endif
			
		#ifdef ROMTEST
			return (u8)*(u8*)(&rom_pl_bin+((address % 0x08000000)/4 ));
		#endif
		//return 0;
	break;
	case 13:
		if(gba.cpueepromenabled)
			//return eepromRead(address); //saves not yet
			return 0;
		else
			goto unreadable;
	break;
	case 14:
		if(gba.cpusramenabled | gba.cpuflashenabled)
			//return flashRead(address);
			return 0;
		else
			goto unreadable;
			
		if(gba.cpueepromsensorenabled) {
			switch(address & 0x00008f00) {
				case 0x8200:
					//return systemGetSensorX() & 255; //sensor not yet
				case 0x8300:
					//return (systemGetSensorX() >> 8)|0x80; //sensor not yet
				case 0x8400:
					//return systemGetSensorY() & 255; //sensor not yet
				case 0x8500:
					//return systemGetSensorY() >> 8; //sensor not yet
				break;
			}
			return 0; //for now until eeprom is implemented
		}
	break;
	//default
	default:
	unreadable:
		
		//arm read
		if(armstate==0) {
			//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
			#ifdef DEBUGEMU
				printf("byte:[ARM] default read! (%x) \n",(unsigned int)address);
			#endif
			
			return virtread_word(rom);
		} 
		
		//thumb read
		else{
			//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
			//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
			#ifdef DEBUGEMU
			printf("byte:[THMB] default read! (%x) \n",(unsigned int)address);
			#endif
			return ( (virtread_hword(rom)) | ((virtread_hword(rom))<< 16) ); //half word duplicate
		}
		
		
		if(gba.cpudmahack) {
			return gba.cpudmalast & 0xFF;
		} 
		else{
			if((armstate)==0){
				#ifdef DEBUGEMU
				printf("byte:[ARM] default GBAmem read! (%x) \n",(unsigned int)address);
				#endif
				//return CPUReadByteQuick(gba, gba->reg[15].I+(address & 3));
				return cpuread_bytefast(rom+((address & 3)));
			} 
			else{
				#ifdef DEBUGEMU
				printf("byte:[THMB] default GBAmem read! (%x) \n",(unsigned int)address);
				#endif
				
				//return CPUReadByteQuick(gba, gba->reg[15].I+(address & 1));
				return cpuread_bytefast(rom+((address & 1)));
			}
		}
		
	break;
}

}

//u16 CPUGBA reads
//old: CPUReadHalfWord(GBASystem *gba, u32 address)
u16 __attribute__ ((hot)) cpuread_hword(u32 address){

//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

u32 value=0;
switch(address >> 24) {
	case 0:
	fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xf), 32,0); 
		if ((dummyreg) >> 24) {
			if(address < 0x4000) {
				//value = ldru16extasm((u32)(u8*)gba.biosprotected,(address&2)); 	//value = READ16LE(((u16 *)&gba->biosProtected[address&2]));
				//#ifdef DEBUGEMU
				//printf("hword gba.biosprotected read! \n");
				//#endif
				
				#ifdef DEBUGEMU
				printf("hword gba.bios read! \n");
				#endif
				//value = ldru16extasm((u32)(u8*)gba.bios,address);
				value = u16read((u32)gba.bios,address);
			} 
			else 
				goto unreadable;
		} 
		else {
			#ifdef DEBUGEMU
			printf("hword gba.bios read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->bios[address & 0x3FFE]));
			value = u16read((u32)gba.bios,(address & 0x3FFE));
		}
	break;
	case 2:
			#ifdef DEBUGEMU
			printf("hword gba.workram read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->workRAM[address & 0x3FFFE]));
			value = u16read((u32)gba.workram,(address & 0x3FFFE));
	break;
	case 3:
			#ifdef DEBUGEMU
			printf("hword gba.intram read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->internalRAM[address & 0x7ffe]));
			value = u16read((u32)gba.intram,(address & 0x7ffe));
	break;
	case 4:
		if((address < 0x4000400) && gba.ioreadable[address & 0x3fe]){
			#ifdef DEBUGEMU
			printf("hword gba.IO read! \n");
			#endif
			
			//value =  READ16LE(((u16 *)&gba->ioMem[address & 0x3fe]));
			value = u16read((u32)gba.iomem,(address & 0x7ffe));
				
				if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E)){
					
					if (((address & 0x3fe) == 0x100) && gba.timer0on)
						//ori: value = 0xFFFF - ((gba.timer0ticks - gba.cputotalticks) >> gba.timer0clockreload); //timer top - timer period
						value = 0xFFFF - ((gba.timer0ticks - cputotalticks) >> gba.timer0clockreload);
						
					else {
						if (((address & 0x3fe) == 0x104) && gba.timer1on && !(gba.GBATM1CNT & 4))
							//ori: value = 0xFFFF - ((gba.timer1ticks - gba.cputotalticks) >> gba.timer1clockreload);
							value = 0xFFFF - ((gba.timer1ticks - cputotalticks) >> gba.timer1clockreload);
						else
							if (((address & 0x3fe) == 0x108) && gba.timer2on && !(gba.GBATM2CNT & 4))
								//ori: value = 0xFFFF - ((gba.timer2ticks - gba.cputotalticks) >> gba.timer2clockreload);
								value = 0xFFFF - ((gba.timer2ticks - cputotalticks) >> gba.timer2clockreload);
							else
								if (((address & 0x3fe) == 0x10C) && gba.timer3on && !(gba.GBATM3CNT & 4))
									//ori: value = 0xFFFF - ((gba.timer3ticks - gba.cputotalticks) >> gba.timer2clockreload);
									value = 0xFFFF - ((gba.timer3ticks - cputotalticks) >> gba.timer2clockreload);
					}
				}
		}
		else 
			goto unreadable;
    break;
	case 5:
		#ifdef DEBUGEMU
			printf("hword gba.palram read! \n");
		#endif
		//value = READ16LE(((u16 *)&gba->paletteRAM[address & 0x3fe]));
		value = u16read((u32)gba.palram,(address & 0x3fe));
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			#ifdef DEBUGEMU
			printf("hword gba.vidram read! \n");
			#endif
		//value = READ16LE(((u16 *)&gba->vram[address]));
		value = u16read((u32)gba.vidram,(address));
	break;
	case 7:
			#ifdef DEBUGEMU
			printf("hword gba.oam read! \n");
			#endif
		//value = READ16LE(((u16 *)&gba->oam[address & 0x3fe]));
		value = u16read((u32)gba.oam,(address & 0x3fe));
	break;
	case 8: case 9: case 10: case 11: case 12:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			//value = rtcRead(address);	//RTC not yet
		}
		else
			//value = READ16LE(((u16 *)&gba->rom[address & 0x1FFFFFE]));
			
		#ifndef ROMTEST
			return stream_readu16(address % 0x08000000); //gbareads are never word aligned. (why would you want that?!)
		#endif
		
		#ifdef ROMTEST
			return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
		#endif
		
	break;
	case 13:
		if(gba.cpueepromenabled)
		// no need to swap this
		//return  eepromRead(address);	//saving not yet
    goto unreadable;
  
	case 14:
		if(gba.cpuflashenabled | gba.cpusramenabled){
		//no need to swap this
		//return flashRead(address);	//saving not yet
		}
	//default
	default:
		unreadable:
			
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				printf("hword default read! \n");
				#endif
				//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
				value=cpuread_wordfast(rom);
			
			//thumb read
			} 
			else{
				#ifdef DEBUGEMU
				printf("hword default read! (x2) \n");
				#endif
				
				//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
				//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
				value = cpuread_hwordfast(rom) | (cpuread_hwordfast(rom) << 16);
			}
			
			
			if(gba.cpudmahack) {
				value = gba.cpudmalast & 0xFFFF;
			} 
			else{
				//thumb?
				if((armstate)==1) {
					#ifdef DEBUGEMU
					printf("[THMB]hword default read! \n");
					#endif
				
					//value = CPUReadHalfWordQuick(gba, gba->reg[15].I + (address & 2));
					value	= cpuread_hwordfast(rom+(address & 2));
				} 
				//arm?
				else{
					#ifdef DEBUGEMU
					printf("[ARM]hword default read! \n");
					#endif
				
					//value = CPUReadHalfWordQuick(gba, gba->reg[15].I);
					value	= cpuread_hwordfast(rom);
				}
			}
			
    break;
}

if(address & 1) {
	value = (value >> 8) | (value << 24);
}

return value;
}

//u32 CPUGBA reads
//old: CPUReadMemory(GBASystem *gba, u32 address)
u32 __attribute__ ((hot)) cpuread_word(u32 address){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

	u32 value=0;
	switch(address >> 24) {
		case 0:
			fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xf), 32,0); //pc
			if(dummyreg >> 24) {
				if(address < 0x4000) {
					//value = READ32LE(((u32 *)&gba->biosProtected));
					//value = ldru32asm((u32)(u8*)gba.biosprotected,0x0);
					//printf("IO access!");
					//#ifdef DEBUGEMU
					//printf("Word gba.biosprotected read! \n");
					//#endif
					#ifdef DEBUGEMU
						printf("Word gba.bios read! (%x)[%x] \n",(unsigned int)((u32)(u8*)gba.bios+address),(unsigned int)*(u32*)((u32)(u8*)gba.bios+address));
					#endif
				
					value = u32read((u32)gba.bios,address);
				}
				else 
					goto unreadable;
			}
			else{
				//printf("any access!");
				//value = READ32LE(((u32 *)&gba->bios[address & 0x3FFC]));
				//ori:
				//value = ldru32extasm((u32)(u8*)gba.bios,(address & 0x3FFC));
				//#ifdef DEBUGEMU
				//printf("Word gba.biosprotected read! \n");
				//#endif
				
				#ifdef DEBUGEMU
					printf("Word gba.bios read! (%x)[%x] \n",(unsigned int)((u32)(u8*)gba.bios+address),(unsigned int)*(u32*)((u32)(u8*)gba.bios+address));
				#endif
			
				value = u32read((u32)gba.bios,address);
			}
		break;
		case 2:
			#ifdef DEBUGEMU
			printf("Word gba.workram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->workRAM[address & 0x3FFFC]));
			value = u32read((u32)gba.workram,address & 0x3FFFC);
		break;
		case 3:
			//stacks should be here..
			#ifdef DEBUGEMU
			printf("Word gba.intram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->internalRAM[address & 0x7ffC]));
			value = u32read((u32)gba.intram, (address & 0x7ffC));
		break;
		case 4: //ioReadable is mapped
			if((address < 0x4000400) && gba.ioreadable[address & 0x3fc]) {
				//u16 or u32
				if(gba.ioreadable[(address & 0x3fc) + 2]) {
					#ifdef DEBUGEMU
					printf("Word gba.iomem read! \n");
					#endif
					
					//value = READ32LE(((u32 *)&gba->ioMem[address & 0x3fC]));
					value = u32read((u32)gba.iomem,(address & 0x3fC));
				} 
				else{
					#ifdef DEBUGEMU
					printf("HWord gba.iomem read! \n");
					#endif
					
					//value = READ16LE(((u16 *)&gba->ioMem[address & 0x3fc]));
					value = u16read((u32)gba.iomem,(address & 0x3fC));
				}
			}
			else
				goto unreadable;
		break;
		case 5:
			#ifdef DEBUGEMU
			printf("Word gba.palram read! \n");
			#endif
					
			//value = READ32LE(((u32 *)&gba->paletteRAM[address & 0x3fC]));
			value = u16read((u32)gba.palram,(address & 0x3fC));
		break;
		case 6:
			address = (address & 0x1fffc);
			if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			
			#ifdef DEBUGEMU
			printf("Word gba.vidram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->vram[address]));
			value = u16read((u32)gba.vidram,(address));	
		break;
		case 7:
			#ifdef DEBUGEMU
			printf("Word gba.oam read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->oam[address & 0x3FC]));
			//value = ldru32extasm((u32)(u8*)gba.oam,(address & 0x3FC));
			value = u16read((u32)gba.oam,(address & 0x3FC));
		break;
		case 8: case 9: case 10: case 11: case 12:{
			//#ifdef DEBUGEMU
			//printf("Word gba read! (%x) \n",(unsigned int)address);
			//#endif
			
			#ifndef ROMTEST
				return (u32)(stream_readu32(address % 0x08000000)); //gbareads are OK and from HERE should NOT be aligned (that is module dependant)
			#endif
			
			#ifdef ROMTEST
				return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
			#endif
			
		break;
		}
		case 13:
			//if(gba.cpueepromenabled)
				// no need to swap this
					//return eepromRead(address);	//save not yet
		break;
		goto unreadable;
		case 14:
			if((gba.cpuflashenabled==1) || (gba.cpusramenabled==1) )
				// no need to swap this
					//return flashRead(address);	//save not yet
				return 0;
		break;
		
		default:
		unreadable:
			//printf("default wordread");
			//struct: map[index].address[(u8*)address]
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				printf("WORD:default read! (%x)[%x] \n",(unsigned int)address,(unsigned int)value);
				#endif
				
				//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
				value=cpuread_wordfast(rom);
				
			//thumb read
			} 
			else{
				//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
				//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
				#ifdef DEBUGEMU
				printf("HWORD:default read! (%x)[%x] \n",(unsigned int)address,(unsigned int)value);
				#endif
				value=cpuread_hwordfast((u16)(rom)) | (cpuread_hwordfast((u16)(rom))<<16) ;
			}
	}
	//shift by n if not aligned (from thumb for example)
	if(address & 3) {
		int shift = (address & 3) << 3;
			value = (value >> shift) | (value << (32 - shift));
	}

return value;
}

//CPU WRITES
//ori: CPUWriteByte(GBASystem *gba, u32 address, u8 b)
u32 __attribute__ ((hot)) cpuwrite_byte(u32 address,u8 b){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 2:
		#ifdef DEBUGEMU
		printf("writebyte: writing to gba.workram");
		#endif
		
		//gba->workRAM[address & 0x3FFFF] = b;
		u32store((u32)gba.workram,(address & 0x3FFFF),b);
	break;
	case 3:
		#ifdef DEBUGEMU
		printf("writebyte: writing to gba.intram");
		#endif
		
		//gba->internalRAM[address & 0x7fff] = b;
		u32store((u32)gba.intram,(address & 0x7fff),b);
	break;
	case 4:
		if(address < 0x4000400) {
			switch(address & 0x3FF) {
				case 0x60:
				case 0x61:
				case 0x62:
				case 0x63:
				case 0x64:
				case 0x65:
				case 0x68:
				case 0x69:
				case 0x6c:
				case 0x6d:
				case 0x70:
				case 0x71:
				case 0x72:
				case 0x73:
				case 0x74:
				case 0x75:
				case 0x78:
				case 0x79:
				case 0x7c:
				case 0x7d:
				case 0x80:
				case 0x81:
				case 0x84:
				case 0x85:
				case 0x90:
				case 0x91:
				case 0x92:
				case 0x93:
				case 0x94:
				case 0x95:
				case 0x96:
				case 0x97:
				case 0x98:
				case 0x99:
				case 0x9a:
				case 0x9b:
				case 0x9c:
				case 0x9d:
				case 0x9e:
				case 0x9f:
					#ifdef DEBUGEMU
						printf("writebyte: updating AUDIO/CNT MAP");
					#endif
					//soundEvent(gba, address&0xFF, b); //sound not yet
				break;
				case 0x301: // HALTCNT, undocumented
					if(b == 0x80)
						//gba.stop = true;
					//gba->holdState = 1;
					//gba->holdType = -1;
					gba.cpunextevent = cputotalticks; //ori: gba.cpunextevent = gba.cputotalticks;
					
				break;
				default:{ // every other register
						u32 lowerBits = (address & 0x3fe);
						if(address & 1) {
							#ifdef DEBUGEMU
								printf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE(&gba->ioMem[lowerBits]) & 0x00FF) | (b << 8));
							cpu_updateregisters(lowerBits, ( (((u16*)gba.iomem)[lowerBits]) & 0x00FF) | (b << 8));
						} 
						else {
							#ifdef DEBUGEMU
								printf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE((u32)&gba.iomem[lowerBits]) & 0xFF00) | b);
							cpu_updateregisters(lowerBits, ( (((u16*)gba.iomem)[lowerBits]) & 0xFF00) | (b));
						}
				}
				break;
			}
		}
		//else 
			//goto unwritable;
    break;
	case 5:
		
		#ifdef DEBUGEMU
		printf("writebyte: writing to gba.palram");
		#endif
		
		//no need to switch
		//*((u16 *)&gba->paletteRAM[address & 0x3FE]) = (b << 8) | b;
		u16store((u32)gba.palram,(address & 0x3FE),(b << 8) | b);
	break;
	case 6:
		address = (address & 0x1fffe);
		if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;

		// no need to switch
		// byte writes to OBJ VRAM are ignored
		if ((address) < objtilesaddress[((gba.GBADISPCNT&7)+1)>>2]){
			#ifdef DEBUGEMU
			printf("writebyte: writing to gba.vidram");
			#endif
		
			//*((u16 *)&gba->vram[address]) = (b << 8) | b;
			//stru16inlasm((u32)(u8*)gba.vidram,(address),(b << 8) | b);
			u16store((u32)gba.vidram,address,((b << 8) | b));
		}
    break;
	case 7:
		// no need to switch
		// byte writes to OAM are ignored
		// *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
    break;
	case 13:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, b);	//saves not yet
			break;
		}
	break;
	//goto unwritable;
	
	case 14: default:
		
		#ifdef DEBUGEMU
		printf("writebyte: default write!");
		#endif
		
		virtwrite_byte(address,b);
	break;
  }

return 0;
}

//ori: CPUWriteHalfWord(GBASystem *gba, u32 address, u16 value)
u32 __attribute__ ((hot)) cpuwrite_hword(u32 address, u16 value){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 2:
		#ifdef DEBUGEMU
		printf("writehword: gba.workram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->workRAM[address & 0x3FFFE]),value);
		u16store((u32)gba.workram,(address & 0x3FFFE),(value&0xffff));
	break;
	case 3:
		#ifdef DEBUGEMU
		printf("writehword: gba.intram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->internalRAM[address & 0x7ffe]), value);
		u16store((u32)gba.intram,(address & 0x7ffe),value);
	break;
	case 4:
		if(address < 0x4000400){
			printf("writehword: gba.IO regs write!");
			//CPUUpdateRegister(gba, address & 0x3fe, value);
			cpu_updateregisters(address & 0x3fe, value);
		}
		//else 
		//	goto unwritable;
    break;
	case 5:
		#ifdef DEBUGEMU
		printf("writehword: gba.palram write!");
		#endif
			
		//WRITE16LE(((u16 *)&gba->paletteRAM[address & 0x3fe]), value);
		u16store((u32)gba.palram,address & 0x3fe,value);
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		#ifdef DEBUGEMU
		printf("writehword: gba.vidram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->vram[address]), value);
		u16store((u32)gba.vidram,address,value);
	break;
	case 7:
		#ifdef DEBUGEMU
		printf("writehword: gba.oam write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->oam[address & 0x3fe]), value);
		u16store((u32)gba.oam,address & 0x3fe,value);
		
	break;
	case 8:
	case 9:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
			/* if(!rtcWrite(address, value))
				goto unwritable; */ //rtc not yet
				return 0; //temporal, until rtc is fixed
		} 
		/* else if(!agbPrintWrite(address, value)) 
			goto unwritable; */ //agbprint not yet
			
			else return 0; //temporal, until rtc is fixed
    break;
	case 13:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, (u8)value); //save not yet
			break;
		}
    break;
	//goto unwritable;
	
	case 14: default:
		#ifdef DEBUGEMU
		printf("writehword: default write!");
		#endif
		
		virtwrite_hword(address,value);
	break;
}

return 0;
}

//ori: CPUWriteMemory(GBASystem *gba, u32 address, u32 value)
u32 __attribute__ ((hot)) cpuwrite_word(u32 address, u32 value){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 0x02:
		#ifdef DEBUGEMU
		printf("writeword: writing to gba.workram");
		#endif
		
		//WRITE32LE(((u32 *)&gba->workRAM[address & 0x3FFFC]), value);
		u32store((u32)gba.workram,address & 0x3FFFC,value);
	break;
	case 0x03:
		#ifdef DEBUGEMU
		printf("writeword: writing to gba.intram: (%x)<-[%x]",(unsigned int)((u32)(u8*)gba.intram+(address & 0x7ffC)),(unsigned int)value);
		#endif
		
		//WRITE32LE(((u32 *)&gba->internalRAM[address & 0x7ffC]), value);
		//stru32inlasm((u32)(u8*)gba.intram,(address & 0x7ffC),value);
		//*(u32*)((u32)(u8*)gba.intram+(address & 0x7ffC))=value; //alt
		
		//DOES NOT WORK BECAUSE GBA.INTRAM HANDLES (U8*)
		//gba.intram[address & 0x7ffC]=value;
		//THIS FIXES IT
		u32store((u32)gba.intram,(address & 0x7ffC),value);
	break;
	case 0x04:
		if(address < 0x4000400) {
			//CPUUpdateRegister(gba, (address & 0x3FC), value & 0xFFFF);
			cpu_updateregisters(address & 0x3FC, (value & 0xFFFF));
			
			//CPUUpdateRegister(gba, (address & 0x3FC) + 2, (value >> 16));
			cpu_updateregisters((address & 0x3FC) + 2, (value >> 16));
			#ifdef DEBUGEMU
				printf("writeword: updating IO MAP");
			#endif
		} 
		else {
			virtwrite_word(address,value); //goto unwritable;
			printf("#inminent lockup!");
		}
	break;
	case 0x05:
		#ifdef DEBUGEMU
		printf("writeword: writing to gba.palram");
		#endif
		//WRITE32LE(((u32 *)&gba->paletteRAM[address & 0x3FC]), value);
		u32store((u32)gba.palram,address & 0x3FC,value);
	break;
	case 0x06:
		address = (address & 0x1fffc);
			if (((gba.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		
		#ifdef DEBUGEMU
		printf("writeword: writing to gba.vidram");
		#endif
		//WRITE32LE(((u32 *)&gba->vram[address]), value);
		u32store((u32)gba.vidram,address,value);
	break;
	case 0x07:
		#ifdef DEBUGEMU
		printf("writeword: writing to gba.oam");
		#endif
		
		//WRITE32LE(((u32 *)&gba->oam[address & 0x3fc]), value);
		u32store((u32)gba.oam,address & 0x3fc,value);
	break;
	case 0x0D:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, value); //saving not yet
			#ifdef DEBUGEMU
			printf("writeword: writing to eeprom");
			#endif
			break;
		}
   break;
   //goto unwritable;
	
	// default
	case 0x0E: default:{
		#ifdef DEBUGEMU
		printf("writeword: address[%x]:(%x) fallback to default",(unsigned int) (address+0x4),(unsigned int)value);
		#endif
	
		virtwrite_word(address,value);
		break;
	}
}

return 0;	
}

////////////////////////////////VBA CPU read mechanism end////////////////////////////////////////// 

//data stack manipulation calls

//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 
// 0 = for CPU swap opcodes (cpurestore/cpubackup) modes
// 1 = for all LDMIAVIRT/STMIAVIRT OPCODES
// 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending

//1) create a call that retrieves data from thumb SP GBA
u32 __attribute__ ((hot)) ldmiavirt(u8 * output_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped,u8 order){ //access 8 16 or 32
int cnt=0;
int offset=0;

//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order

switch(byteswapped){
case(0): 	
	while(cnt<16){
		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
	
			if (access == (u8)8){
			
			}
			else if (access == (u8)16){
			
			}
			else if (access == (u8)32){
				if ((order&1) == 0){
					*((u32*)output_buf+(offset))=*((u32*)stackptr+(cnt)); // *(level_addr[0] + offset) //where offset is index depth of (u32*) / OLD
					//printf(" ldmia:%x[%x]",offset,*((u32*)stackptr+(cnt)));
				}
				else{
					*((u32*)output_buf-(offset))=*((u32*)stackptr-(cnt));
				}
			}
		offset++; //linear offset
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
			
			if ((order&1) == 0)
				*((u32*)output_buf+(offset))=rom;
			
			else
				*((u32*)output_buf-(offset))=rom;
		}
	
	cnt++; //non linear register offset
	}
break;

//if you come here again because values aren't r or w, make sure you actually copy them from the register list to be copied
//this can't be modified as breaks ldmia gbavirtreg handles (read/write). SPECIFICALLY DONE FOR gbavirtreg[0] READS. don't use it FOR ANYTHING ELSE.

case(1):
	while(cnt<16) {
		if((regs>>cnt) & 0x1){
		//printf("%x ",cnt);
		
		if (access == (u8)8)
			*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset));
		
		else if (access == (u8)16){
			//printf("%x ",cnt);
			//printf("%x ",(u32*)stackptr);
			//for(i=0;i<16;i++){
			//printf("%x ", *((u32*)input_buf+i*4));
			//}
			//printf("%x ",cnt);
			//printf("ldmia:%d[%x] \n", offset,*((u32*)stackptr+(offset)));
			*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
		}	
		
		else if (access == (u8)32){
			
			if ((order&1) == 0){
				//printf("%x ",cnt);
				//printf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//printf("%x ", *((u32*)stackptr+i*4));
				//}
				//printf("%x ",cnt);
				//printf("ldmia:%d[%x] \n", offset,*((u32*)stackptr+(cnt)));
				*((u32*)output_buf+(offset))= *((u32*)stackptr+(cnt));
			}
			else{
				*((u32*)output_buf-(offset))= *((u32*)stackptr-(cnt));
			}
		}
			
		offset++;
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (PUSH-POP MANAGEMENT)
case (2):

	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
			//printf("ldmia virt: %x ",cnt);
		
			if (access == (u8)8)
				*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset)) ;
		
			else if (access == (u8)16){
				//printf("%x ",cnt);
				//printf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//printf("%x ", *((u32*)input_buf+i*4));
				//}
				//printf("%x ",cnt);
					*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
			}	
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//printf("%x ",cnt);
					//printf("%x ",(u32*)stackptr);
					//for(i=0;i<16;i++){
					//printf("%x ", *((u32*)stackptr+i*4));
					//}
					//printf("%x ",cnt);
		
					//printf("(2)ldmia:%x[%x]",offset,*((u32*)stackptr+(offset)));
					*((u32*)output_buf+(cnt))= *((u32*)stackptr+(offset));
				}
				else{
					*((u32*)output_buf-(cnt))= *((u32*)stackptr-(offset));
				}
			}
			offset++;
		}
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			
			if ((order&1) == 0){
				//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				rom=*((u32*)stackptr+(offset));
			}
			else{
				rom=*((u32*)stackptr-(offset));
			}
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (LDMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
case(3): 	
	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
		//printf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//Ok but direct reads
					//*(u32*) ( (*((u32*)output_buf+ 0)) + (offset*4) )=*((u32*)stackptr+(cnt));
					//printf(" ldmia%x:srcvalue[%x]",cnt, *((u32*)stackptr+(cnt)) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
			
					//handled reads is GBA CPU layer specific:
					*((u32*)(output_buf+(cnt*4)))=cpuread_word((u32)(stackptr+(offset*4)));
					//printf(" (3)ldmia%x:srcvalue[%x]",cnt, cpuread_word(stackptr+(offset*4)));
				}
				else{
					*((u32*)(output_buf-(cnt*4)))=cpuread_word((u32)(stackptr-(offset*4)));
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				//Ok but direct reads
				//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				//rom=*((u32*)stackptr+(offset));
			
				//handled reads is GBA CPU layer specific:
				rom=cpuread_word((u32)(stackptr+(cnt*4)));
				break;
			}
			else{
				rom=cpuread_word((u32)(stackptr-(cnt*4)));
				break;
			}
		}
	cnt++;
	}
break;

} //switch

return 0;
}

//2)create a call that stores desired r0-r7 reg (value held) into SP
//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 1 = not swapped (direct rw)
//	/ 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending
u32 __attribute__ ((hot)) stmiavirt(u8 * input_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped, u8 order){ 

//bit(regs);
//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order
int cnt=0;
int offset=0;

switch(byteswapped){
case(0): 	//SPECIFICALLY DONE FOR CPUBACKUP/RESTORE. 
			//don't use it FOR ANYTHING ELSE.
			
	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
			
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){
				
				if ((order&1) == 0){
					*((u32*)stackptr+(cnt))= (* ((u32*)input_buf + offset)); //linear reads (each register is set on their corresponding offset from the buffer)
					//printf(" stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ offset)); );
				}
				else{
					*((u32*)stackptr-(cnt))= (* ((u32*)input_buf - offset)); 
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){		
			if ((order&1) == 0){
				//Ok but direct writes
				//*(((u32*)stackptr)+(offset))=rom;
				//printf("saved rom: %x -> stack",rom); //if rom overwrites this is why
			
				rom=(* ((u32*)input_buf + offset));
				break;
			}
			else{
				rom=(* ((u32*)input_buf - offset));
				break;
			}
		}
		
	cnt++;
	}
break;

case(1): //for STMIA/LDMIA virtual opcodes (if you save / restore all working registers full asc/desc this will not work for reg[0xf] PC)
	while(cnt<16) {
	//drainwrite();

		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
			//printf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			
				if ((order&1) == 0){
					//printf("stmia:%d[%x] \n", offset,*((u32*)input_buf+(offset)));
					*((u16*)stackptr+(cnt))= *((u16*)input_buf+(offset));
				}
				else{
					*((u16*)stackptr-(cnt))= *((u16*)input_buf-(offset));
				}
			}	
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//printf("stmia:%d[%x]", offset,*(((u32*)input_buf)+(cnt)));
					*((u32*)stackptr+(offset))= (* ((u32*)input_buf+ cnt));
				}
				else{
					*((u32*)stackptr-(offset))= (* ((u32*)input_buf- cnt));
				}
			}
			
			offset++;
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				*((u32*)stackptr+offset)=rom;
				//rom=(u32)*((u32*)input_buf+(offset));
				//printf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
			}
			else{
				*((u32*)stackptr-offset)=rom;
				//rom=(u32)*((u32*)input_buf+(offset));
				//printf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
			}
		}
	cnt++;
	}
break;

//EXCLUSIVE FOR STACK (PUSH-POP stack<mode>_curr framepointer MANAGEMENT)
case(2): 	
	while(cnt<16) {
	
		if((regs>>cnt) & 0x1){
		//printf("%x ",cnt);
		
		if (access == (u8)8){
		}
		
		else if (access == (u8)16){
		}
		
		else if (access == (u8)32){
			
			if ((order&1) == 0){
				*((u32*)stackptr+(offset)) = (* ((u32*)input_buf+ cnt));	//reads from n register offset into full ascending stack
				//printf(" (2)stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ cnt)) ); // stmia:  rn value = *(level_addsrc[0] + offset)
			}
			else{
				*((u32*)stackptr-(offset)) = (* ((u32*)input_buf- cnt));
			}
		}
			
		offset++;
		}
	cnt++;
	}
break;

//EXCLUSIVE FOR STACK (STMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
case(3): 	
	while(cnt<16) {
		if( ((regs>>cnt) & 0x1) && (cnt!=15)){
		//printf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){ 
				if ((order&1) == 0){
				
					//OK writes but direct access
					//*((u32*)stackptr+(offset))= *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) );	//reads from register offset[reg n] from [level1] addr into full ascending stack
					//printf(" stmia%x:srcvalue[%x]",cnt, *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) ) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
			
					//handled writes is GBA CPU layer specific:
					cpuwrite_word((u32)(stackptr+(offset*4)), *((u32*)input_buf+cnt) );
					//printf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
				}
				else{
					//handled writes is GBA CPU layer specific:
					cpuwrite_word((u32)(stackptr-(offset*4)), *((u32*)input_buf-cnt) );
					//printf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				//Ok but direct writes
				//*(((u32*)stackptr)+(offset))=rom;
				//printf("saved rom: %x -> stack",rom); //if rom overwrites this is why
		
				//handled writes is GBA CPU layer specific:
				cpuwrite_word((u32)(stackptr+(offset*4)), rom );
				//printf("read value:%x",cpuread_word(stackptr+(offset*4)));
				break;
			}
			else{
				//handled writes is GBA CPU layer specific:
				cpuwrite_word((u32)(stackptr-(offset*4)), rom );
				//printf("read value:%x",cpuread_word(stackptr+(offset*4)));
				break;
			}
		}
	cnt++;	//offset for non-linear register stacking
	}
break;

} //switch

return 0;
}

//fast single ldr / str opcodes for virtual environment gbaregs

u32 __attribute__ ((hot)) faststr(u8 * input_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
switch(byteswapped){
	//0: gbavirtreg handles.
	case(0):{
		if (access == (u8)8){
			switch(regs){
				case(0xf):
					rom=(* ((u8*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u8*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
		else if (access == (u8)16){
			switch(regs){
				case(0xf):
					rom=(* ((u16*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u16*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
		else if (access == (u8)32){
			
			switch(regs){
				case(0xf):
					rom=(* ((u32*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u32*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
	}
	break;
}

return 0;
}
//gbaregs[] arg 2
u32 __attribute__ ((hot)) fastldr(u8 * output_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
switch(byteswapped){
	//0: gbavirtreg handles.
	case(0):{
		if (access == (u8)8){
			switch(regs){
				case(0xf):
					*((u8*)output_buf+(0))=rom;
				break;
				
				default:
					*((u8*)output_buf+(0))= gbareg[regs];
					//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
		else if (access == (u8)16){
				switch(regs){
				case(0xf):
					*((u16*)output_buf+(0))=rom;
				break;
				
				default:
					*((u16*)output_buf+(0))= gbareg[regs];
					//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
		else if (access == (u8)32){
		
			switch(regs){
				case(0xf):
					*((u32*)output_buf+(0))=rom;
				break;
				
				default:
					*((u32*)output_buf+(0))= gbareg[regs] ;
					//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
	break;
	}
}
return 0;
}

//u32 stackframepointer / u32 #Imm to be added to stackframepointer
 
//3)create a call that add from SP address #Imm, so SP is updated /* detect CPSR so each stack has its stack pointer updated */
u32 addspvirt(u32 stackptr,int ammount){
	
	//sanitize stack top depending on gbastckmodeadr_curr (base stack ptr depending on CPSR mode)
	stackptr=(u32)updatestackfp((u32*)stackptr, (u32*)gbastckmodeadr_curr);
	
	//update new stack fp size
	stackptr=addsasm((int)stackptr,ammount);
	
	//printf("%x",stackptr);
	
	return (u32)stackptr;
}

//4)create a call that sub from SP address #Imm, so SP is updated
u32 subspvirt(u32 stackptr,int ammount){
	
	//sanitize stack top depending on gbastckmodeadr_curr (base stack ptr depending on CPSR mode)
	stackptr=(u32)updatestackfp((u32*)stackptr, (u32*)gbastckmodeadr_curr);

	//update new stack fp size
	stackptr=subsasm((int)stackptr,ammount);
	
	//printf("%x",stackptr);
	
	return (u32)stackptr;
}

//(CPUinterrupts)
//software interrupts service (GBA BIOS calls!)
u32 __attribute__ ((hot)) swi_virt(u32 swinum){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(swinum){
	case(0x0):
		//swi #0! : SoftReset
		#ifdef DEBUGEMU
		printf("bios_softreset()!");
		#endif
		bios_cpureset();
	break;
	
	case(0x1):
		//swi #1! BIOS_RegisterRamReset
		#ifdef DEBUGEMU
		printf("bios_registerramreset()!");
		#endif
		bios_registerramreset(0xFFFF);
	break;
	
	case(0x2):
		//swi #2! Halt
		#ifdef DEBUGEMU
		printf("bios_halt()!");
		#endif
		bios_cpuhalt();
	break;
	
	case(0x3):
		//swi #3! Stop/Sleep
		#ifdef DEBUGEMU
		printf("bios_stop/sleep()!");
		#endif
		bios_stopsleep();
	break;
	
	case(0x4):
		//swi #4!
	break;
	
	case(0x5):
		//swi #5!
	break;
	
	case(0x6):
		//swi #6! Div
		#ifdef DEBUGEMU
		printf("bios_div()!");
		#endif
		bios_div();
	break;
	
	case(0x7):
		//swi #7!
		#ifdef DEBUGEMU
		printf("bios_divarm()!");
		#endif
		bios_divarm();
	break;
	
	case(0x8):
		//swi #8!
		#ifdef DEBUGEMU
		printf("bios_sqrt()!");
		#endif
		bios_sqrt();
	break;
	
	case(0x9):
		//swi #9!
		#ifdef DEBUGEMU
		printf("bios_arctan()!");
		#endif
		bios_arctan();
	break;
		
	case(0xa):
		#ifdef DEBUGEMU
		printf("bios_arctan2()!");
		#endif
		bios_arctan2();
	break;
	
	case(0xb):
		#ifdef DEBUGEMU
		printf("bios_cpuset()!");
		#endif
		bios_cpuset();
	break;
	
	case(0xc):
		#ifdef DEBUGEMU
		printf("bios_cpuset()!");
		#endif
		bios_cpufastset();
	break;
	
	case(0xd):
		#ifdef DEBUGEMU
		printf("bios_getbioschecksum()!");
		#endif
		bios_getbioschecksum();
	break;
	
	case(0xe):
		#ifdef DEBUGEMU
		printf("bios_bgaffineset()!");
		#endif
		bios_bgaffineset();
	break;
	
	case(0xf):
		#ifdef DEBUGEMU
		printf("bios_objaffineset()!");
		#endif
		bios_objaffineset();
	break;
	
	//swi #10! bit unpack
	case(0x10):
		#ifdef DEBUGEMU
		printf("bios_bitunpack()!");
		#endif
		bios_bitunpack();
	break;
	
	case(0x11):
		#ifdef DEBUGEMU
		printf("bios_lz77uncompwram()!");
		#endif
		bios_lz77uncompwram();
	break;

	case(0x12):
		#ifdef DEBUGEMU
		printf("bios_lz77uncompvram()!");
		#endif
		bios_lz77uncompvram();
	break;
	
	case(0x13):
		#ifdef DEBUGEMU
		printf("bios_huffuncomp()!");
		#endif
		bios_huffuncomp();
	break;
	
	case(0x14):
		#ifdef DEBUGEMU
		printf("bios_rluncompwram()!");
		#endif
		bios_rluncompwram();
	break;
	
	case(0x15):
		#ifdef DEBUGEMU
		printf("bios_rluncompvram()!");
		#endif
		bios_rluncompvram();
	break;
	
	case(0x16):
		#ifdef DEBUGEMU
		printf("bios_diff8bitunfilterwram()!");
		#endif
		bios_diff8bitunfilterwram();
	break;
	
	case(0x17):
		#ifdef DEBUGEMU
		printf("bios_diff8bitunfiltervram()!");
		#endif
		bios_diff8bitunfiltervram();
	break;
	
	case(0x18):
		#ifdef DEBUGEMU
		printf("bios_diff16bitunfilter()!");
		#endif
		bios_diff16bitunfilter();
	break;
	
	case(0x1f):
		#ifdef DEBUGEMU
		printf("bios_midikey2freq()!");
		#endif
		bios_midikey2freq();
	break;
	
	case(0x2a):
		#ifdef DEBUGEMU
		printf("bios_snddriverjmptablecopy()!");
		#endif
		bios_snddriverjmptablecopy();
	break;
	
	default:
	break;
}
return 0;
}

//GBA Interrupts
//4000200h - IE - Interrupt Enable Register (R/W)
//  Bit   Expl.
//  0     LCD V-Blank                    (0=Disable)
//  1     LCD H-Blank                    (etc.)
//  2     LCD V-Counter Match            (etc.)
//  3     Timer 0 Overflow               (etc.)
//  4     Timer 1 Overflow               (etc.)
//  5     Timer 2 Overflow               (etc.)
//  6     Timer 3 Overflow               (etc.)
//  7     Serial Communication           (etc.)
//  8     DMA 0                          (etc.)
//  9     DMA 1                          (etc.)
//  10    DMA 2                          (etc.)
//  11    DMA 3                          (etc.)
//  12    Keypad                         (etc.)
//  13    Game Pak (external IRQ source) (etc.)


void vblank_thread(){

//browsefile();
//printf("vblank thread \n");

			if(keysPressed() & KEY_START){


///////////////////////////////////opcode test

//tempbuffer[0x1]=(u32)call_adrstack[0x3]; //r1 branch thumb opcode

//swp r0,r1,[r2]

//tempbuffer[0x0]=0xc00ffee0; //patched bios address
//tempbuffer[0x0]=0xc00ffeee;
tempbuffer[0x0]=(u32)&gbabios[0];
//tempbuffer[0x1]=0xc0ca;
//tempbuffer[0x1]=0x0;
//tempbuffer[0x1]=(u32)&tempbuffer2[0];
tempbuffer[0x1]=0x3;

//tempbuffer[0x2]=(u32)(u32*)rom;

//tempbuffer[0x2]=(u32)&tempbuffer2[0];
//tempbuffer[0x2]=0xff;
//tempbuffer[0x2]=(u32)&tempbuffer2[0]; //COFFE1 SHOULD BE AT tempbuf2[0]

tempbuffer2[0]=0x0; //AND COFFEE 2 SHOULD BE AT R0

tempbuffer[0x3]=0xc33ffee3;
//tempbuffer[0x3]=(u32)(u32*)rom;

//tempbuffer[0x3]=(u32)&tempbuffer2[0x0];

//tempbuffer2[1]=0xcac0c0ca;

//tempbuffer[0x3]=(u32)&tempbuffer[0xc];
//tempbuffer[0x3]=(u32)(u32*)rom;
tempbuffer[0x4]=0xc00ffee4;
//tempbuffer[0x4]=0xc0c0caca;
//tempbuffer[0x4]=(u32)(u32*)rom;
tempbuffer[0x5]=0xc00ffee5;

//tempbuffer[0x6]=-56;
tempbuffer[0x6]=0xc00ffee6; //for some reason ldrsb does good with sign bit 2^8 values, but -128 is Z flag enabled  (+127 / -128) 
//tempbuffer[0x6]=(u32)(u32*)rom;

tempbuffer[0x7]=0xc00ffee7;
tempbuffer[0x8]=0xc00ffee8;
tempbuffer[0x9]=0xc00ffee9;
tempbuffer[0xa]=0xc00ffeea;
tempbuffer[0xb]=0xc00ffeeb;
tempbuffer[0xc]=0xc00ffeec;

//*((u32*)gbastckmodeadr_curr+0)=0xfedcba98;

//stack ptr
//tempbuffer[0xd]=(u32)(u32*)gbastckmodeadr_curr;
tempbuffer[0xd]=(u32)(u32*)gbastckmodeadr_curr;

//tempbuffer[0xd]=0xc00ffeed;

tempbuffer[0xe]=0xc070eeee;
tempbuffer[0xf]=rom;

//c07000-1:  usr/sys  _ c17100-6: fiq _ c27200-1: irq _ c37300-1: svc _ c47400-1: abt _ c57500-1: und

gbavirtreg_r13usr[0]=0xc07000;
gbavirtreg_r14usr[0]=0xc07001;

gbavirtreg_r13fiq[0]=0xc17100;
gbavirtreg_r14fiq[0]=0xc17101;
gbavirtreg_fiq[0x0]=0xc171002;
gbavirtreg_fiq[0x1]=0xc171003;
gbavirtreg_fiq[0x2]=0xc171004;
gbavirtreg_fiq[0x3]=0xc171005;
gbavirtreg_fiq[0x4]=0xc171006;

gbavirtreg_r13irq[0]=0xc27200;
gbavirtreg_r14irq[0]=0xc27201;

gbavirtreg_r13svc[0]=0xc37300;
gbavirtreg_r14svc[0]=0xc37301;

gbavirtreg_r13abt[0]=0xc47400;
gbavirtreg_r14abt[0]=0xc47401;

gbavirtreg_r13und[0]=0xc57500;
gbavirtreg_r14und[0]=0xc57501;

//tempbuffer[0x0]=0xc0700000;
//tempbuffer[0x1]=0xc0700001;
//tempbuffer[0x2]=0xc0700002;
//tempbuffer[0x3]=0xc0700003;
//tempbuffer[0x4]=0xc0700004;
//tempbuffer[0x5]=0xc0700005;
//tempbuffer[0x6]=0xc0700006;
//tempbuffer[0x7]=0xc0700007;
//tempbuffer[0x8]=0xc0700008;
//tempbuffer[0x9]=0xc0700009;
//tempbuffer[0xa]=0xc070000a;
//tempbuffer[0xb]=0xc070000b;
//tempbuffer[0xc]=0xc070000c;
//tempbuffer[0xd]=0xc070000d;
//tempbuffer[0xe]=0xc070000e;


//writing to (0x7fff for stmiavirt opcode) destroys PC , be aware that buffer_input[0xf] slot should be filled before rewriting PC.
//0 for CPURES/BACKUP / 2 for stack/push pop / 1 free
stmiavirt( ((u8*)tempbuffer), (u32)gbavirtreg_cpu, 0x00ff, 32, 0, 0);

//stack pointer test
/*
printf("old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xb07f); //add sp,#508

printf("new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

printf("old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);


disthumbcode(0xb0ff); //add sp,#508

printf("new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/


// PUSH / POP REG TEST OK
/*
printf(" 1/2 old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr); 

disthumbcode(0xb4ff);  //push r0-r7

//clean registers
for(i=0;i<16;i++){
	*((u32*)gbavirtreg_cpu[0]+(i))=0x0;
}
printf(" 1/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

printf(" 2/2 old sfp:%x ",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xbcff); //pop r0-r7

printf(" 2/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/

// PUSH r0-r7,LR / POP r0,-r7,PC	REG TEST OK
/*
printf(" 1/2 old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xb5ff);  //push r0-r7,LR

//clean registers
for(i=0;i<16;i++){
	*((u32*)gbavirtreg_cpu[0]+(i))=0x0;
}
rom=0x0;

printf(" 1/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

printf(" 2/2 old sfp:%x ",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xbdff); //pop r0-r7,PC

printf(" 2/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/

/* //single stack save/restore gbareg 5.11
//backup r3 (5.11)
disthumbcode(0x9302); //str r3,[sp,#0x8]

dummyreg2=0;
faststr((u8*)&dummyreg2, (u32)gbavirtreg_cpu[0], (0x3), 32,0);

//restore (5.11)
disthumbcode(0x9b02); //ldr r3,[sp,#0x8]
*/

//*(u32*)gbastckfpadr_curr++;

//reads: u8 = 1 byte depth / index / 16u = 2 / 32u = 4


/*
//stmia / ldmia test 5.15
disthumbcode(0xc3ff); // stmia r3!,{rdlist}

//clean registers

for(i=0;i<16;i++){
	if(i!=0x2 )
		gbavirtreg_cpu[i]=0x0;

}

disthumbcode(0xcaff); // ldmia r2!,{rdlist}
//disthumbcode(0xcbff); // ldmia r3!,{rdlist} //r3 doesnt work as the address is incremented, ldmia requires same of as stmia
*/


//disthumbcode(0x4708); //bx r1 (branch to ARM mode)
//disthumbcode(0x4709); //bx r1 (branch to thumb mode)
//disthumbcode(0x472d); //bx r5 (branch to thumb code)


//branch test
//disthumbcode(0x2901); 	//cmp r1,#0x1
//disthumbcode(0xd080);		//beq #0x100


//disthumbcode(0xdf01);	//swi #1
// 0xfffffffc = thumb 0x80 lsl r0,r0[0xffffffff],#2


//disthumbcode(0xf1e5); //bl 1/2
//disthumbcode(0xf88c); //bl 2/2
//disthumbcode(0xaf02);

//push-pop test (thumb)
/*disthumbcode(0xb5f0); //push r4-r7,lr
for(i=0;i<16;i++){
	if(i!=0x2 )
		gbavirtreg_cpu[i]=0x0;
}
disthumbcode(0xbdf0); //pop {r4-r7,pc}
*/

//disthumbcode(0xb5f0);

//disthumbcode(0xf1e5f88c); //poksapp bl thumb


//disthumbcode(0xf1e5); // bl 1 / 2
//disthumbcode(0xf88c); // bl 2 / 2
//thumb test
			//0x80 lsl r0,r0[8],#2  / 0x880 lsr r0,r0[0x20],#2 / 0x1080 asr r0,r0[0xffffffff], #3 (5.1) hw CPSR OK!
			//0x188b add r3,r1,r2 / 0x1a8b sub r3,r1[0x10],r2[0xc4] / 0x1dcb add r3,r1[0x10],#7 / 0x1fcb sub r3,r1[0x10],#0x7 (5.2) hw CPSR OK!
			//0x23fe mov r3,#0xfe / 0x2901 cmp r1,#0x1 / 0x3120 add r1,#0x20 / 0x3c01 sub r4,#1 = 0xffffffff (5.3) hw CPSR OK!
			//0x400a and r2,r1   /0x404a eor r2,r1 / 0x408a lsl r2,r1 / 0x40da lsr r2,r3 / 0x4111 asr r1,r2 / 0x4151 adc r1,r2 / 0x4191 sbc r1,r2 / 0x41d1 ror r1,r2 / 0x4211 tst r1,r2 / 0x424a neg r2,r1 / 0x4291 cmp r1,r2 / 0x42d1 cmn r1,r2 / 0x4311 orr r2,r2,r1 / 0x4351 mul r1,r1,r2 / 0x4391 bic r1,r1,r2 / 0x43d1 mvn r1,r2  (5.4) hw CPSR OK!
			//0x4478 add r0,r15 / 0x449c add ip(r12),r3 / 0x44f6 add r14,r14 /0x456e cmp rd, hs / 0x459e cmp hd, rs / 0x45f7 cmp pc,lr / 0x4678 mov r1,pc / 0x4687 mov pc,r0  / 0x46f7 mov hd,hs / 0x4708 bx r1 (arm) / 0x4709 (thumb) / 0x4770 bx lr (arm) / 0x4771 bx lr (thumb) / (5.5) hw CPSR OK!. pending branch opcodes because ARM disassembler is missing..!
			//0x4820 ldr r0,[pc,#0x20] (5.6)  hw CPSR OK!.
			//0x5158 str r0,[r3,r5] / 0x5558 strb r0,[r3,r5] / 0x58f8 ldr r0,[r7,r3] / 0x5d58 ldrb r0,[r3,r5]  (5.7) hw CPSR OK!
			//0x5358 strh r0,[r5,r3] / 0x5758 ldrh r0,[r3,r5] / 0x5b58 ldsb r0,[r3,r5] / 0x5f58 ldsh r0,[r3,r5] (5.8) hw CPSR OK!
			//0x6118 str r0,[r3,#0x10]  / 0x6858 ldr r0,[r3,#0x4] / 0x7058 strb r0,[r3,#0x4] /0x7858 ldrb r0,[r3,#0x4] (5.9) OK
			//0x8098 strh r0,[r3,#0x4] / 0x8898 ldrh r0,[r3,#0x8] (5.10) 
			//0x9302 str r3,[sp,#0x8] / 0x9b02 ldr r3,[sp,#0x8] (5.11) OK
			//0xaf02 add r0,[sp,#0x8] / 0xa002 add r0,[pc,#0x8] (5.12) OK
			
			//0xb01a add sp,#imm / 0xb09a subs sp,#imm(5.13) OK
			//b4ff push r0-r7 / 0xb5ff push r0-r7,LR / 0xbcff pop r0-r7 / 0xbdff pop r0-r7,PC OK
			//0xc3ff stmia r3!, {r0-r7} / 0xcfff ldmia rb!,{r0-r7} (5.15)
			
			//(5.16)
			//0xd0ff BEQ label / 0xd1ff BNE label / 0xd2ff BCS label / 0xd3ff BCC label 
			//0xd4ff BMI label / 0xd5ff BPL label / 0xd6ff BVS label / 0xd7ff BVC label
			//0xd8ff BHI label / 0xd9ff BLS label / 0xdaff BGE label / 0xdbff BLT label
			//0xdcff BGT label / 0xddff BLE label
			
			//0xdfff swi #value8 (5.17)
			//0xe7ff b(al) PC-relative area (5.18)
			//0xf7ff 1/2 long branch
			//0xffff 2/2 long branch (5.19)
//////////////////////////////////////////////////////////////////////////////////////////////////
//ARM opcode test

//other conditional
//z_flag=0;
//disarmcode(0x00810002); //addeq r0,r1,r2
//disarmcode(0x10810002); //addne r0,r1,r2
//disarmcode(0x05910000); //ldreq r0,[r1]
//disarmcode(0x05810000); //streq r0,[r1]

//5.2 BRANCH CONDITIONAL
//z_flag=1;
//disarmcode(0x0afffffe); //beq label
//disarmcode(0x1afffffd); //bne label
//disarmcode(0x2afffffc); //bcs label
//disarmcode(0x3afffffb); //bcc label
//disarmcode(0x4afffffa); //bmi label
//disarmcode(0x5afffff9); //bpl label
//disarmcode(0x6afffff8); //bvs label
//disarmcode(0x7afffff7); //bvc label
//disarmcode(0x8afffff6); //bhi label
//disarmcode(0x9afffff5); //bls label
//disarmcode(0xaafffff4); //bge label
//disarmcode(0xbafffff3); //blt label
//disarmcode(0xcafffff2); //bgt label
//disarmcode(0xdafffff1); //ble label
//disarmcode(0xeafffff0); //b label

//5.3 BRANCH WITH LINK
//z_flag=1;
//disarmcode(0x012fff31); //blxeq r1
//disarmcode(0x112fff31); //blxne r1
//disarmcode(0xe12fff31); //blx r1

//disarmcode(0xe12fff11); //bx r1 pokesapp

//5.4
//AND 
//#Imm with rotate
//disarmcode(0xe0000001); //and r0,r0,r1
//disarmcode(0xe20004ff); //and r0,r0,#0xff000000

//shift with either #imm or register
//disarmcode(0xe0011082); //and r1,r1,r2,lsl #0x1
//disarmcode(0xe0011412);  //and r1,r2,lsl r4
//disarmcode(0xe0011a12); //and r1,r2,lsl r10

//disarmcode(0xe00110a2); //and r1,r2,lsr #1
//disarmcode(0xe0011a32);	//and r1,r2,lsr r10
//disarmcode(0xe00110c2);	//and r1,r2,asr #1
//disarmcode(0xe0011a52);	//and r1,r2,asr r10
//disarmcode(0xe00110e2); //and r1,r2,ror #1
//disarmcode(0xe0011a72); //and r1,r2,ror r10

//EOR
//disarmcode(0xe2211001);  //eor r1,#1
//disarmcode(0xe0211002);  //eor r1,r2

//disarmcode(0xe0211082); //eor r1,r2,lsl #1
//disarmcode(0xe0211a12); //eor r1,r2,lsl r10
//disarmcode(0xe02110a2); //eor r1,r2,lsr #1
//disarmcode(0xe0211a32); //eor r1,r2,lsr r10
//disarmcode(0xe02110c2); //eor r1,r2,asr #1
//disarmcode(0xe0211a52); //eor r1,r2,asr r10
//disarmcode(0xe02110e2); //eor r1,r2,ror #1
//disarmcode(0xe0211a72); //eor r1,r2,ror r10

//SUB
//disarmcode(0xe2411001); //sub r1,#1
//disarmcode(0xe0411002); //sub r1,r2
//disarmcode(0xe0411082);	//sub r1,r2,lsl #1
//disarmcode(0xe0411a12); //sub r1,r2,lsl r10
//disarmcode(0xe04110a2); //sub r1,r2,lsr #1
//disarmcode(0xe0411a32); //sub r1,r2,lsr r10
//disarmcode(0xe04110c2); //sub r1,r2,asr #1
//disarmcode(0xe0411a52); //sub r1,r2,asr r10
//disarmcode(0xe04110e2); //sub r1,r2,ror #1
//disarmcode(0xe0411a72); //sub r1,r2,ror sl

//RSB
//disarmcode(0xe2611001); //rsb r1,#1
//disarmcode(0xe0611002); //rsb r1,r2
//disarmcode(0xe0611082); //rsb r1,r2,lsl #1
//disarmcode(0xe0611a12); //rsb r1,r2,lsl r10
//disarmcode(0xe06110a2);	//rsb r1,r2,lsr #1
//disarmcode(0xe0611a32);	//rsb r1,r2,lsr r10
//disarmcode(0xe06110c2); //rsb r1,r2, asr #1
//disarmcode(0xe0611a52); //rsb r1,r2, asr r10
//disarmcode(0xe06110e2);	//rsb r1,r2,ror #1
//disarmcode(0xe0611a72);	//rsb r1,r2,ror r10

//ADD
//disarmcode(0xe2811001); //add r1,#1
//disarmcode(0xe0811002); //add r1,r2
//disarmcode(0xe0811082); //add r1,r2,lsl #1
//disarmcode(0xe0811a12); //add r1,r2,lsl r10
//disarmcode(0xe08110a2); //add r1,r2,lsr #1 
//disarmcode(0xe0811a32); //add r1,r2,lsr r10
//disarmcode(0xe08110c2); //add r1,r2,asr #1
//disarmcode(0xe0811a52); //add r1,r2,asr r10
//disarmcode(0xe08110e2); //add r1,r2,ror #1
//disarmcode(0xe0811a72); //add r1,r2,ror r10
//disarmcode(0xe28f0018);	//add r0,=0x08000240

//disarmcode(0xe5810000); //str r0,[r1]

//ADC
//disarmcode(0xe2a11001); //adc r1,#1
//disarmcode(0xe0a11002); //adc r1,r2
//disarmcode(0xe0a11082); //adc r1,r2,lsl #1
//disarmcode(0xe0a11a12); //adc r1,r2,lsl r10
//disarmcode(0xe0a110a2); //adc r1,r2,lsr #1
//disarmcode(0xe0a11a32); //adc r1,r2,lsr r10
//disarmcode(0xe0a110c2); //adc r1,r2,asr #1
//disarmcode(0xe0a11a52); //adc r1,r2,asr r10
//disarmcode(0xe0a110e2); //adc r1,r2,ror #1
//disarmcode(0xe0a11a72); //adc r1,r2,ror r10

//SBC
//disarmcode(0xe2c11001); //sbc r1,#1
//disarmcode(0xe0c11002); //sbc r1,r2
//disarmcode(0xe0c11082); //sbc r1,r2,lsl #1
//disarmcode(0xe0c11a12); //sbc r1,r2,lsl r10
//disarmcode(0xe0c110a2); //sbc r1,r2,lsr #1
//disarmcode(0xe0c11a32); //sbc r1,r2,lsr r10
//disarmcode(0xe0c110c2); //sbc r1,r2,asr #1
//disarmcode(0xe0c11a52); //sbc r1,r2,asr r10
//disarmcode(0xe0c110e2); //sbc r1,r2,ror #1
//disarmcode(0xe0c11a72); //sbc r1,r2,ror r10

//RSC
//disarmcode(0xe2e11001); //rsc r1,#1
//disarmcode(0xe0e11002); //rsc r1,r2
//disarmcode(0xe0e11082); //rsc r1,r2,lsl #1
//disarmcode(0xe0e11a12); //rsc r1,r2,lsl r10
//disarmcode(0xe0e110a2); //rsc r1,r2,lsr #1
//disarmcode(0xe0e11a32); //rsc r1,r2,lsr r10
//disarmcode(0xe0e110c2); //rsc r1,r2,asr #1
//disarmcode(0xe0e11a52); //rsc r1,r2,asr r10
//disarmcode(0xe0e110e2); //rsc r1,r2,ror #1
//disarmcode(0xe0e11a72); //rsc r1,r2,ror r10

//TST
//disarmcode(0xe3110001); //tst r1,#1
//disarmcode(0xe1110002); //tst r1,r2
//disarmcode(0xe1110082); //tst r1,r2,lsl #1
//disarmcode(0xe1110a12); //tst r1,r2,lsl r10
//disarmcode(0xe11100a2); //tst r1,r2,lsr #1
//disarmcode(0xe1110a32); //tst r1,r2,lsr r10
//disarmcode(0xe11100c2); //tst r1,r2,asr #1
//disarmcode(0xe1110a52); //tst r1,r2,asr r10
//disarmcode(0xe11100e2); //tst r1,r2,ror #1
//disarmcode(0xe1110a72); //tst r1,r2,ror r10

//TEQ
//disarmcode(0xe3310001); //teq r1,#1
//disarmcode(0xe1310002); //teq r1,r2
//disarmcode(0xe1310082); //teq r1,r2,lsl #1
//disarmcode(0xe1310a12); //teq r1,r2, lsl r10
//disarmcode(0xe13100a2); //teq r1,r2,lsr #1
//disarmcode(0xe1310a32); //teq r1,r2,lsr r10
//disarmcode(0xe13100c2); //teq r1,r2,asr #1
//disarmcode(0xe1310a52); //teq r1,r2,asr r10
//disarmcode(0xe13100e2); //teq r1,r2,ror #1
//disarmcode(0xe1310a72); //teq r1,r2,ror r10

//CMP
//disarmcode(0xe3510001); //cmp r1,#1
//disarmcode(0xe1510002); //cmp r1,r2
//disarmcode(0xe1510082); //cmp r1,r2, lsl #1
//disarmcode(0xe1510a12); //cmp r1,r2,lsl r10
//disarmcode(0xe15100a2); //cmp r1,r2, lsr #1
//disarmcode(0xe1510a32); //cmp r1,r2, lsr r10
//disarmcode(0xe15100c2); //cmp r1,r2, asr #1
//disarmcode(0xe1510a52); //cmp r1,r2,asr r10
//disarmcode(0xe15100e2); //cmp r1,r2,ror #1
//disarmcode(0xe1510a72); //cmp r1,r2,ror r10

//CMN
//disarmcode(0xe3710001); //cmn r1,#1
//disarmcode(0xe1710002); //cmn r1,r2
//disarmcode(0xe1710082); //cmn r1,r2,lsl #1
//disarmcode(0xe1710a12); //cmn r1,r2, lsl r10
//disarmcode(0xe17100a2); //cmn r1,r2, lsr #1
//disarmcode(0xe1720a32); //cmn r1,r2,lsr r10
//disarmcode(0xe17100c2); //cmn r1,r2,asr #1
//disarmcode(0xe1710a52); //cmn r1,r2, asr r10
//disarmcode(0xe17100e2); //cmn r1,r2, ror #1
//disarmcode(0xe1710a72); //cmn r1,r2, ror r10

//ORR
//disarmcode(0xe3811001); //orr r1,#1
//disarmcode(0xe1811002); //orr r1,r2 
//disarmcode(0xe1811082); //orr r1,r2,lsl #1
//disarmcode(0xe1811a12); //orr r1,r2,lsl r10
//disarmcode(0xe18110a2); //orr r1,r2,lsr #1
//disarmcode(0xe1811a32); //orr r1,r2,lsr r10
//disarmcode(0xe18110c2); //orr r1,r2,asr #1
//disarmcode(0xe1811a52); //orr r1,r2,asr r10
//disarmcode(0xe18110e2); //orr r1,r2,ror #1
//disarmcode(0xe1811a72); //orr r1,r2,ror r10

//MOV
//disarmcode(0xe3a01001); //mov r1,#1
//disarmcode(0xe1a01002); //mov r1,r2
//disarmcode(0xe1b01082); //movs r1,r1,lsl #1
//disarmcode(0xe1b01a12); //movs r1,r1,lsl r10
//disarmcode(0xe1b010a2); //movs r1,r1,lsr #1
//disarmcode(0xe1b01a32); //movs r1,r1,lsr r10
//disarmcode(0xe1b010c2); //movs r1,r1,asr #1
//disarmcode(0xe1b01a52); //movs r1,r1,asr r10
//disarmcode(0xe1b010e2); //movs r1,r1,ror #1
//disarmcode(0xe1b01a72); //movs r1,r1,ror r10
//disarmcode(0xe1a0e00f); //mov r14,r15 (poke sapp)


//MVN
//disarmcode(0xe3e01001); //mvn r1,#1
//disarmcode(0xe1e01002); //mvn r1,r2
//disarmcode(0xe1e01082); //mvn r1,r2,lsl #1
//disarmcode(0xe1e01a12); //mvn r1,r2,lsl r10
//disarmcode(0xe1e010a2); //mvn r1,r2, lsr #1
//disarmcode(0xe1e01a32); //mvn r1,r2, lsr r10
//disarmcode(0xe1e010c2); //mvn r1,r2, asr #1
//disarmcode(0xe1e01a52); //mvn r1,r2, asr r10
//disarmcode(0xe1e010e2); //mvn r1,r2, ror #1
//disarmcode(0xe1e01a72); //mvn r1,r2,ror r10


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//disarmcode(0xe19000f1); 		//ldrsh r0,[r0,r1]
//disarmcode(0xe59f2064);		//ldr r2,[pc, #100]
//disarmcode(0xe3a01000);		//mov r1,#0
//disarmcode(0xe3b01000);		//bad opcode mov r1,#0 with S bit set

//disarmcode(0xe1b00801);	//lsl r0,r1,#0x10
//disarmcode(e1b00211);	//lsl r0,r1,r2

//disarmcode(0xe24000ff);		//sub r0,r0,#0xff
//disarmcode(0xe0400001);	//sub r0,r0,r1
//disarmcode(0xe27000ff); //rsbs r0,#0xff
//disarmcode(0xe0700001); //rsbs r0,r1
//disarmcode(0xe28004ff);		//add r0,#0xff000000
//disarmcode(0xe0800001); //add r0,r1
//disarmcode(0xe2a00333); //adc r0,#0xcc000000
//disarmcode(0xe0a00001);	//adc r0,r1
//disarmcode(0xe2d004dd);	//sbc r0,#0xdd000000
//disarmcode(0xe0d00001);	//sbc r0,r1
//disarmcode(0xe2e004dd);	//rsc r0,#0xdd000000
//disarmcode(0xe0e00001);	//rsc r0,r1
//disarmcode(0xe31004dd);	//tst r0,#0xdd000000
//disarmcode(0xe1100001);	//tst r0,r1
//disarmcode(0xe33004bb); //teq r0,#0xbb000000
//disarmcode(0xe1300001);	//teq r0,r1
//disarmcode(0xe35004aa);	//cmp r0,0xaa000000
//disarmcode(0xe1500001);	//cmp r0,r1
//disarmcode(0xe37004aa); 	//cmn r0,#0xaa000000
//disarmcode(0xe1700001);		//cmn r0,r1
//disarmcode(0xe38004aa); //orr r0,#0xaa000000
//disarmcode(0xe1800001); //orr r0,r1
//disarmcode(0xe3a004aa); //mov r0,#0xaa000000
//disarmcode(0xe1a00001); //mov r0,r1
//disarmcode(0xe3c004aa); //bic r0,#0xaa000000
//disarmcode(0xe1c00001); //bic r0,r1
//disarmcode(0xe3e004aa); //mvn r0,#0xaa000000
//disarmcode(0xe1e00001); //mvn r0,r1
//disarmcode(0xe1b00801); //lsl r0,r1,#0x10
//disarmcode(0xe1b00211); //lsl r0,r1,r2

//cpsr test
//disarmcode(0xe10f0000); //mrs r0,CPSR
//cpsrvirt=0;
//disarmcode(0xe129f000); //msr CPSR_fc, r0

//spsr test
//disarmcode(0xe14f0000); //mrs r0,spsr
//spsr_last=0;
//disarmcode(0xe169f000); //msr spsr,r0

//5.5
//CPSR
//disarmcode(0xe10f0000); //mrs r0,CPSR
//disarmcode(0xe129f000); //msr CPSR_fc, r0
//disarmcode(0xe329f20f); //msr CPSR_fc, #0xf0000000
//disarmcode(0xf328f004); //msr cpsr,#imm transfer to psr
//disarmcode(0xf128f00f); //msr cpsr,pc transfer to psr
//disarmcode(0xe129f000); //pokesapp

//SPSR
//disarmcode(0xe14f0000); //mrs r0,spsr
//disarmcode(0xe169f000); //msr spsr,r0
//disarmcode(0xe369f20f); //msr spsr,#0xf0000000
//disarmcode(0xf368f004); //msr spsr,#imm transfer to psr
//disarmcode(0xf168f00f); //msr spsr,pc transfer to psr

//5.6 MUL
//disarmcode(0xe00a0592); 	//mul r10,r2,r5 (no cpsr flags)
//disarmcode(0xe01a0592);		//mul r10,r2,r5 (cpsr enabled flag)

//disarmcode(0xe02a5192); //mla r10,r2,r1,r5 (rd = (rs*rn)+rm)
//untested but unused?
//disarmcode(0xe0100292); //muls r0,r2,r2
//disarmcode(0xb0020293); //mullt r2,r3,r2
//disarmcode(0x70388396); //mlasvc r8,r6,r3,r8
//disarmcode(0xe04a0592); //purposely broken mul r10,r2,r5

//5.7 STR/LDR
//disarmcode(0xe7b21004); //ldr r1,[r2,r4]!
//disarmcode(0xe7a21004); //str r1,[r2,r4]!
//disarmcode(0xe59f1150);	//ldr r1,=0x3007ffc (pokémon sapp compiler) (pc+0x150)
//disarmcode(0xe59f1148);	//ldr r1,=0x8000381

//disarmcode(0xe59f103c);	//ldr r1,=0x3007ffc (own compiler)


//disarmcode(0xe6921004); //ldr r1,[r2],r4
//disarmcode(0xe5821010); //str r1,[r2,#0x10]
//disarmcode(0xe7821103); //str r1,[r2,r3,lsl #2]
//disarmcode(0x05861005); //streq r1,[r6,#5]

//disarmcode(0xe6821004); //ldr r1,[r2,#0x10]
//disarmcode(0xe7921103); //ldr r1,[r2,r3,lsl #2]
//disarmcode(0x05961005); //ldreq r1,[r6,#0x5]
//disarmcode(0xe59f5040); //ldr r5,[pc, #0x40]

//5.8 LDM/STM

 //test ldm/stm 5.8 (requires (u32)&gbabios[0] on r0 first)
/*
disarmcode(0xe880ffff); //stm r0,{r0-r15}

//clean registers
for(i=0;i<16;i++){
	if(i!=0x0 )
		gbavirtreg_cpu[i]=0x0;

}
gbavirtreg_cpu[0x0]=(u32)&gbabios[0];
rom=0;
disarmcode(0xe890ffff); //ldm r0,{r0-r15}
*/

//5.8 ldm/stm with PC

//disarmcode(0xe92d8000);//stm r13!,{pc}
//rom=0x0;
//disarmcode(0xe97d8000);	//ldm sp!,{pc}^ //pre-indexed (sub)


//without PSR
//disarmcode(0xe880ffff); //stm r0,{r0-r15}
//disarmcode(0xe890ffff); //ldm r0,{r0-r15}

//with PSR
//disarmcode(0xe880ffff); //stm r0,{r0-r15}
//disarmcode(0xe8d0ffff); //ldm r0,{r0-r15}^

//stack test
/*
disarmcode(0xe92d0007);		//push {r0,r1,r2} (writeback enabled and post-bit enabled / add offset base)
disarmcode(0xe93d0007);		//pop {r0,r1,r2} (writeback enabled and pre-bit enabled // subs offset bast)
*/
//disarmcode(0xe8bd8000);	//pop {pc}

//disarmcode(0xe92d8000);//stm r13!,{pc}

//disarmcode(0xe8fd8000);	//ldm sp!,{pc}^ //post-indexed
//disarmcode(0xe9fd8000);	//ldm sp!,{pc}^ //pre-indexed (add)
//	disarmcode(0xe97d8000);		//ldm sp!,{pc}^ //pre-indexed (sub)

//disarmcode(0xe96d7fff);	//stmdb sp!,{r0-r14}^
//disarmcode(0xe82d400f);	//stmda sp!,{r0-r3,lr}

//disarmcode(0xe59f5048);	//ldr r5,[pc,#72]
//disarmcode(0xe8956013);	//ldm r5,{r0,r1,r4,sp,lr}
//disarmcode(0xe9bd800f);	//ldmib sp!,{r0,r1,r2,r3,pc}

//5.9 SWP
//disarmcode(0xe1020091); 	//swp r0,r1,[r2]
//disarmcode(0xe1420091);		//swpb r0,r1,[r2]
//disarmcode(0x01010090);	//swpeq r0,r0,[r1]

//5.10 swi / svc call
//z_flag=1;
//c_flag=0;
//disarmcode(0xef000010);	    //swi 0x10

//5.15 undefined
//disarmcode(0xe755553e);

			//other things!
			//printf("\x1b[0;0H sbrk(0) is at: %x ",(u32)sbrk(0));
			//debugDump();
			
			//psg noise test printf("pitch: %x \n",(u32)((*(buffer_input+0)>>16)&0xffff));printf("pitch2: %x \n",(u32)((*(buffer_input+0))&0xffff));printf("                         \n");
			//printf("timer:%d status:%x \n",0,(u32)TMXCNT_LR(0));

			//gbastack test u8
			/*
			for(i=0;i<8;i++){
				stru8asm((u32)&gbastack,i,i); //gbastack[i] = 0xc070;				
				//printf("%x \n",(u32)gbastack[i]); //printf("%x \n",(u32)**(&gbastack+i*2)); //non contiguous memory  //printf("%x \n",(u32)ldru16asm((u32)&gbastack,i*2));

				if(ldru8asm((u32)&gbastack,i)!=i){
					printf("failed writing @ %x \n",(u32)&gbastack+(i));
					while(1);
				}
	
				printf("%x ",(u32)ldru8asm((u32)&gbastack,i));
	
				stru8asm((u32)&gbastack,i,0x0);
			}
			*/

			//load store test
			//disthumbcode(0x683c);
			
			//printf("squareroot:%d",(int)sqrtasm(10000)); //%e double :p
			}
			
			
			if(keysPressed() & KEY_A){
			//need to fed timerX value to timer, will react on overflow and if timerX is started through cnt
			//TMXCNT_LW(u8 TMXCNT,int TVAL)
			//u16 TMXCNT_HW(u8 TMXCNT, u8 prescaler,u8 countup,u8 status) 
				//TMXCNT_LW(0,300);
				//TMXCNT_HW(0, 0,0,1);
			
			//swi call vector
			//swicaller(0x0); #number
			
			//debug stack (hardware chunk BIG stack for each CPU mode)
			/*
			//you must detect stack mode: for gbastackptr
			
			printf("\n gbastack @ %x",(u32)(u32*)gbastackptr);
			printf("\n //////contents//////: \n");
			
			for(i=0;i<16;i+=4){
			printf(" %d:[%x] ",i,(u32)ldru32asm((u32)&gbastack,i));
			//printf("%x \n",(u32)gbastack[i]);
			//printf("%x \n",(u32)gbastack[i+1]);
			
			if (i==9) printf("\n");
			
			}
			*/
			
			/* //tempbuffer1 
			printf("\n    tempbuffer @ %x",(u32)&tempbuffer);
			printf("\n ///////////contents/////////: \n");
			
			for(i=0;i<24;i+=4){
			printf(" %d[%x] ",i,(u32)ldru32asm((u32)tempbuffer,i));
			
			if (i==12) printf("\n");
			
			}
			*/
			
			// 
			//printf("\n /// GBABIOS @ %x //",(unsigned int)(u8*)gba.bios);
			
			/*
			for(i=0;i<16;i++){
			printf(" %d:[%x] ",i,(unsigned int)*((u32*)gba.bios+i));//ldru32asm((u32)tempbuffer2,i));
			
				if (i==15) printf("\n");
			
			}
			*/
			
			int cntr=0;
			
			/*
			printf("rom contents @ 0x08000000 \n");
			for(cntr=0;cntr<4;cntr++){
			printf(" %d:[%x] ",cntr,(unsigned int)cpuread_word(0x08000000+(cntr*4)));//ldru32asm((u32)tempbuffer2,i));
			
				if (cntr==15) printf("\n");
			
			}
			*/
			printf("\n hardware r0-r15 stack (GBA) [MODE");
			
			if ((cpsrvirt&0x1f) == (0x10) || (cpsrvirt&0x1f) == (0x1f))
				printf(" USR/SYS STACK]");
			else if ((cpsrvirt&0x1f)==(0x11))
				printf(" FIQ STACK]");
			else if ((cpsrvirt&0x1f)==(0x12))
				printf(" IRQ STACK]");
			else if ((cpsrvirt&0x1f)==(0x13))
				printf(" SVC STACK]");
			else if ((cpsrvirt&0x1f)==(0x17))
				printf(" ABT STACK]");
			else if ((cpsrvirt&0x1f)==(0x1b))
				printf(" UND STACK]");
			else
				printf(" STACK LOAD ERROR] CPSR: %x -> psr:%x",(unsigned int)cpsrvirt,(unsigned int)(cpsrvirt&0x1f));
				
			printf("@ %x",(unsigned int)gbastckmodeadr_curr); //base sp cpu <mode>
			printf("\n curr_fp:%x ",(unsigned int)gbastckfpadr_curr); //fp sp cpu <mode>
			printf("\n //////contents//////: \n");
			
			for(cntr=0;cntr<16;cntr++){
				//printf(" r%d :[%x] ",cntr,(unsigned int)ldru32asm((u32)&gbavirtreg_cpu,cntr*4)); //byteswap reads
				if (cntr!=0xf) 
				printf(" r%d :[0x%x] ",cntr, (unsigned int)gbavirtreg_cpu[cntr]);
				else printf(" PC(0x%x)",  (unsigned int)rom); //works: (u32)&rom  // &gbavirtreg_usr[0xf] OK
			}
			
			printf("\n CPSR[%x] / SPSR:[%x] \n",(unsigned int)cpsrvirt,(unsigned int)spsr_last);
			printf("CPUvirtrunning:");
			if (gba.cpustate==true) printf("true:");
			else printf("false");
			printf("/ CPUmode:");
			if (armstate==1) printf("THUMB");
			else printf("ARM \n");
			
			printf("\n CPUtotalticks:(%d) / gba.lcdticks:(%d) / vcount: (%d)",(int)cputotalticks,(int)gba.lcdticks,(int)gba.GBAVCOUNT);
			
			} //a button
			
			
			if(keysPressed() & KEY_B){
			printf("hi");
			
			//for testing stmiavirt
			/*
			for(i=0;i<16;i++){
			
				printf(" [%c] ",(unsigned int)*((u32*)&gbavirtreg_cpu+i));
				if (i==15) printf("\n"); //sizeof(tempbuffer[0]) == 1 byte (u8)
			}
			*/
			
			//for testing tempbuffer
			/*
			for(i=0;i<16;i++){
			
			stru32asm( (u32)&tempbuffer, i*(4/(sizeof(tempbuffer[0]))) ,0xc0700000+i);
			
			//stru32asm((u32)&gbavirtreg_cpu, i*4,ldru32asm((u32)&tempbuffer,i*4));
			
			}
			
			//clean tempbuffer
			for(i=0;i<64;i++){
			tempbuffer[i]=0;
			}
			
			ldmiavirt((u8*)&tempbuffer, (u32)&gbavirtreg_cpu, 0xffff, 32, 1); //read using normal reading method
			
			for(i=0;i<16;i++){
				printf(" [%x] ",(unsigned int)ldru32asm((u32)&tempbuffer,i*4));
				
				if (i==15) printf("\n"); //sizeof(tempbuffer[0]) == 1 byte (u8)
			}
			*/
			
			//ldmiavirt((u8*)&tempbuffer, (u32)(u32*)gbastackptr, 0xf, 32, 0); //read using byteswap (recomended for stack management)
			
			//for two arguments
			//gbachunk=(u32)(0x08000000+ (rand() % romsize));
			//printf("\n gbarom(%x):[%x] ",(unsigned int)gbachunk,(unsigned int)ldru32inlasm((int)gbachunk));
			
			//for zero arguments, just arg passing
			//gbachunk=ldru32inlasm((int)(0x08000000+ ( (rand() % 0x0204)& 0xfffe )));
			//printf("romread:[%x] \n",(unsigned int)gbachunk);
			
			/*
			TMXCNT_HW(0, 0,0,0);
			printf("dtcm contents: \n");
			for(i=0;i<64;i++){
				i2=0x027C0000;
				i2+=i;
				printf("%08x ",(unsigned int)ldru32inlasm(i2));
			}
			printf("\n");
			*/
			
			//psg noise testiprintf("pitch2: %d \n",(*(buffer_input+0))&0xffff);temp5--;
		
		
		//load store test
		//disthumbcode(0x692a); //ldr r2,[r5,#0x10]
		//disthumbcode(0x603a); //str r2,[r7,#0]
		
			//tested
			//0x008b lsl rd,rs,#Imm (5.1)
			//0x1841 adds rd,rs,rn / 0x1a41 subs rd,rs,rn / #Imm (5.2)
			//0x3c01 subs r4,#1 (5.3) 
			//0x40da lsrs rd,rs (5.4)
			//0x4770 bx lr / 0x449c add ip,r3 (5.5)
			//0x4e23 LDR RD,[PC,#Imm] (5.6)
			//0x5158 str r0,[r3,r5] / 0x58f8 ldr r0,[r7,r3] (5.7)
			//0x5e94 ldsh r0,[r3,r4] (5.8)
			//0x69aa ldr r2,[r5,#24] / 0x601a str r2,[r3,#0] (5.9)
			//0x87DB ldrh r3,[r2,#3] // 0x87DB strh r3,[r2,#3] (5.10)
			//0x97FF str rd,[sp,#Imm] / 0x9FFF ldr rd,[sp,#Imm] (5.11)
			//0xa2e1 rd,[pc,#Imm] / 0xaae1 add rd,[sp,#Imm] (5.12)
			//0xb01a add sp,#imm / 0xb09a add sp,#-imm(5.13)
			//0xb4ff push {rlist} / 0xb5ff push {Rlist,LR} / 0xbcff pop {Rlist} / 0xbdff pop {Rlist,PC}
			//0xc7ff stmia rb!,{rlist} / 0xcfff ldmia rb!,{rlist} (5.15)
			
			//(5.16)
			//0xd0ff BEQ label / 0xd1ff BNE label / 0xd2ff BCS label / 0xd3ff BCC label 
			//0xd4ff BMI label / 0xd5ff BPL label / 0xd6ff BVS label / 0xd7ff BVC label
			//0xd8ff BHI label / 0xd9ff BLS label / 0xdaff BGE label / 0xdbff BLT label
			//0xdcff BGT label / 0xddff BLE label
			
			//0xdfff swi #value8 (5.17)
			//0xe7ff b(al) PC-relative area (5.18)
			//0xf7ff 1/2 long branch
			//0xffff 2/2 long branch (5.19)
			
			//stack for virtual environments
			/*
			printf("\n 	branchstack[%x]:[%x]",(unsigned int)&branch_stack[0],(unsigned int)(u32*)(branch_stackfp));
			printf("\n ///////////contents/////////: \n");
			
			for(i=0;i<16;i++){
			printf(" %d:[%x] ",i,(unsigned int)ldru32asm((u32)(u32*)(branch_stackfp),i*4));
			
			if (i==12) printf("\n");
			
			}
			*/
			
			}
			
			if(keysPressed() & KEY_X){
			}
			
			if(keysPressed() & KEY_UP){
				/*
				printf("\n");
				printf("readu32:(%d) \n",i2);
				printf("%08x ",(unsigned int)ichfly_readu32(i2));
				i2*=4;
				*/
			
			//rom=(u32)(0x08000000+ (rand() %  (romsize-0x11))); //causes undefined opcode if romsize unset ( overflow )
			
			//load store test
			//disthumbcode(0x6932); //ldr r2,[r6,#0x10]
		
			printf("\n");	
			
			//addspvirt((u32)(u32*)gbastackptr,0x4);
			
			//psg noise test printf("volume: %d\n",*(buffer_input+1));temp4++;
			//sbrk(0x800);
			}
			
			if(keysPressed() & KEY_DOWN){

			//for(i=0;i<255;i++){
			//*(gbastackptr+i)=0;
			//}
			
			gbavirtreg_cpu[0xf]=rom=(u32)rom_entrypoint;
			
			//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
			updatecpuflags(1,cpsrvirt,0x12);
			
			//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
			updatecpuflags(1,cpsrvirt,0x10);

			
				//set CPU <mode>
				//updatecpuflags(1,0x0,0x10);
			
			//load store test
			//tempbuffer[0]=0x0;
			//tempbuffer[1]=0x00000000;
			//tempbuffer[2]=(u32)&tempbuffer[2]; //address will be rewritten with gbaread / r2 value
			//stmiavirt( ((u8*)&tempbuffer[0]), (u32)gbavirtreg_cpu[0], 0xe0, 32, 1);
	
			
			/* VBAM stats
			printf("\n *********************");
			printf("\n emu stats: ");
			printf("\n N_FLAG(%x) C_FLAG(%x) Z_FLAG(%x) V_FLAG(%x) \n",(unsigned int)gba.N_FLAG,(unsigned int)gba.C_FLAG,(unsigned int)gba.Z_FLAG,(unsigned int)gba.V_FLAG);
			printf("\n armstate(%x) frameskip(%x)  \n",(unsigned int)gba.armState,(unsigned int)gba.frameSkip);
			printf("\n DISPCNT(%x) DISPSTAT(%x) VCOUNT(%x)",(unsigned int)gba.GBADISPCNT,(unsigned int)gba.GBADISPSTAT,(unsigned int)gba.GBAVCOUNT);
			printf("\n IE(%lu) IF(%x) IME(%x) EMULATING:(%x)",(unsigned int)gba.IE,(unsigned int)gba.IF,(unsigned int)gba.IME,(unsigned int)gba.emulating);
			printf("\n clockTicks(%x) cpuTotalTicks(%x) LCDTicks(%x)",(unsigned int)gba.clockTicks,(unsigned int)gba.cpuTotalTicks,(unsigned int)gba.lcdTicks);
			printf("\n frameCount(%x) count(%x) romSize(%x)",(unsigned int)gba.frameCount,(unsigned int)gba.count,(unsigned int)gba.romSize);
			*/
			
			//sbrk(-0x800);
			//psg noise test printf("volume: %d\n",*(buffer_input+1));temp4--;
			
			/*
			printf("\n");
				printf("readu32:(%d) \n",(i2*4));
				printf("%08x ",(unsigned int)ichfly_readu32(i2*4));
				i2/=4;
			*/
			//subspvirt((u32)(u32*)gbastackptr,0x4);
			
			}
				
			if(keysPressed() & KEY_LEFT){
			
			//printf("words found! (%d) \n",extract_word((int*)minigsf,"fade",55,output)); //return 0 if invalid, return n if words copied to *buf
			//for(i=0;i<10;i++) printf("%x ",output[i]);
			//TMXCNT_L(1,0xF0F0F0F0);
			//TMXCNT_M(timernum,div,cntup,enable,start)
			//TMXCNT_H(1,1,0,1,1);
			//rs_sappy_bin_size
			
			/* //fifo test 
			recvfifo(buffer_input,FIFO_SEMAPHORE_FLAGS);
			i=buffer_input[15];
			i+=0x10;
			if(i<0) i=0;
			else if (i>0x100000) i=0x100000;
			
			printf("\n value: %x",(unsigned int)i);
			
			buffer_output[0]=i;
			sendfifo(buffer_output);
			*/
			
			//branch_stackfp=cpubackupmode((u32*)branch_stackfp,gbavirtreg_cpu,cpsrvirt);
			//printf("branch fp :%x \n",(unsigned int)(u32*)branch_stackfp);
			
			//psg noise test printf("pitch: %d\n",(*(buffer_input+0)>>16)&0xffff);temp3++;
			
			}
			
			if(keysPressed() & KEY_RIGHT){
			
			/* //fifo test 
			
			recvfifo(buffer_input,FIFO_SEMAPHORE_FLAGS);
			i=buffer_input[15];
			i-=0x10;
			if(i<0) i=0;
			else if (i>0x100000) i=0x100000;
			
			printf("\n value: %x",(unsigned int)i);
			
			buffer_output[0]=i;
			sendfifo(buffer_output);
			*/
			
			//branch_stackfp=cpurestoremode((u32*)branch_stackfp,&gbavirtreg_cpu[0]);
			//printf("branch fp :%x \n",(unsigned int)(u32*)branch_stackfp);

			//psg noise test printf("pitch: %d\n",(*(buffer_input+0)>>16)&0xffff);temp3--;
			}
			
			
			if(keysPressed() & KEY_SELECT){ //ZERO
			
			//disthumbcode(0xb081); //add sp,#-0x4
			
			//swi GBAREAD test OK / BROKEN with libnds swi code
			/*
			gbachunk=(u32)(0x08000000+(rand() % 0xfffffe));
			printf("\n acs: (%x)",(unsigned int)gbachunk);
			swicaller(gbachunk);
			printf("=> (%x)",(unsigned int)gbachunk);
			*/
			
			//stress test (load/store through assembly inline)
			//for ewram 0x02000000 access
			//CPUWriteHalfWord(&gba, (u32) ( 0x02000000 + (rand() % 0x3FFFe) ) , (u16) rand() % 0xffff ); //OK
			
			//for Iram 03000000-03007FFF access
			//CPUWriteHalfWord(&gba, (u32) ( 0x03000000 + (rand() % 0x7FFE) ) , (u16) rand() % 0xffff ); //OK
			
			//for PaletteRAM 05000000-050003FF access
			//CPUWriteHalfWord(&gba, (u32) ( 0x05000000 + ((rand() % 0x3FE) & 0x3ff) ) , (u16) rand() % 0xffff ); //OK
			
			//for VRAM 06000000-06017FFF
			//CPUWriteHalfWord(&gba, (u32) ( 0x06000000 + ((rand() % 0x17FFF) & 0x17FFF) ) , (u16) rand() % 0xffff ); //OK
			
			//for OAM 07000000-070003FF
			//CPUWriteHalfWord(&gba, (u32) ( 0x07000000 + (rand() % 0x3FF) ) , (u16) rand() % 0xffff ); 
			
			//data abort GBAREAD test OK
			
			//rom
			//printf("rom pointer (%x)",(unsigned int)(u8*)rom);
			//entrypoint
			//printf("\n rom entrypoint: %x",(unsigned int)(u32*)rom_entrypoint);
			
			//gbaread latest
			//gbachunk=ldru32inlasm(0x08000000+ (rand() % romsize));
			//printf("romread:[%x] \n",(unsigned int)gbachunk);
			
			
			//stack cpu backup / restore test
			/*
			u32 * branchfpbu;
			
			branchfpbu=branch_stackfp;
			
			branchfpbu=cpubackupmode((u32*)(branchfpbu),&gbavirtreg_cpu[0],cpsrvirt);
			
			stmiavirt((u8*)&cpsrvirt, (u32)(u32*)branchfpbu, 1 << 0x0, 32, 1);
			
			branchfpbu=(u32*)addspvirt((u32)(u32*)branchfpbu,1);
			
			printf("old cpsr: %x",(unsigned int)cpsrvirt);
			
			//flush workreg
			cpsrvirt=0;
			for(i=0;i<0x10;i++){
				*((u32*)gbavirtreg_cpu[0]+i)=0x0;
			}
			
			
			branchfpbu=(u32*)subspvirt((u32)(u32*)branchfpbu,1);
			
			ldmiavirt((u8*)&cpsrvirt, (u32)(u32*)branchfpbu, 1 << 0x0, 32, 1);
			
			printf("restored cpsr: %x",(unsigned int)cpsrvirt);
			
			branchfpbu=cpurestoremode((u32*)(branchfpbu),&gbavirtreg_cpu[0]);
			
			updatecpuflags(1,cpsrvirt,cpsrvirt&0x1f);
			*/
			
			
			//branch stack test
			//branch_stackfp=branch_test(branch_stackfp);
			//printf("\n branch fp ofs:%x",(unsigned int)(u32*)branch_stackfp);
			
			//printf("size branchtable: 0x%d",(int)gba_branch_table_size); //68 bytes each
			
			//printf("bytes per branchblock: 0x%d",(int)gba_branch_block_size); //68 bytes each
			
			//stmiavirt((u8*)&tempbuffer, (u32)(u32*)gbastackptr, 0xf, 32, 0); //for storing to stack
			
			//printf("offset:%x per each block size:%x",(unsigned int)ofset,(int)gba_branch_block_size);
			emulatorgba();
			}
			
			
			if(keysPressed() & KEY_L){
			
			//DO NOT MODIFY CPSR WHEN CHANGING CPU MODES.
				updatecpuflags(1,cpsrvirt,0x12); //swap CPU-stack to usermode
				//printf("switch to 0x10");
			}
			
			if(keysPressed() & KEY_R){
				updatecpuflags(1,cpsrvirt,0x13); //swap stack to sys
				//printf("switch to 0x11");
				u32 tempaddr=((0x08000000)+(rand()&0xffff));
				printf("gba:%x:->[%x] \n",(unsigned int)tempaddr,(unsigned int)stream_readu32(tempaddr));
			}
			
			
//psg noise test if (temp3<0) temp3 = 0; else if (temp3>0x500) temp3 = 0x500;
//psg noise test if (temp4<0) temp4 = 0; else if (temp4>70) temp4 = 70;
//psg noise test if (temp5<0) temp5 = 0; else if (temp5>300) temp5 = 300;

//psg noise test *(buffer_output+0)=(temp3<<16)|((temp5<<0)&0xffff); *(buffer_output+1)=temp4;
//psg noise test sendfifo(buffer_output);

//scanKeys();
}

//if fifo recv != empty
void fifo_thread(){
FIFO_SEMAPHORE_FLAGS.REG_FIFO_RECVEMPTY=1<<8;
FIFO_SEMAPHORE_FLAGS.REG_FIFO_ERRSENDRECVEMPTYFULL=1<<14; //0<<14
FIFO_SEMAPHORE_FLAGS.REG_FIFO_SENDEMPTY_STAT=0<<0;
	//receive into *buffer
	if (!( (*(u32*)(0x04000184)) & FIFO_SEMAPHORE_FLAGS.REG_FIFO_RECVEMPTY)){ //if something is in the queue? (recv)
		recvfifo(buffer_input,FIFO_SEMAPHORE_FLAGS);
	}
}

void hblank_thread(){
	//emulatorgba(cpsrvirt); //not yet until it is stable (don't think so)
	cputotalticks++;
}

void vcount_thread(){
	gbacpu_refreshvcount();	//HEALTHY
	gba.GBAVCOUNT++;			//HEALTHY
}

//process saving list
void save_thread(u32 * srambuf){
	//check for u32 * sound_block, u32 freq, and other remaining lists from the block_list emu
}

//GBA Render threads
u32 gbavideorender(){
	*(u32*)(0x04000000)= (0x8030000F) | (1 << 16); //nds DISPCNT
	
	u8 *pointertobild = (u8 *)(0x6820000);
	int iy=0;
	for(iy = 0; iy <160; iy++){
		dmaTransferWord(3, (uint32)pointertobild, (uint32)(0x6200000/*bgGetGfxPtr(bgrouid)*/+512*(iy)), 480);
		pointertobild+=512;
	}
return 0;
}