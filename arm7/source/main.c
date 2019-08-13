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
#include "ipcfifoTGDSUser.h"

#include "wifi_arm7.h"
#include "usrsettingsTGDS.h"
#include "timerTGDS.h"
#include "biosTGDS.h"
#include "spiTGDS.h"
#include "dmaIO.h"
#include "CPUARMTGDS.h"

bool isArm7ClosedLid = false;

//---------------------------------------------------------------------------------
int main(int _argc, sint8 **_argv) {
//---------------------------------------------------------------------------------
	/*			TGDS 1.5 Standard ARM7 Init code start	*/
	installWifiFIFO();		
	/*			TGDS 1.5 Standard ARM7 Init code end	*/
	
	powerON(POWER_SOUND);
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7F);
	
    while (1) {
		if(isArm7ClosedLid == false){
			if((REG_KEYXY & KEY_HINGE) == KEY_HINGE){
				SendFIFOWords(FIFO_IRQ_LIDHASCLOSED_SIGNAL, 0);
				screenLidHasClosedhandlerUser();
				isArm7ClosedLid = true;
			}
		}
		IRQVBlankWait();
	}
   
	return 0;
}