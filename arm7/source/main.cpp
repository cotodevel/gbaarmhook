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


#include <string.h>
#include "main.h"
#include "InterruptsARMCores_h.h"
#include "interrupts.h"

#include "ipcfifoTGDS.h"
#include "specific_shared.h"

#include "wifi_arm7.h"
#include "usrsettingsTGDS.h"
#include "timerTGDS.h"
#include "biosTGDS.h"
#include "spiTGDS.h"
#include "dmaIO.h"
#include "CPUARMTGDS.h"

//---------------------------------------------------------------------------------
int main(int _argc, sint8 **_argv) {
//---------------------------------------------------------------------------------
	IRQInit();
	while (!(*((vuint8*)0x04000240) & 0x2));
	useARM7VRAMStacks();	//change ARM7 stacks to VRAM
	installWifiFIFO();		//use DSWIFI
	initTGDSAudioSystem();	//Init Audio NDS7
	
	powerON(POWER_SOUND);
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7F);
	
    while (1) {
		IRQVBlankWait();
	}
   
	return 0;
}