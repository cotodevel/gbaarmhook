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
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dswnifi_lib.h"
#include "keypadTGDS.h"
#include "TGDSLogoLZSSCompressed.h"
#include "fileBrowse.h"	//generic template functions from TGDS: maintain 1 source, whose changes are globally accepted by all TGDS Projects.
#include "biosTGDS.h"
#include "ipcfifoTGDSUser.h"
#include "dldi.h"
#include "global_settings.h"
#include "posixHandleTGDS.h"
#include "TGDSMemoryAllocator.h"
#include "consoleTGDS.h"
#include "soundTGDS.h"
#include "nds_cp15_misc.h"
#include "fatfslayerTGDS.h"
#include "utilsTGDS.h"
#include "click_raw.h"
#include "ima_adpcm.h"

// Includes
#include "WoopsiTemplate.h"

//TGDS Soundstreaming API
int internalCodecType = SRC_NONE; //Returns current sound stream format: WAV, ADPCM or NONE
struct fd * _FileHandleVideo = NULL; 
struct fd * _FileHandleAudio = NULL;

bool stopSoundStreamUser(){
	return stopSoundStream(_FileHandleVideo, _FileHandleAudio, &internalCodecType);
}

void closeSoundUser(){
	//Stubbed. Gets called when closing an audiostream of a custom audio decoder
}

//extract_word(u32 * buffer (haystack), u32 word (needle) , int buffsize, u32 * buffer (output data),u32 delimiter_word), u8 type = 0 character string / 1 = hex
//extracts all values until "delimiter"
int extract_word(u32 * buffer,u32 word,int size ,u32 * buffer_out,u32 delimiter,u8 type){ //strings shall be constant (not movables) for strlen
int i=0,temp=0,m=0;

//removed because code was fugly
//char string search
if (type==0){
	//printf("char string search!");
	/*
	//m=0;
	for(i=0;i<size;i++){
		k=0;
		for(j=m;j<strlen(word);j++){
		
			if  ( (( ( buffer[i]>>(k*8) )&0xff ) == (word[j]) ) ){ //j gets actual pos, does not always begin from 0
				temp++;
				//debug
				//printf(" %x_i - k(%d)\n",( ( buffer[i]>>(k*8) )&0xff ),k);
				//j=strlen(word);
				m++; //increase word segment only til reach top
			}
			
			//not found? then reset counter
			else {
				m=0;
				j=strlen(word);
			}
			
			//did we reach top bit 8*3?
			if(k==3) {
				k=0;
				j=strlen(word); //next i
			}
			else k++;
		}
		
		//found word? get everything starting from there until delimiter
		if (temp >= strlen(word)){
			m=0;
			//0 gets re used by m, twice, both for reset, and as a valid offset. this causes double valid word found.
			if (temp != strlen(word)) { 
				//k-=1;
			}
			else i++; //aligned means the current i index hasn't changed, next index is what we look for.
			
				//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				while( (u32)( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				printf(" %x_i - k(%d)\n",(unsigned int)( ( buffer[i]>>(k*8) )&0xff ), (int)k);
				
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=( ( buffer[i]>>(k*8) ) );
				
				m++;
		
				if(k==3) {
					k=0;
					i++;
				}
				else k++;
				
			}
		break;
		}
	}
*/
}
//hex search
else if (type==32){
	//printf("hex search!");
	//printf("hex to look: %x",(unsigned int)word);
	for(i=0;i<size;i++){
		if  (buffer[i] == word ){ //j gets actual pos, does not always begin from 0
				temp++;
		}
		
		//found word? get everything starting from there until delimiter
		if (temp > 0){
			//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
			while(buffer[m] != delimiter){
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=buffer[m];
				m++;				
			}
			buffer_out[m]=buffer[m]; //and copy over delimiter's last opcode
			
			m++; //offset fix for natural numbers (at least 1 was found!)
			break; //done? kill main process
		}
	}
}
return m;
}

static inline void menuShow(){
	clrscr();
	printf("     ");
	printf("     ");
	printf("toolchaingenericds-template: ");
	printf("(Select): This menu. ");
	printf("(Start): FileBrowser : (A) Play WAV/IMA-ADPCM (Intel) strm ");
	printf("(D-PAD:UP/DOWN): Volume + / - ");
	printf("(D-PAD:LEFT): GDB Debugging. >%d", TGDSPrintfColor_Green);
	printf("(D-PAD:RIGHT): Demo Sound. >%d", TGDSPrintfColor_Yellow);
	printf("(B): Stop WAV/IMA-ADPCM file. ");
	printf("Current Volume: %d", (int)getVolume());
	if(internalCodecType == SRC_WAVADPCM){
		printf("ADPCM Play: >%d", TGDSPrintfColor_Red);
	}
	else if(internalCodecType == SRC_WAV){	
		printf("WAVPCM Play: >%d", TGDSPrintfColor_Green);
	}
	else{
		printf("Player Inactive");
	}
	printf("Available heap memory: %d >%d", getMaxRam(), TGDSPrintfColor_Cyan);
}

//ToolchainGenericDS-LinkedModule User implementation: Called if TGDS-LinkedModule fails to reload ARM9.bin from DLDI.
char args[8][MAX_TGDSFILENAME_LENGTH];
char *argvs[8];
int TGDSProjectReturnFromLinkedModule() {
	return -1;
}

int main(int argc, char **argv) {
	
	/*			TGDS 1.6 Standard ARM9 Init code start	*/
	bool isTGDSCustomConsole = false;	//set default console or custom console: default console
	GUI_init(isTGDSCustomConsole);
	GUI_clear();
	
	bool isCustomTGDSMalloc = true;
	setTGDSMemoryAllocator(getProjectSpecificMemoryAllocatorSetup(TGDS_ARM7_MALLOCSTART, TGDS_ARM7_MALLOCSIZE, isCustomTGDSMalloc, TGDSDLDI_ARM7_ADDRESS));
	sint32 fwlanguage = (sint32)getLanguage();
	
	asm("mcr	p15, 0, r0, c7, c10, 4");
	flush_icache_all();
	flush_dcache_all();
	
	int ret=FS_init();
	if (ret == 0)
	{
		printf("FS Init ok.");
	}
	else if(ret == -1)
	{
		printf("FS Init error.");
	}
	
	/*			TGDS 1.6 Standard ARM9 Init code end	*/
	
	//Show logo
	RenderTGDSLogoMainEngine((uint8*)&TGDSLogoLZSSCompressed[0], TGDSLogoLZSSCompressed_size);
	
	// Create Woopsi UI
	WoopsiTemplate WoopsiTemplateApp;
	WoopsiTemplateProc = &WoopsiTemplateApp;
	return WoopsiTemplateApp.main(argc, argv);
	
	while(1) {
		handleARM9SVC();	/* Do not remove, handles TGDS services */
		IRQVBlankWait();
	}

	return 0;
}

