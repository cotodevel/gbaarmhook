/*

			Copyright (C) 2017  Coto
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
USA

*/

//TGDS required version: IPC Version: 1.3

//IPC FIFO Description: 
//		struct sIPCSharedTGDS * TGDSIPC = TGDSIPCStartAddress; 														// Access to TGDS internal IPC FIFO structure. 		(ipcfifoTGDS.h)
//		struct sIPCSharedTGDSSpecific * TGDSUSERIPC = (struct sIPCSharedTGDSSpecific *)TGDSIPCUserStartAddress;		// Access to TGDS Project (User) IPC FIFO structure	(ipcfifoTGDSUser.h)

#ifndef __ipcfifoTGDSUser_h__
#define __ipcfifoTGDSUser_h__

#include "dsregs.h"
#include "dsregs_asm.h"
#include "typedefsTGDS.h"
#include "ipcfifoTGDS.h"
#include "dswnifi.h"
#include "utilsTGDS.h"

//gba dma fifo
#define INTERNAL_FIFO_SIZE 	(sint32)(16)	//each DMA
#define FIFO_BUFFER_SIZE	(sint32)(4)		//FIFO_A/FIFO_B = 4 Bytes

struct gbaheader_t{
	u32 entryPoint;
	u8 logo[156];
	char title[0xC];
	char gamecode[0x4];
	char makercode[0x2];
	u8 is96h;
	u8 unitcode;
	u8 devicecode;
	u8 unused[7];
	u8 version;
	u8 complement;
	u16 res;
};

typedef struct sIPCSharedTGDSSpecific{
	uint32 frameCounter7;	//VBLANK counter7
	uint32 frameCounter9;	//VBLANK counter9
	uint32 * IPC_ADDR;
    uint8 * ROM;   		//pointer to ROM page
    int rom_size;   	//rom total size
}  IPCSharedTGDSSpecific	__attribute__((aligned (4)));


//project specific IPC. tMyIPC is used by TGDS so don't overlap
#define SpecificIPCUnalign ((volatile tSpecificIPC*)(TGDSIPCUserStartAddress))
#define SpecificIPCAlign ((volatile struct sAlignedIPCProy*)(TGDSIPCUserStartAddress+(sizeof(tSpecificIPC))))

//#define testGBAEMU4DSFSCode	//enable for generating a file you can later test in any emu, that file is created (you pick from the list) is using the same gbaemu4ds streaming driver.

//TGDS Memory Layout ARM7/ARM9 Cores
#define TGDS_ARM7_MALLOCSTART (u32)(0x06000000)
#define TGDS_ARM7_MALLOCSIZE (int)(112*1024)
#define TGDSDLDI_ARM7_ADDRESS (u32)(TGDSDLDI_ARM7_MALLOCSTART + TGDSDLDI_ARM7_MALLOCSIZE)

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ARM9
extern struct gbaheader_t gbaheader;
#endif

extern struct sIPCSharedTGDSSpecific* getsIPCSharedTGDSSpecific();
//NOT weak symbols : the implementation of these is project-defined (here)
extern void HandleFifoNotEmptyWeakRef(uint32 cmd1,uint32 cmd2);
extern void HandleFifoEmptyWeakRef(uint32 cmd1,uint32 cmd2);

#ifdef __cplusplus
}
#endif