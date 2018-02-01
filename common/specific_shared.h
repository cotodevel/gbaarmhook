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

//inherits what is defined in: ipcfifoTGDS.h
#ifndef __specific_shared_h__
#define __specific_shared_h__

#include "dsregs.h"
#include "dsregs_asm.h"
#include "typedefsTGDS.h"
#include "ipcfifoTGDS.h"
#include "dswnifi.h"
#include "memoryHandleTGDS.h"

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

struct sIPCSharedTGDSSpecific{
	uint32 frameCounter7;	//VBLANK counter7
	uint32 frameCounter9;	//VBLANK counter9
	uint32 * IPC_ADDR;
    uint8 * ROM;   		//pointer to ROM page
    int rom_size;   	//rom total size
};


//project specific IPC. tMyIPC is used by TGDS so don't overlap
#define SpecificIPCUnalign ((volatile tSpecificIPC*)(getUserIPCAddress()))
#define SpecificIPCAlign ((volatile struct sAlignedIPCProy*)(getUserIPCAddress()+(sizeof(tSpecificIPC))))

//#define testGBAEMU4DSFSCode	//enable for generating a file you can later test in any emu, that file is created (you pick from the list) is using the same gbaemu4ds streaming driver.
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern struct gbaheader_t gbaheader;
extern struct sIPCSharedTGDSSpecific* getsIPCSharedTGDSSpecific();
//NOT weak symbols : the implementation of these is project-defined (here)
extern void HandleFifoNotEmptyWeakRef(uint32 cmd1,uint32 cmd2,uint32 cmd3,uint32 cmd4);
extern void HandleFifoEmptyWeakRef(uint32 cmd1,uint32 cmd2,uint32 cmd3,uint32 cmd4);

#ifdef __cplusplus
}
#endif