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
#include "dsregs_asm.h"
#include "keypadTGDS.h"
#include "gbaarmhookFS.h"
#include "TGDSLogoLZSSCompressed.h"
#include "dswnifi_lib.h"
#include "biosTGDS.h"
#include "global_settings.h"
#include "fileBrowse.h"
#include "crc32.h"
#include "posixHandleTGDS.h"
#include "TGDSMemoryAllocator.h"
#include "ipcfifoTGDSUser.h"
#include "fatfslayerTGDS.h"
#include "utilsTGDS.h"

char curChosenBrowseFile[MAX_TGDSFILENAME_LENGTH+1];
char biospath[MAX_TGDSFILENAME_LENGTH+1];
char savepath[MAX_TGDSFILENAME_LENGTH+1];
char patchpath[MAX_TGDSFILENAME_LENGTH+1];

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
	
	printf("GBAARMHook ");
	printf("(Select): This menu. ");
	printf("(A): Patch GBA Rom (CRC32:%x) ");
	printf("Available heap memory: %d", getMaxRam());
	printarm7DebugBuffer();
}

int main(int argc, char argv[argvItems][MAX_TGDSFILENAME_LENGTH]) {

	/*			TGDS 1.6 Standard ARM9 Init code start	*/
	bool isTGDSCustomConsole = false;	//set default console or custom console: default console
	GUI_init(isTGDSCustomConsole);
	GUI_clear();
	printf("              ");
	printf("              ");
	
	bool isCustomTGDSMalloc = false;
	setTGDSMemoryAllocator(getProjectSpecificMemoryAllocatorSetup(TGDS_ARM7_MALLOCSTART, TGDS_ARM7_MALLOCSIZE, isCustomTGDSMalloc));
	sint32 fwlanguage = (sint32)getLanguage();
	#ifdef ARM7_DLDI
	setDLDIARM7Address((u32 *)TGDSDLDI_ARM7_ADDRESS);	//Required by ARM7DLDI!
	#endif
	
	int ret=FS_init();
	if (ret == 0)
	{
		printf("FS Init ok.");
	}
	else if(ret == -1)
	{
		printf("FS Init error.");
	}
	switch_dswnifi_mode(dswifi_idlemode);
	asm("mcr	p15, 0, r0, c7, c10, 4");
	flush_icache_all();
	flush_dcache_all();
	/*			TGDS 1.6 Standard ARM9 Init code end	*/
	
	//Show logo
	RenderTGDSLogoMainEngine((uint8*)&TGDSLogoLZSSCompressed[0], TGDSLogoLZSSCompressed_size);
	
	//Remove logo and restore Main Engine registers
	//restoreFBModeMainEngine();
	
	menuShow();
	
	while (1)
	{
		scanKeys();
		if(keysPressed() & KEY_A){
			clrscr();
			printf("     ");
			
			biospath[0] = 0;
			savepath[0] = 0;
			patchpath[0] = 0;
			
			char startPath[MAX_TGDSFILENAME_LENGTH+1];
			strcpy(startPath,"/");
			while( ShowBrowser((char *)startPath, (char *)curChosenBrowseFile) == true ){	//as long you keep using directories ShowBrowser will be true
				//navigating DIRs here...
			}
			globalfileHandle = opengbarom(curChosenBrowseFile, "r+");
			if (globalfileHandle != NULL){
				printf("Please wait calculating CRC32...");
				//CRC32 handling
				unsigned long crc32 = -1;
				int err;
				err = Crc32_ComputeFile(globalfileHandle, &crc32 );
				if (err == -1){
					printf("Couldn't calculate CRC32.");
					printf("Power Off NDS.");
					while(1==1){}
				}
				if((u32)crc32 != (u32)0x5F35977E){
					printf("Invalid file: crc32 = 0x%x ", crc32);
					printf("Expected: crc32 = 0x%x ", 0x5F35977E);
					printf("Power Off NDS.");
					while(1==1){}
				}
			}
			else {
				printf("GBA File not found. Power Off NDS.");
				while(1);
			}

			//int gbaofset=0;
			//printf("gbaromread @ %x:[%x]",(unsigned int)(0x08000000+gbaofset),(unsigned int)readu32gbarom(gbaofset));

			// u32 PATCH_BOOTCODE();
			// u32 PATCH_START();
			// u32 PATCH_HOOK_START();

			//label asm patcher	
			u32 * PATCH_BOOTCODE_PTR =((u32*)&PATCH_BOOTCODE);
			u32 * PATCH_START_PTR =((u32*)&PATCH_START);
			u32 * PATCH_HOOK_START_PTR =((u32*)&PATCH_HOOK_START);
			u32 * NDS7_RTC_PROCESS_PTR =((u32*)&NDS7_RTC_PROCESS);
			
			//printf("PATCH_BOOTCODE[0]: (%x) ",(unsigned int)(PATCH_BOOTCODE_PTR[0]));
			//printf("PATCH_START[0]: (%x) ",(unsigned int)(PATCH_START_PTR[0]));
			//printf("PATCH_HOOK_START[0]: (%x) ",(unsigned int)(PATCH_HOOK_START_PTR[0]));
			
			u8* buf_wram = (u8*)malloc(1024*1024*1);
			
			//PATCH_BOOTCODE EXTRACT
			int PATCH_BOOTCODE_SIZE = extract_word(PATCH_BOOTCODE_PTR,(PATCH_BOOTCODE_PTR[0]),(int)(4*64),(u32*)buf_wram,0xe1a0f00d,32); //mov pc,sp end
			//printf(">PATCH_BOOTCODE EXTRACT'd (%d) opcodes",(int)PATCH_BOOTCODE_SIZE);
			
			//PATCH_START EXTRACT
			int PATCH_START_SIZE = extract_word(PATCH_START_PTR,(PATCH_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)),0xe12fff1e,32); //bx lr end
			//printf(">PATCH_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_START_SIZE);
			
			//PATCH_HOOK_START EXTRACT
			int PATCH_HOOK_START_SIZE = extract_word(PATCH_HOOK_START_PTR,(PATCH_HOOK_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)),0xe1a0f003,32); //mov pc,r3
			//printf(">PATCH_HOOK_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_HOOK_START_SIZE);
			
			//NDS7 RTC
			int NDS7_RTC_PROCESS_SIZE = extract_word(NDS7_RTC_PROCESS_PTR,(NDS7_RTC_PROCESS_PTR[0]),(int)(4*64),(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)+(PATCH_HOOK_START_SIZE*4)),0xe12fff1e,32); //bx lr end
			//printf(">NDS7_RTC_PROCESS EXTRACT'd (%d) ARM opcodes",(int)NDS7_RTC_PROCESS_SIZE);
			
			//mostly ARM code
			//PATCH_BOOTCODE (entrypoint patch...)
			writeu32gbarom(0x00ff8000,(u32*)(buf_wram),PATCH_BOOTCODE_SIZE*4);
			
			//PATCH_START (patch on entrypoint action...)
			writeu32gbarom(0x00ff0000,(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)),PATCH_START_SIZE*4);
			
			//PATCH_HOOK_START (IRQ handler patch)
			writeu32gbarom(0x00fe0000,(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)),PATCH_HOOK_START_SIZE*4);
			
			//NDS7_RTC_PROCESS (IRQ handler patch)
			writeu32gbarom(0x00fe8000,(u32*)(buf_wram+(PATCH_BOOTCODE_SIZE*4)+(PATCH_START_SIZE*4)+(PATCH_HOOK_START_SIZE*4)),NDS7_RTC_PROCESS_SIZE*4);
			
			//ENTRYPOINT
			//printf("\n ENTRYPOINT:(%x):[%x]",
			//(unsigned int)PATCH_ENTRYPOINT[0],(unsigned int)PATCH_ENTRYPOINT[1]);
			
			printf("\n >ENTRYPOINT PATCH");
			writeu32gbarom(0x00000204,(u32*)&PATCH_ENTRYPOINT[0],sizeof(u32)); //entrypoint opcode patch
			writeu32gbarom(0x000000d0,(u32*)&PATCH_ENTRYPOINT[1],sizeof(u32)); //entrypoint new address
			
			//IRQ Handler@0x0:ldr pc,=PATCH_HOOK_START
			writeu32gbarom(0x00000240,(u32*)&PATCH_ENTRYPOINT[2],sizeof(u32)); //IRQ redirect opcode
			writeu32gbarom(0x00000244,(u32*)&PATCH_ENTRYPOINT[3],sizeof(u32)); //IRQ redirect @0x08FE0000 new address
			
			free(buf_wram);
			closegbarom();
			printf("File patched successfully. Power Off NDS.");
			
			while(1==1){}
		}
		
		if (keysPressed() & KEY_SELECT){
			menuShow();
			scanKeys();
			while(keysPressed() & KEY_SELECT){
				scanKeys();
			}
		}
		
		handleARM9SVC();	/* Do not remove, handles TGDS services */
		IRQVBlankWait();
	}
	return 0;
}
