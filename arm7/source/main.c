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


#include "main.h"
#include "biosTGDS.h"
#include "spifwTGDS.h"
#include "posixHandleTGDS.h"
#include "biosTGDS.h"
#include "eventsTGDS.h"
#include "wifi_arm7.h"
#include "powerTGDS.h"

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	/*			TGDS 1.6 Standard ARM7 Init code start	*/
	//wait for VRAM D to be assigned from ARM9->ARM7 (ARM7 has load/store on byte/half/words on VRAM)
	while (!(*((vuint8*)0x04000240) & 0x2));
		
	installWifiFIFO();		
		
	int argBuffer[MAXPRINT7ARGVCOUNT];
	memset((unsigned char *)&argBuffer[0], 0, sizeof(argBuffer));
	argBuffer[0] = 0xc070ffff;
	writeDebugBuffer7("TGDS ARM7.bin Boot OK!", 1, (int*)&argBuffer[0]);
		
	/*			TGDS 1.6 Standard ARM7 Init code end	*/
	
	powerON(POWER_SOUND);
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7F);
	
    while (1) {
		handleARM7SVC();	/* Do not remove, handles TGDS services */
		IRQWait(IRQ_HBLANK);
	}
   
	return 0;
}
