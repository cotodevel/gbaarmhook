#ifndef translatorGBAdefs
#define translatorGBAdefs

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include "util.h"
#include "opcode.h"
#include "pu.h"
#include "supervisor.h"

/*
 GBA Memory Map

General Internal Memory
  00000000-00003FFF   BIOS - System ROM         (16 KBytes)
  00004000-01FFFFFF   Not used
  02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
  02040000-02FFFFFF   Not used
  03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
  03008000-03FFFFFF   Not used
  04000000-040003FE   I/O Registers
  04000400-04FFFFFF   Not used
Internal Display Memory
  05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
  05000400-05FFFFFF   Not used
  06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
  06018000-06FFFFFF   Not used
  07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
  07000400-07FFFFFF   Not used
External Memory (Game Pak)
  08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
  0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
  0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
  0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
  0E010000-0FFFFFFF   Not used
*/


extern struct GBASystem gba;

//address whitelist to be patched
extern u32  __attribute__((section(".dtcm"))) addrpatches[0x10];

//new redirected patched addresses (requires init first after initemu() as pointers are generated there)
extern u32  __attribute__((section(".dtcm"))) addrfixes[0x10];

//CPU flags will live on the DTCM , so they're accessible
extern u32 __attribute__((section("dtcm"))) cpsrasm;	//gba hardware cpsr from asm opcodes
extern u32 __attribute__((section("dtcm"))) cpsrvirt;	//gba virtualized cpsr for environment

extern u8 __attribute__((section("dtcm"))) z_flag;
extern u8 __attribute__((section("dtcm"))) n_flag;
extern u8 __attribute__((section("dtcm"))) c_flag;
extern u8 __attribute__((section("dtcm"))) v_flag;
extern u8 __attribute__((section("dtcm"))) i_flag;
extern u8 __attribute__((section("dtcm"))) f_flag;

//inmediate flag set for ARM opcode (5.4 ARM)
extern u8 __attribute__((section("dtcm"))) immop_arm;
	
//set/alter condition codes for ARM opcode (5.4 ARM)
extern u8 __attribute__((section("dtcm"))) setcond_arm;

	
//SPSR == the last mode Interrupt, Fast interrupt, the old CPU flags (old stack [mode] && old CPU [mode]) 
extern u32 __attribute__((section("dtcm"))) spsr_svc;
extern u32 __attribute__((section("dtcm"))) spsr_irq;
extern u32 __attribute__((section("dtcm"))) spsr_abt;
extern u32 __attribute__((section("dtcm"))) spsr_und;
extern u32 __attribute__((section("dtcm"))) spsr_fiq;
extern u32 __attribute__((section("dtcm"))) spsr_usr;	//well, there's no spsr for user but for compatibility
extern u32 __attribute__((section("dtcm"))) spsr_sys;
extern u32 __attribute__((section("dtcm"))) spsr_last; //this one for any cpu<mode> SPSR handle


extern u8 __attribute__((section("dtcm"))) armstate;	//0 arm / 1 thumb
extern u8 __attribute__((section("dtcm"))) armirqstate;//0 disabled / 1 enabled
extern u8 __attribute__((section("dtcm"))) armswistate;//0 disabled / 1 enabled


#ifdef __cplusplus
extern "C"{
#endif

//u32  addresslookup(u32 srcaddr, u32 blacklist[], u32 whitelist[]);
u32 updatecpuflags(u8 mode, u32 cpsr, u32 cpumode); //updatecpuflags(mode,cpsr,cpumode); mode: 0 = hardware asm cpsr update / 1 = virtual CPU mode change,  CPSR , change to CPU mode

//disassemblers
u32 disthumbcode(u32 thumbinstr);
u32 disarmcode(u32 arminstr);

//CPU virtualize opcodes
u32 * cpubackupmode(u32 * branch_stackfp, u32 cpuregvector[], u32 cpsr);
u32 * cpurestoremode(u32 * branch_stackfp, u32 cpuregvector[]);

//thumb opcodes that require hardware CPSR bits
//5.1
u32 lslasm(u32 x1,u32 y1);
u32 lsrasm(u32 x1,u32 y1);
int asrasm(int x1, u32 y1);
//5.2
u32 addasm(u32 x1,u32 y1);
int addsasm(int x1,int y1); //re-uses addasm bit31 signed

u32 subasm(u32 x1,u32 y1);
int subsasm(int x1,int y1); //re-uses subasm bit31 signed
//5.3
u32 movasm(u32 reg);
u32 cmpasm(u32 x, u32 y); //r0,r1
//add & sub already added
//5.4
u32 andasm(u32 x1,u32 y1);
u32 eorasm(u32 x1,u32 y1);
//lsl, lsr, asr already added
u32 adcasm(u32 x1,u32 y1);
u32 sbcasm(u32 x1,u32 y1);
u32 rorasm(u32 x1,u32 y1);
u32 tstasm(u32 x1,u32 y1);
u32 negasm(u32 x1);
//cmp rd,rs already added
u32 cmnasm(u32 x1,u32 y1);
u32 orrasm(u32 x1,u32 y1);
u32 mulasm(u32 x1,u32 y1); //unsigned multiplier
u32 bicasm(u32 x1,u32 y1);
u32 mvnasm(u32 x1);
//5.5
//add & cmp (low & high reg) already added

//5.8
//ldsb
u32 ldsbasm(u32 x1,u32 y1);
u32 ldshasm(u32 x1,u32 y1);

//ARM opcodes
u32 rscasm(u32 x1,u32 y1);
u32 teqasm(u32 x1,u32 y1);
u32 rsbasm(u32 x1,u32 y1);

//5.6
u32 mlaasm(u32 x1,u32 y1, u32 y2); // z1 = ((x1 * y1) + y2) 
u32 mulsasm(u32 x1,u32 y1); //signed multiplier
u32 multtasm(u32 x1,u32 y1);
u32 mlavcsasm(u32 x1,u32 y1,u32 y2);

//ldr/str
u32 ldru32extasm(u32 x1,u32 y1);
u16 ldru16extasm(u32 x1,u16 y1);
u8	ldru8extasm(u32 x1,u8 y1);

#ifdef __cplusplus
}
#endif

#endif