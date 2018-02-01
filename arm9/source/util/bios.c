// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include <unistd.h>//BRK(); SBRK();
#include <fat.h>
#include <filesystem.h>
#include <dirent.h>

#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>

#include "../pu/supervisor.h"
#include "../pu/pu.h"
#include "opcode.h"
#include "util.h"
#include "buffer.h"
#include "translator.h"

extern struct GBASystem gba;

s16 sinetable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};


//swi 0
u32 bios_cpureset(){
	//refresh jump opcode in biosprotected vector
	gba.biosprotected[0] = 0x00;
	gba.biosprotected[1] = 0xf0;
	gba.biosprotected[2] = 0x29;
	gba.biosprotected[3] = 0xe1;
	
	//set CPU-stack to usermode
	cpsrvirt=0x0;
	updatecpuflags(1,cpsrvirt,0x10);
	
	//flush working CPU registers
	for(i=0;i<0x10;i++){
		gbavirtreg_cpu[i]=0x0;
	}
	gbavirtreg_cpu[0x0]=0;

	// clean caioMem memory
	memset(gba.caioMem, 0, 0x400);
	
	// clean io memory
	memset(gba.iomem, 0, 0x400);
	
	// clean bios memory
	//memset(gba.bios,0, 0x4000);
	
	// clean internal work ram
	memset(gba.intram,0, 0x8000);
	
	// clean palette
	memset(gba.palram, 0, 0x400);
	
	// clean OAM
	memset(gba.oam, 0, 0x400);
	
	// clean vram
	//memset(gba.vidram, 0, 0x20000);	//test later
	
	//gbamap reset
	//struct: map[index].address[(u8*)address] <- (u8*)(u32)&gba.dummysrc
	for(i = 0; i < 256; i++) {
		gba.map[i].address = (u8 *)(u32)gba.dummysrc;
		gba.map[i].mask = 0;
	}
return 0;	
}

//swi 1
u32 bios_registerramreset(u32 flags){
// no need to trace here. this is only called directly from GBA.cpp
// to emulate bios initialization

cpu_updateregisters(0x0, 0x80);	//CPUUpdateRegister(0x0, 0x80);
	
if(flags) {
	if(flags & 0x01) {
		// clear work RAM
		memset(gba.workram, 0, 0x40000);
		//iprintf("do clean wram!");
	}
	if(flags & 0x02) {
		// clear internal RAM
		memset(gba.intram, 0, 0x7e00); // don't clear 0x7e00-0x7fff
		//iprintf("do clean iwram!");
	}
	if(flags & 0x04) {
		// clear palette RAM
		memset(gba.palram, 0, 0x400);
		//iprintf("do clean palram!");
	}
	if(flags & 0x08) {
		// clear VRAM
		//memset(vram, 0, 0x18000);
	}
	if(flags & 0x10) {
		// clean OAM
		memset(gba.oam, 0, 0x400);
		//iprintf("do clean oam!");
	}
	
	if(flags & 0x80) {
		int i;
		for(i = 0; i < 0x10; i++)
			cpu_updateregisters(0x200+(i*2), 0x0);	//CPUUpdateRegister(0x200+i*2, 0);

		for(i = 0; i < 0xF; i++)
			cpu_updateregisters(0x4+(i*2), 0x0);	//CPUUpdateRegister(0x4+i*2, 0);

		for(i = 0; i < 0x20; i++)
			cpu_updateregisters(0x20+(i*2), 0x0);	//CPUUpdateRegister(0x20+(i*2), 0);

		for(i = 0; i < 0x18; i++)
			cpu_updateregisters(0xb0+(i*2), 0x0);	//CPUUpdateRegister(0xb0+(i*2), 0);

		cpu_updateregisters(0x130, 0x0);			//CPUUpdateRegister(0x130, 0);
		cpu_updateregisters(0x20, 0x100);			//CPUUpdateRegister(0x20, 0x100);
		cpu_updateregisters(0x30, 0x100);			//CPUUpdateRegister(0x30, 0x100);
		cpu_updateregisters(0x26, 0x100);			//CPUUpdateRegister(0x26, 0x100);
		cpu_updateregisters(0x36, 0x100);			//CPUUpdateRegister(0x36, 0x100);
		//iprintf("register map 1!");
	}
    
	if(flags & 0x20) {
		int i;
		for(i = 0; i < 8; i++)
			cpu_updateregisters(0x110+(i*2), 0x0);		//CPUUpdateRegister(0x110+i*2, 0);
			cpu_updateregisters(0x134, 0x8000);			//CPUUpdateRegister(0x134, 0x8000);
		for(i = 0; i < 7; i++)
			cpu_updateregisters(0x140+(i*2), 0x0);		//CPUUpdateRegister(0x140+i*2, 0);
	}

	if(flags & 0x40) {
		int i;
		cpuwrite_byte(0x04000084, 0x0);					//CPUWriteByte(0x4000084, 0);
		cpuwrite_byte(0x04000084, 0x80);					//CPUWriteByte(0x4000084, 0x80);
		cpuwrite_word(0x04000080, 0x880e0000);			//CPUWriteMemory(0x4000080, 0x880e0000);
		
		//CPUUpdateRegister(0x88, CPUReadHalfWord(0x04000088)&0x3ff);
		cpu_updateregisters(0x88, (cpuread_hword(0x04000088)&0x3ff));
		
		cpuwrite_byte(0x04000070, 0x70);				//CPUWriteByte(0x4000070, 0x70);
		
		for(i = 0; i < 8; i++)
			cpu_updateregisters(0x90+(i*2), 0x0);		//CPUUpdateRegister(0x90+i*2, 0);
		
		cpuwrite_byte(0x04000070, 0x0);					//CPUWriteByte(0x4000070, 0);
      
		for(i = 0; i < 8; i++)
			cpu_updateregisters(0x90+(i*2), 0x0);		//CPUUpdateRegister(0x90+i*2, 0);
		cpuwrite_byte(0x04000084, 0x0);					//CPUWriteByte(0x4000084, 0);
	//iprintf("register map 2!");
	}
	
}

return 0;
}

//swi 2 (halt not emulated?)
u32 bios_cpuhalt(){
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 		//+r0
gba.cpustate=dummyreg;
faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 		//=r0
return 0;
}


//swi 3
u32 bios_stopsleep(){
bios_cpuhalt();
return 0;
}

//swi 6
u32 bios_div(){
//#ifdef DEV_VERSION
//    log("Div: 0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1]);
//#endif

//int number = exRegs[0];
//int denom = exRegs[1];

fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 		//+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); 		//+r1

int number = dummyreg;
int denom = dummyreg2;

if(denom != 0) {
	dummyreg=number / denom;						//exRegs[0] = number / denom;
	dummyreg2=number % denom;						//exRegs[1] = number % denom;
	s32 temp =	(s32)dummyreg;						//s32 temp = (s32)exRegs[0];
	dummyreg3 = temp < 0 ? (u32)-temp : (u32)temp;//exRegs[3] = temp < 0 ? (u32)-temp : (u32)temp;
	
	faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
	faststr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0);
	faststr((u8*)&dummyreg3, gbavirtreg_cpu, (0x3), 32,0); 	//=r3
}

//#ifdef DEV_VERSION
//    log("Div: return=0x%08x,0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1],
//        exRegs[3]);
//#endif
return 0;
}

//swi 7
u32 bios_divarm(){
	fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 	//r0
	fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); 	//r1
	
	dummyreg3=dummyreg;		//tmp = r0
	
	faststr((u8*)&dummyreg2, gbavirtreg_cpu, (0x0), 32,0);	//r0=r1
	faststr((u8*)&dummyreg3, gbavirtreg_cpu, (0x1), 32,0);	//r1=r0
	
	//u32 temp = exRegs[0];
	//exRegs[0] = exRegs[1];
	//exRegs[1] = temp;
  
	bios_div();
return 0;
}

//swi 8
u32 bios_sqrt(){
	fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
	dummyreg2 = (u16)sqrtasm((int)dummyreg);//(u32)sqrt((double)dummyreg);
	faststr((u8*)&dummyreg2, gbavirtreg_cpu, (0x0), 32,0);
return 0;
}

//swi 9
u32 bios_arctan(){
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0

s32 a =  -( ((s32)(dummyreg*dummyreg)) >> 14);
s32 b = ((0xA9 * a) >> 14) + 0x390;
b = ((b * a) >> 14) + 0x91C;
b = ((b * a) >> 14) + 0xFB6;
b = ((b * a) >> 14) + 0x16AA;
b = ((b * a) >> 14) + 0x2081;
b = ((b * a) >> 14) + 0x3651;
b = ((b * a) >> 14) + 0xA2F9;
a = ((s32)dummyreg * b) >> 16;
dummyreg = a;

faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //=r0
return 0;
}


//swi 0xa
u32 bios_arctan2(){
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1

s32 x = dummyreg;
s32 y = dummyreg2;
u32 res = 0;
if (y == 0) {
	res = ((x>>16) & 0x8000);
} 
else{
	if (x == 0) {
		res = ((y>>16) & 0x8000) + 0x4000;
	} 
	else {
		if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x<0) && (y<0))))) {
			dummyreg2 = x;
			dummyreg = y << 14;
			bios_div();
			bios_arctan();
		if (x < 0)
			res = 0x8000 + dummyreg;
		else
			res = (((y>>16) & 0x8000)<<1) + dummyreg;
		} 
		else {
			dummyreg = x << 14;
			bios_div();
			bios_arctan();
			res = (0x4000 + ((y>>16) & 0x8000)) - dummyreg;
      }
	}
}
dummyreg = res;
faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
return 0;
}

//swi 0xb
u32 bios_cpuset(){

fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1
fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (0x2), 32,0); //+r2

u32 source = dummyreg;
u32 dest = dummyreg2;
u32 cnt = dummyreg3;

if(((source & 0xe000000) == 0) ||
	((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
	return 0;

int count = cnt & 0x1FFFFF;

// 32-bit ?
if((cnt >> 26) & 1) {
	// needed for 32-bit mode!
	source &= 0xFFFFFFFC;
	dest &= 0xFFFFFFFC;
    // fill ?
    if((cnt >> 24) & 1) {
		u32 value = (source>0x0EFFFFFF ? 0x1CAD1CAD : cpuread_word(source)); //CPUReadMemory(source)
		while(count) {
			cpuwrite_word(dest, value);		//CPUWriteMemory(dest, value);
			dest += 4;
			count--;
		}
    } 
	else {
		// copy
		while(count) {
			//CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source)));
			cpuwrite_word(dest, (source>0x0EFFFFFF ? 0x1CAD1CAD : cpuread_word(source)));
			source += 4;
			dest += 4;
			count--;
		}
    }
} 
else {
	// 16-bit fill?
	if((cnt >> 24) & 1) {
		u16 value = (source>0x0EFFFFFF ? 0x1CAD : cpuread_hword(source)); //CPUReadHalfWord(source));
		while(count) {
			cpuwrite_hword(dest, value); //CPUWriteHalfWord(dest, value);
			dest += 2;
			count--;
		}
    } 
	else {
	// copy
		while(count) {
			//CPUWriteHalfWord(dest, (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source)));
			cpuwrite_hword(dest, (source>0x0EFFFFFF ? 0x1CAD : cpuread_hword(source)));
			source += 2;
			dest += 2;
			count--;
		}
    }
}
return 0;
}

//swi 0xc
u32 bios_cpufastset(){

fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1
fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (0x2), 32,0); //+r2

u32 source = dummyreg;
u32 dest = dummyreg2;
u32 cnt = dummyreg3;

if(((source & 0xe000000) == 0) || ((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
	return 0;

	// needed for 32-bit mode!
	source &= 0xFFFFFFFC;
	dest &= 0xFFFFFFFC;
  
	int count = cnt & 0x1FFFFF;
  
	// fill?
	if((cnt >> 24) & 1) {
		while(count > 0) {
			// BIOS always transfers 32 bytes at a time
			u32 value = (source>0x0EFFFFFF ? 0xBAFFFFFB : cpuread_word(source)); //CPUReadMemory(source));
			for(i = 0; i < 8; i++) {
				cpuwrite_word(dest, value);		//CPUWriteMemory(dest, value);
				dest += 4;
			}
			count -= 8;
		}
	}
	else {
		// copy
		while(count > 0) {
			// BIOS always transfers 32 bytes at a time
			for(i = 0; i < 8; i++) {
				//CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0xBAFFFFFB :CPUReadMemory(source)));
				cpuwrite_word(dest, (source>0x0EFFFFFF ? 0xBAFFFFFB :cpuread_word(source)));
				source += 4;
				dest += 4;
			}
		count -= 8;
		}
	}
return 0;
}

//swi 0xd
u32 bios_getbioschecksum(){
	dummyreg=0xBAAE187F;
	faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
return 0;
}

//swi 0xe
u32 bios_bgaffineset(){
//#ifdef DEV_VERSION  
//    log("BgAffineSet: %08x,%08x,%08x\n",
//        exRegs[0],
//        exRegs[1],
//        exRegs[2]);
//#endif

fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1
fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (0x2), 32,0); //+r2

u32 src = dummyreg;
u32 dest = dummyreg2;
int num = dummyreg3;

for(i = 0; i < num; i++) {
	s32 cx = cpuread_word(src);				 //CPUReadMemory(src);
	src+=4;
	s32 cy = cpuread_word(src); 			//CPUReadMemory(src);
	src+=4;
	s16 dispx = cpuread_hword(src);			//CPUReadHalfWord(src);
	src+=2;
	s16 dispy = cpuread_hword(src);			//CPUReadHalfWord(src);
	src+=2;
	s16 rx = cpuread_hword(src);			//CPUReadHalfWord(src);
	src+=2;
	s16 ry = cpuread_hword(src); 			//CPUReadHalfWord(src);
	src+=2;
	u16 theta = (cpuread_hword(src)>>8);	//CPUReadHalfWord(src)>>8;
	src+=4; // keep structure alignment
	s32 a = sinetable[(theta+0x40)&255];
	s32 b = sinetable[theta];

	s16 dx =  (rx * a)>>14;
	s16 dmx = (rx * b)>>14;
	s16 dy =  (ry * b)>>14;
	s16 dmy = (ry * a)>>14;
    
	cpuwrite_hword(dest, dx);	//CPUWriteHalfWord(dest, dx);
	dest += 2;
	cpuwrite_hword(dest, -dmx);	//CPUWriteHalfWord(dest, -dmx);
	dest += 2;
	cpuwrite_hword(dest, dy);	//CPUWriteHalfWord(dest, dy);
	dest += 2;
	cpuwrite_hword(dest, dmy);	//CPUWriteHalfWord(dest, dmy);
	dest += 2;

	s32 startx = cx - dx * dispx + dmx * dispy;
	s32 starty = cy - dy * dispx - dmy * dispy;
    
	cpuwrite_word(dest, startx);	//CPUWriteMemory(dest, startx);
	dest += 4;
	cpuwrite_word(dest, starty);	//CPUWriteMemory(dest, starty);
	dest += 4;
}
return 0;
}

//swi 0xf
u32 bios_objaffineset(){
//#ifdef DEV_VERSION
//    log("ObjAffineSet: 0x%08x,0x%08x,0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1],
//        exRegs[2],
//        exRegs[3]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1
fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (0x2), 32,0); //+r2
fastldr((u8*)&dummyreg4, gbavirtreg_cpu, (0x3), 32,0); //+r3
  
	u32 src = dummyreg; 		//r0
	u32 dest = dummyreg2;		//r1
	int num = dummyreg3;		//r2
	int offset = dummyreg4; 	//r3

	for(i = 0; i < num; i++){
		s16 rx = cpuread_hword(src); //CPUReadHalfWord(src);
		src+=2;
		s16 ry = cpuread_hword(src); //CPUReadHalfWord(src);
		src+=2;
		u16 theta = (cpuread_hword(src)>>8); //CPUReadHalfWord(src)>>8;
		src+=4; // keep structure alignment

		s32 a = (s32)sinetable[(theta+0x40)&255];
		s32 b = (s32)sinetable[theta];

		s16 dx =  ((s32)rx * a)>>14;
		s16 dmx = ((s32)rx * b)>>14;
		s16 dy =  ((s32)ry * b)>>14;
		s16 dmy = ((s32)ry * a)>>14;
    
		cpuwrite_hword(dest, dx); 	//CPUWriteHalfWord(dest, dx);
		dest += offset;
		cpuwrite_hword(dest, -dmx);	//CPUWriteHalfWord(dest, -dmx);
		dest += offset;
		cpuwrite_hword(dest, dy);	//CPUWriteHalfWord(dest, dy);
		dest += offset;
		cpuwrite_hword(dest, dmy);	//CPUWriteHalfWord(dest, dmy);
		dest += offset;
	}
return 0;
}

//swi 0x10
u32 bios_bitunpack(){

u32 source=0; 	//u32 source = exRegs[0];
u32 dest=0;		//u32 dest = exRegs[1];
u32 header=0;	//u32 header = exRegs[2];

fastldr((u8*)&source, gbavirtreg_cpu, (0x0), 32,0); 	//r0
fastldr((u8*)&dest, gbavirtreg_cpu, (0x1), 32,0); 	//r1
fastldr((u8*)&header, gbavirtreg_cpu, (0x2), 32,0); 	//r2

int len=cpuread_hword(header); 			//int len = CPUReadHalfWord(header);

// check address
if(((source & 0xe000000) == 0) || ((source + len) & 0xe000000) == 0)
	return 0;

int bits=cpuread_byte(header+2);		//int bits = CPUReadByte(header+2);
int revbits = 8 - bits; 
// u32 value = 0; //unneeded

u32 base=cpuread_word(header+4);		//u32 base = CPUReadMemory(header+4);

bool addBase = (base & 0x80000000) ? true : false;
base &= 0x7fffffff;
int dataSize=cpuread_byte(header+3);	//int dataSize = CPUReadByte(header+3);

int data = 0; 
int bitwritecount = 0; 
while(1) {
	len -= 1;
	if(len < 0)
		return 0;
		
	int mask = 0xff >> revbits; 
	u8 b=cpuread_byte(source);			//u8 b = CPUReadByte(source); 
	source++;
	int bitcount = 0;
	while(1) {
		if(bitcount >= 8)
			return 0;
			
		u32 d = b & mask;
		u32 temp = d >> bitcount;
			
		if(d || addBase) {
			temp += base;
		}
		data |= temp << bitwritecount;
		bitwritecount += dataSize;
		if(bitwritecount >= 32) {
			cpuwrite_word(dest, data);		//CPUWriteMemory(dest, data);
			dest += 4;
			data = 0;
			bitwritecount = 0;
		}
		mask <<= bits;
		bitcount += bits;
	}
}

return 0;
}

//swi 0x11
u32 bios_lz77uncompwram(){
//#ifdef DEV_VERSION
//    log("LZ77UnCompWram: 0x%08x,0x%08x\n", exRegs[0], exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0);//+r1

u32 source = dummyreg; 	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source);//CPUReadMemory(source);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
int len = header >> 8;

while(len > 0) {
	u8 d = cpuread_byte(source++); //CPUReadByte(source++);

	if(d) {
		for(i = 0; i < 8; i++) {
			if(d & 0x80) {
				u16 data = (cpuread_byte(source++) << 8); //CPUReadByte(source++) << 8;
				data |= cpuread_byte(source++);	//CPUReadByte(source++);
				int length = (data >> 12) + 3;
				int offset = (data & 0x0FFF);
				u32 windowOffset = dest - offset - 1;
				for(i = 0; i < length; i++) {
					cpuwrite_byte(dest++,cpuread_byte(windowOffset++)); //CPUWriteByte(dest++, CPUReadByte(windowOffset++));
					len--;
						if(len == 0)
							return 0;
				}
			} 
			else{
				cpuwrite_byte(dest++,cpuread_byte(source++)); //CPUWriteByte(dest++, CPUReadByte(source++));
				len--;
					if(len == 0)
						return 0;
			}
		d <<= 1;
		}
	} 
	else {
		for(i = 0; i < 8; i++) {
			cpuwrite_byte(dest++,cpuread_byte(source++)); //CPUWriteByte(dest++, CPUReadByte(source++));
			len--;
				if(len == 0)
					return 0;
		}
	}
}
return 0;
}

//swi 0x12
u32 bios_lz77uncompvram(){
//#ifdef DEV_VERSION
//    log("LZ77UnCompVram: 0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1
  
u32 source = dummyreg; 	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source); //CPUReadMemory(source);
source += 4;

if(((source & 0xe000000) == 0) ||((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
int byteCount = 0;
int byteShift = 0;
u32 writeValue = 0;

int len = header >> 8;

while(len > 0) {
	u8 d = cpuread_byte(source++); // CPUReadByte(source++);

	if(d) {
	for(i = 0; i < 8; i++) {
		if(d & 0x80) {
			u16 data = ( cpuread_byte(source++) << 8 ); //CPUReadByte(source++) << 8;
			data |= cpuread_byte(source++);			//CPUReadByte(source++);
			int length = (data >> 12) + 3;
			int offset = (data & 0x0FFF);
			u32 windowOffset = dest + byteCount - offset - 1;
			for(i = 0; i < length; i++) {
				writeValue |= (cpuread_byte(windowOffset++) << byteShift); //(CPUReadByte(windowOffset++) << byteShift);
				byteShift += 8;
				byteCount++;

				if(byteCount == 2) {
					cpuwrite_hword(dest, writeValue);	//CPUWriteHalfWord(dest, writeValue);
					dest += 2;
					byteCount = 0;
					byteShift = 0;
					writeValue = 0;
				}
				len--;
				if(len == 0)
					return 0;
			}
        }
		else {
			writeValue |= (cpuread_byte(source++) << byteShift); //(CPUReadByte(source++) << byteShift);
			byteShift += 8;
			byteCount++;
			
			if(byteCount == 2) {
				cpuwrite_hword(dest, writeValue);		//CPUWriteHalfWord(dest, writeValue);
				dest += 2;
				byteCount = 0;
				byteShift = 0;
				writeValue = 0;
			}
			len--;
			if(len == 0)
				return 0;
		}
        d <<= 1;
	}
    }
	else {
		for(i = 0; i < 8; i++) {
			writeValue |= (cpuread_byte(source++) << byteShift);
			byteShift += 8;
			byteCount++;
				if(byteCount == 2) {
					cpuwrite_hword(dest, writeValue);
					dest += 2;      
					byteShift = 0;
					byteCount = 0;
					writeValue = 0;
				}
			len--;
			if(len == 0)
				return 0;
		}
	}
}
return 0;
}

//swi 0x13
u32 bios_huffuncomp(){
//#ifdef DEV_VERSION
//    log("HuffUnComp: 0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0);

u32 source = dummyreg; //exRegs[0];
u32 dest = dummyreg2; //exRegs[1];

u32 header = cpuread_word(source); //CPUReadMemory(source);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
u8 treeSize = cpuread_byte(source++); //CPUReadByte(source++);

u32 treeStart = source;

source += ((treeSize+1)<<1)-1; // minus because we already skipped one byte
  
int len = header >> 8;

u32 mask = 0x80000000;
u32 data = cpuread_word(source); //CPUReadMemory(source);
source += 4;

int pos = 0;
u8 rootNode = cpuread_byte(treeStart); //CPUReadByte(treeStart);
u8 currentNode = rootNode;
bool writeData = false;
int byteShift = 0;
int byteCount = 0;
u32 writeValue = 0;

if((header & 0x0F) == 8) {
	while(len > 0) {
		// take left
		if(pos == 0)
			pos++;
		else
			pos += (((currentNode & 0x3F)+1)<<1);
      
		if(data & mask) {
			// right
			if(currentNode & 0x40)
				writeData = true;
			currentNode = cpuread_byte(treeStart+pos+1); 	//CPUReadByte(treeStart+pos+1);
		} 
		else {
			// left
			if(currentNode & 0x80)
				writeData = true;
			currentNode = cpuread_byte(treeStart+pos);		//CPUReadByte(treeStart+pos);
		}
      
      if(writeData) {
			writeValue |= (currentNode << byteShift);
			byteCount++;
			byteShift += 8;

			pos = 0;
			currentNode = rootNode;
			writeData = false;

			if(byteCount == 4) {
				byteCount = 0;
				byteShift = 0;
				cpuwrite_word(dest, writeValue);	//CPUWriteMemory(dest, writeValue);
				writeValue = 0;
				dest += 4;
				len -= 4;
			}
		}
		mask >>= 1;
		if(mask == 0) {
			mask = 0x80000000;
			data = cpuread_word(source); //CPUReadMemory(source);
			source += 4;
		}
    }
} 
else {
	int halfLen = 0;
	int value = 0;
		while(len > 0) {
			// take left
			if(pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3F)+1)<<1);

			if((data & mask)) {
				// right
				if(currentNode & 0x40)
					writeData = true;
				currentNode = cpuread_byte(treeStart+pos+1); //CPUReadByte(treeStart+pos+1);
			} 
			else{
				// left
				if(currentNode & 0x80)
					writeData = true;
				currentNode = cpuread_byte(treeStart+pos);		//CPUReadByte(treeStart+pos);
			}
      
			if(writeData) {
				if(halfLen == 0)
					value |= currentNode;
				else
					value |= (currentNode<<4);

				halfLen += 4;
				if(halfLen == 8) {
					writeValue |= (value << byteShift);
					byteCount++;
					byteShift += 8;
				
					halfLen = 0;
					value = 0;

					if(byteCount == 4) {
						byteCount = 0;
						byteShift = 0;
						cpuwrite_word(dest, writeValue);		//CPUWriteMemory(dest, writeValue);
						dest += 4;
						writeValue = 0;
						len -= 4;
					}
				}
				pos = 0;
				currentNode = rootNode;
				writeData = false;
			}
			mask >>= 1;
			if(mask == 0) {
				mask = 0x80000000;
				cpuread_word(source);	//data = CPUReadMemory(source);
				source += 4;
			}
		}   
	}
return 0;
}


//swi 0x14
u32 bios_rluncompwram(){
//#ifdef DEV_VERSION
//    log("RLUnCompWram: 0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1

u32 source = dummyreg; 	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source & 0xFFFFFFFC); //CPUReadMemory(source & 0xFFFFFFFC);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
int len = header >> 8;

while(len > 0) {
	u8 d = cpuread_byte(source++);	//CPUReadByte(source++);
    int l = d & 0x7F;
		if(d & 0x80) {
			u8 data = cpuread_byte(source++);	 //CPUReadByte(source++);
			l += 3;
			for(i = 0;i < l; i++) {
				cpuwrite_byte(dest++, data);	//CPUWriteByte(dest++, data);
				len--;
					if(len == 0)
					return 0;
			}
		} 
		else{
			l++;
			for(i = 0; i < l; i++) {
				cpuwrite_byte(dest++, cpuread_byte(source++));	//CPUWriteByte(dest++,  CPUReadByte(source++));
				len--;
				if(len == 0)
					return 0;
			}
		}
}

return 0;
}

//swi 0x15
u32 bios_rluncompvram(){
//#ifdef DEV_VERSION
//    log("RLUnCompVram: 0x%08x,0x%08x\n",
//        exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); 

u32 source = dummyreg; 	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source & 0xFFFFFFFC);	//CPUReadMemory(source & 0xFFFFFFFC);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
int len = header >> 8;
int byteCount = 0;
int byteShift = 0;
u32 writeValue = 0;

while(len > 0) {
	u8 d = cpuread_byte(source++);	//CPUReadByte(source++);
    int l = d & 0x7F;
	if(d & 0x80) {
		u8 data = cpuread_byte(source++);	//CPUReadByte(source++);
		l += 3;
		for(i = 0;i < l; i++) {
			writeValue |= (data << byteShift);
			byteShift += 8;
			byteCount++;

			if(byteCount == 2) {
				cpuwrite_hword(dest, writeValue);	//CPUWriteHalfWord(dest, writeValue);
				dest += 2;
				byteCount = 0;
				byteShift = 0;
				writeValue = 0;
			}
			len--;
			if(len == 0)
				return 0;
		}
    } 
	else {
		l++;
		for(i = 0; i < l; i++) {
			writeValue |= (cpuread_byte(source++) << byteShift);	//(CPUReadByte(source++) << byteShift);
			byteShift += 8;
			byteCount++;
			if(byteCount == 2) {
				cpuwrite_hword(dest, writeValue);	//CPUWriteHalfWord(dest, writeValue);
				dest += 2;
				byteCount = 0;
				byteShift = 0;
				writeValue = 0;
			}
			len--;
			if(len == 0)
				return 0;
      }
    }
}

return 0;
}

//swi 0x16
u32 bios_diff8bitunfilterwram(){
//#ifdef DEV_VERSION
//    log("Diff8bitUnFilterWram: 0x%08x,0x%08x\n", exRegs[0],
//        exRegs[1]);
//#endif

fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1

u32 source = dummyreg; 	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source);	//CPUReadMemory(source);
source += 4;

if( ((source & 0xe000000) == 0) || (( source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  

int len = header >> 8;

u8 data = cpuread_byte(source++);		//CPUReadByte(source++);
cpuwrite_byte(dest++, data);			//CPUWriteByte(dest++, data);
len--;
	
while(len > 0) {
	u8 diff = cpuread_byte(source++);	//CPUReadByte(source++);
	data += diff;
	cpuwrite_byte(dest++, data);		//CPUWriteByte(dest++, data);
	len--;
}

return 0;
}

//swi 0x17

u32 bios_diff8bitunfiltervram(){
//#ifdef DEV_VERSION
//    log("Diff8bitUnFilterVram: 0x%08x,0x%08x\n", exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); //+r0
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); //+r1

u32 source = dummyreg;	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source);	//CPUReadMemory(source);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
	
int len = header >> 8;

u8 data = cpuread_byte(source++);	//CPUReadByte(source++);
u16 writeData = data;
int shift = 8;
int bytes = 1;
  
while(len >= 2) {
    u8 diff = cpuread_byte(source++);	//CPUReadByte(source++);
    data += diff;
    writeData |= (data << shift);
    bytes++;
    shift += 8;
	if(bytes == 2) {
		cpuwrite_hword(dest, writeData);	//CPUWriteHalfWord(dest, writeData);
		dest += 2;
		len -= 2;
		bytes = 0;
		writeData = 0;
		shift = 0;
    }
}  

return 0;
}


//swi 0x18
u32 bios_diff16bitunfilter(){
//#ifdef DEV_VERSION
//    log("Diff16bitUnFilter: 0x%08x,0x%08x\n", exRegs[0],
//        exRegs[1]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0); 

u32 source = dummyreg;	//exRegs[0];
u32 dest = dummyreg2;	//exRegs[1];

u32 header = cpuread_word(source);	//CPUReadMemory(source);
source += 4;

if(((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
	return 0;  
  
int len = header >> 8;

u16 data = cpuread_hword(source);		//CPUReadHalfWord(source);
source += 2;
cpuwrite_hword(dest, data);				//CPUWriteHalfWord(dest, data);
dest += 2;
len -= 2;
  
while(len >= 2) {
	u16 diff = cpuread_hword(source);	//CPUReadHalfWord(source);
	source += 2;
	data += diff;
	cpuwrite_hword(dest, data);			//CPUWriteHalfWord(dest, data);
	dest += 2;
	len -= 2;
}
return 0;
}

//swi 0x1f
u32 bios_midikey2freq(){
//#ifdef DEV_VERSION
//    log("MidiKey2Freq: WaveData=%08x mk=%08x fp=%08x\n",
//        exRegs[0],
//        exRegs[1],
//        exRegs[2]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);
fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0x1), 32,0);
fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (0x2), 32,0);

int freq = cpuread_word(dummyreg+4);	//CPUReadMemory(exRegs[0]+4);
double tmp;
tmp = ((double)(180 - dummyreg2)) - ((double)dummyreg3 / 256.f);
tmp = pow((double)2.f, tmp / 12.f);
dummyreg = (int)((double)freq / tmp); //save to r0
faststr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0);

//#ifdef DEV_VERSION
//    log("MidiKey2Freq: return %08x\n",
//        exRegs[0]);
//#endif
return 0;
}

//swi 0x2a
u32 bios_snddriverjmptablecopy(){
//#ifdef DEV_VERSION
//    log("SndDriverJmpTableCopy: dest=%08x\n",
//        exRegs[0]);
//#endif
fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0x0), 32,0); 

for(i = 0; i < 0x24; i++) {
	cpuwrite_word(dummyreg, 0x9c);	//CPUWriteMemory(dummyreg, 0x9c);
	dummyreg += 4;
}

return 0;
}