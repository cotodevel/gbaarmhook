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
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include "socket.h"
#include "in.h"
#include <netdb.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "main.h"

//filesystem
#include "fsfatlayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "specific_shared.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

#include "gbaemu4ds_fat_ext.h"

#include "devoptab_devices.h"
#include "usrsettingsTGDS.h"

#include "videoTGDS.h"
#include "keypadTGDS.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include "stdint.h"

#include "main.h"
#include "settings.h"

//zlib (gzip) and zip
//#include "zlib/zlib.h"
//#include "zlib/zip/unzip.h"

//tools.
#include "util/opcode.h"
#include "util/util.h"
#include "util/buffer.h"
#include "util/translator.h"
#include "util/bios.h"


//disassembler (thumb)

#include "armstorm/arm.h" //THUMB DISASSEMBLER
#include "armstorm/armstorm.h" //THUMB DISASSEMBLER
#include "armstorm/common.h" //THUMB DISASSEMBLER
#include "armstorm/thumb.h" //THUMB DISASSEMBLER
#include "armstorm/thumb_db.h" //THUMB DISASSEMBLER

//PU and stack managmt
#include "pu/pu.h"
#include "pu/supervisor.h"

#define CLUSTER_FREE	0x00000000
#define	CLUSTER_EOF		0x0FFFFFFF
#define CLUSTER_FIRST	0x00000002

//unzip buffer
//char * uncompr = (char*) calloc(1024*1024,sizeof(char)); //for temp decomp buffer
//uLong uncomprLen = 1024*1024;


/*jmp from inline
asm volatile(
	"\tbx %0\n"
	: : "r" (0x02FFFE04)
);

struct GSF_FILE{
	char psftag[220];//psftag[50001]; //int psftag[12500]; //minigsf 
	char libname[64];
	bool gsfloaded;
	char gsflib[20];
	char *program; //8 bytes + 1*20 bytes + 1 byte + 1*64 bytes + 1*220 bytes = 406 bytes
	char *reserved;
	
};// __attribute__((aligned (8))); //GSF_FILE GSF_STR; 
//#pragma pack (2) //2 byte alignment

//alloc'd block into NDSRAM
//(void *) calloc (size , datablock size)
struct GSF_FILE * gsfptr = (GSF_FILE*) calloc(406,1);

*/

/* ////////////////////////////////////zlib
//const char hello[] = "if you see this zlib is working nicely!!";

//const char dictionary[] = "hello";
uLong len = (uLong)strlen(hello)+1;

//static const char* myVersion = ZLIB_VERSION;

//unsigned char decompressedgsf[1024*1024]; //1MB decompr buffer

// INSIDE FUNCTION ZLIB 
if (zlibVersion()[0] != myVersion[0]){
    printf("incompatible zlib version\n");
} 
else if (strcmp(zlibVersion(), ZLIB_VERSION) != 0){
    printf("warning: different zlib version\n");
}

//(dest, destLen, source, sourceLen)
//3rd arg (const unsigned char*)file, 4th arg (unsigned int long)GSFSIZE

//-->> works for zip:static Byte *compr, *uncompr;
//-->> works for zip:uLong comprLen = (1024*1024);
//-->> works for zip:uLong uncomprLen = comprLen;

//-->> works for zip:compr    = (Byte*)calloc((uInt)comprLen, 1);
//-->> works for zip:uncompr  = (Byte*)calloc((uInt)uncomprLen, 1);


res = compress(compr,&comprLen,(const Bytef*)hello,len);
switch(res){
case(Z_OK):
printf("stream compress status: OK! (%x) BUFFER:(%x) \n ",Z_OK,(int)&compr);
break;
case(Z_STREAM_END):
printf("stream compress status: error, unexpected end! (%x)",Z_STREAM_END);
break;
case(Z_NEED_DICT):
printf("stream compress status: error, need dictionary! (%x)",Z_NEED_DICT);
break;
case(Z_ERRNO):
printf("stream compress status: error, cant open file! (%x)",Z_ERRNO);
break;
case(Z_STREAM_ERROR):
printf("stream compress status: error, corrupted source stream! (%x)",Z_STREAM_ERROR);
break;
case(Z_DATA_ERROR):
printf("stream compress status: error, data not zlib ! (%x)",Z_DATA_ERROR);
break;
case(Z_MEM_ERROR):
printf("stream compress status: error, memory source corrupted! (%x)",Z_MEM_ERROR);
break;
case(Z_BUF_ERROR):
printf("stream compress status: error, not enough room in the output buffer! (%x)",Z_BUF_ERROR);
break;
}

//uncompress(uncompr, &uncomprLen, compr, comprLen);
// reserved buffer for uncompress: uncompr / size for res buffer : &uncomprLen
//res=uncompress(uncompr,&uncomprLen,compr,comprLen);(unsigned long)SBANKSZ
res=uncompress(decompressedgsf,&uncomprLen,compr,comprLen);
switch(res){
case(Z_OK):
printf("stream decomp status: OK! (%x) \n ",Z_OK);
break;
case(Z_STREAM_END):
printf("stream decomp status: error, unexpected end! (%x)",Z_STREAM_END);
break;
case(Z_NEED_DICT):
printf("stream decomp status: error, need dictionary! (%x)",Z_NEED_DICT);
break;
case(Z_ERRNO):
printf("stream decomp status: error, cant open file! (%x)",Z_ERRNO);
break;
case(Z_STREAM_ERROR):
printf("stream decomp status: error, corrupted source stream! (%x)",Z_STREAM_ERROR);
break;
case(Z_DATA_ERROR):
printf("stream decomp status: error, data not zlib ! (%x)",Z_DATA_ERROR);
break;
case(Z_MEM_ERROR):
printf("stream decomp status: error, memory source corrupted! (%x)",Z_MEM_ERROR);
break;
case(Z_BUF_ERROR):
printf("stream decomp status: error, not enough room in the output buffer! (%x)",Z_BUF_ERROR);
break;
}

printf("DATA at (%x):  \n",(int)&uncompr);
for (res=0;res<strlen(hello);res++){
printf("%c ",decompressedgsf[res]);
if ( (res % 9) == 0 ) printf("\n");
}

//free(compr);
free(uncompr);

*/
//////////////////////ZIP 
/*	strcat(temppath,(char*)"test.zip");
			int unzoutput=unzip(temppath, uncompr, uncomprLen);
			

	if(UNZ_END_OF_LIST_OF_FILE==unzoutput){
		printf("UNZ_END_OF_LIST_OF_FILE \n");
	}
	else if(UNZ_ERRNO==unzoutput){
		printf("error ZLIB deflate \n");
	}
	else if(UNZ_EOF==unzoutput){
		printf("error EOF FOUND \n");
	}
	else if(UNZ_PARAMERROR==unzoutput){
		printf("error UNZ_PARAMERROR \n");
	}
	else if(UNZ_BADZIPFILE==unzoutput){
		printf("error UNZ_BADZIPFILE \n");
	}
	else if(UNZ_INTERNALERROR==unzoutput){
		printf("error UNZ_INTERNALERROR \n");
	}
	else if(UNZ_CRCERROR==unzoutput){
		printf("error UNZ_CRCERROR \n");
	}
	else{
	printf("\x1b[22;1H OK. buf:[%p], Filesz:[%d] \n",uncompr,unzoutput);
	
				for(i=0;i<unzoutput;i++){
					printf("%c ",uncompr[i]);
				}
	
	}*/

/* THUMB DISASSEMBLER (little endian) */ //format { 0xc5, 0xc0-- };
unsigned char buf[1*2]; //buffer for 16 thumb instructions
struct DInst insts[10] = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}}; //_DInst insts[10] = {0};
struct DecomposeInfo info = {0}; //_DecomposeInfo info = {0};
struct TInst tinst = {0}; //TInst t = {0};
/* END THUMB DISASSEMBLER */

GBASystem gba; //this creates a proper GBASystem pointer to struct (stack) - across C++ from struct GBASystem (C)
//gbaHeader_t *gbaGamehdr;

extern struct gbaheader_t gbaheader;

//0x03800000 - 0x038082xx = 0x82B4 (33xxx bytesld) - 0x038082B5 - 0x03810000 = 0x7D4C free 
//note:

//  380FFDCh  ..  NDS7 Debug Stacktop / Debug Vector (0=None)
//  380FFF8h  4   NDS7 IRQ Check Bits (hardcoded RAM address)
//  380FFFCh  4   NDS7 IRQ Handler (hardcoded RAM address)

//linker (bin) objects (main.h)

//vector for dirlibs
int toggle=0; //toggler for browsefile() IRQ vsync-able

bool dldiload = false;
bool dirloaded;

//ticker for emulator loop
u32 cpucore_tick=0;

//fifo
extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

typedef struct{
	u32 Version;
	u32 listentr;
} __attribute__ ((__packed__)) patch_t;
patch_t patchheader;

typedef struct{
	u32 gamecode;
	u8 homebrew;
	u64 crc;
	char patchpath[512 * 2];
	u8 swaplcd;
	u8 savfetype;
	u8 frameskip;
	u8 frameskipauto;
	u16 frameline;
	u8 fastpu;
	u8 mb;
	u8 loadertype;
} __attribute__ ((__packed__)) patch2_t;
	
struct DirEntry {
	char name[512];
	bool isDirectory;
};

DIR *pdir;
struct dirent *pent;

/*
inline __attribute__((always_inline))
int wramtst(int wram,int top){
extern int temp,temp2;
temp=wramtst1(wram,top);
if(temp==alignw(top))
	printf("ARM9 test1/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
else 
	printf("ARM9RAM tst 1/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
temp2=wramtst2(wram,top);
if(temp2==alignw(top))
	printf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
else if ((temp2-2)==alignw(top))
	printf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n !!aligned read!!",temp2,EWRAM,temp2);
else 
	printf("ARM9RAM tst 2/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
return 0;
}
*/

int u32count_values(u32 u32word){
int i=0,cntr=0;

while(cntr<32){
	if (((u32word>>cntr)&0xf)>0){
		i++;
	}
	cntr+=4;
}
return i;
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

//psg noise test int temp5=0;

//Timers
//(data counter)
//4000100h - TM0CNT_L - Timer 0 Counter/Reload (R/W)
//4000104h - TM1CNT_L - Timer 1 Counter/Reload (R/W)
//4000108h - TM2CNT_L - Timer 2 Counter/Reload (R/W)
//400010Ch - TM3CNT_L - Timer 3 Counter/Reload (R/W)

//Writing to these registers initializes the <reload> value (but does not directly affect the current counter value). 
//Reading returns the current <counter> value (or the recent/frozen counter value if the timer has been stopped).
//The reload value is copied into the counter only upon following two situations: Automatically upon timer overflows, 
//or when the timer start bit becomes changed from 0 to 1.
//Note: When simultaneously changing the start bit from 0 to 1, and setting the reload value at the same time (by a single 32bit I/O operation), 
//then the newly written reload value is recognized as new counter value.

//(controller)
//4000102h - TM0CNT_H - Timer 0 Control (R/W)
//4000106h - TM1CNT_H - Timer 1 Control (R/W)
//400010Ah - TM2CNT_H - Timer 2 Control (R/W)
//400010Eh - TM3CNT_H - Timer 3 Control (R/W)
//  Bit   Expl.
//  0-1   Prescaler Selection (0=F/1, 1=F/64, 2=F/256, 3=F/1024)
//  2     Count-up Timing   (0=Normal, 1=See below)
//  3-5   Not used
//  6     Timer IRQ Enable  (0=Disable, 1=IRQ on Timer overflow)
//  7     Timer Start/Stop  (0=Stop, 1=Operate)
//  8-15  Not used
//When Count-up Timing is enabled, the prescaler value is ignored, instead the time is incremented each time when the previous counter overflows. This function cannot be used for Timer 0 (as it is the first timer).
//F = System Clock (16.78MHz).


//timer 0,1,2,3 (write only) TVAL
u16 TMXCNT_LW(u8 TMXCNT,int TVAL){
		*(u32*)(0x04000100+(TMXCNT*4))=TVAL;
	return 0;
}

//timer 0,1,2,3 (read only) CUR_TVAL
u16 TMXCNT_LR(u8 TMXCNT){
	return (*(u16*)(0x04000100+(TMXCNT*4)));
}

//timer 0,1,2,3 controller
u16 TMXCNT_HW(u8 TMXCNT, u8 prescaler,u8 countup,u8 status){
		*(u32*)(0x04000100+(TMXCNT*4+2))=(prescaler<<0)|(countup<<2)|(status<<7);
	return 0;
}

u32 emulatorgba(){

//1) read GBAROM entrypoint
//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
//3) get pointers from all reserved memory areas (allocated)
//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]

//btw entrypoint is always ARM code 

if(gba.cpustate==true){
	
	u32 new_instr=armfetchpc(rom);
	#ifdef DEBUGEMU
		printf("/*****************/");
		printf("\n rom:%x [%x]\n",(unsigned int)rom,(unsigned int)new_instr);
	#endif
	
	//CPUfetch depending on CPUmode
	(armstate==0)?disarmcode(new_instr):disthumbcode(new_instr);	

	//refresh vcount & disptat here before cpuloop
	gba.lcdticks=((*(u32*)0x04000006) &0xfff); //use vcounter for generation ticks

	cpuloop(cpucore_tick);	//1 list per hblank / threads from gba.lcdticks into IF
	cpucore_tick++;
	if(cpucore_tick>10001) 
		cpucore_tick=0;
}
else
	gba.cpustate=true;

//read input is done already -> gba.GBAP1

//increase PC depending on CPUmode
(armstate==0)?rom+=4:rom+=2;

//before anything, interrupts (GBA generated) are checked on NDS9 IRQ.s PU.C exceptirq()

//old dcache is discarded
//DC_InvalidateAll(); 
//DC_FlushAll();

return 0;
}

char temppath[255 * 2];
char biospath[255 * 2];
char savepath[255 * 2];
char patchpath[255 * 2];

int main(int _argc, sint8 **_argv) {

	/*			TGDS 1.5 Standard ARM9 Init code start	*/
	bool project_specific_console = false;	//set default console or custom console: default console
	GUI_init(project_specific_console);
	GUI_clear();

	sint32 fwlanguage = (sint32)getLanguage();

	printf("     ");
	printf("     ");
	
	int ret=FS_init();
	if (ret == 0)
	{
		printf("FS Init ok.");
	}
	else if(ret == -1)
	{
		printf("FS Init error.");
	}
	/*			TGDS 1.5 Standard ARM9 Init code end	*/
	
	biospath[0] = 0;
	savepath[0] = 0;
	patchpath[0] = 0;

	// GBA EMU INIT//
	//show gbadata printf("\x1b[21;1H
	strcat(temppath,(char*)"/gba/rs-pzs.gba");
	//printgbainfo (temppath);

	/******************************************************** GBA EMU INIT CODE *********************************************************************/

	//initemu(&gba);	//OK
	//bios_cpureset();	// clears all cpuregs!
	//bios_registerramreset(0xffff); //unless byte swap reads, 0xff alone hangs the nds hw // clears all cpuregs!
	//printf("initemu(); / cpureset(); /bios_regramrst()! \n");

	//CPULoadRom(struct GBASystem * gba, const char *szFile,bool extram)
	//loadrom(&gba,temppath,256);		//false for extraram ////int utilLoad(struct GBASystem * gba,const char *file,u8 *data,int size,bool extram)
	//printf("loadrom();\n");


	//setup emu
	gba.cpustate=true;
	gba.cpudmahack=false;
	cputotalticks = 0; //ori: gba.cputotalticks = 0;
	gba.armirqenable=true;

	//bios calls (flush) destroyed sp13 for usr mode
	gbavirtreg_cpu[0xd]=gbavirtreg_r13usr[0];
	  
	//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
	updatecpuflags(1,cpsrvirt,0x10);

	//opengbarom
	if(isgbaopen(gbaromfile)==0)
		printf("ready to open gbarom. ");
	if (opengbarom((const char*)getfatfsPath((char*)"gba/rs-pzs.gba"),"r+")==0){
		//printf("GBAROM open OK!");
		//printf("GBAROM size is (%d) bytes", (int)getfilesizegbarom());
	}
	else {
		printf("GBAROM open ERROR. close the nds");
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
	
	//PATCH_BOOTCODE EXTRACT
	int PATCH_BOOTCODE_SIZE = extract_word(PATCH_BOOTCODE_PTR,(PATCH_BOOTCODE_PTR[0]),(int)(4*64),(u32*)buf_wram,0xe1a0f00d,32); //mov pc,sp end
	//printf(">PATCH_BOOTCODE EXTRACT'd (%d) opcodes",(int)PATCH_BOOTCODE_SIZE);
	
	//PATCH_START EXTRACT
	int PATCH_START_SIZE = extract_word(PATCH_START_PTR,(PATCH_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+PATCH_BOOTCODE_SIZE),0xe12fff1e,32); //bx lr end
	//printf(">PATCH_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_START_SIZE);
	
	//PATCH_HOOK_START EXTRACT
	int PATCH_HOOK_START_SIZE = extract_word(PATCH_HOOK_START_PTR,(PATCH_HOOK_START_PTR[0]),(int)(4*64),(u32*)(buf_wram+PATCH_BOOTCODE_SIZE+PATCH_START_SIZE),0xe1a0f003,32); //mov pc,r3
	//printf(">PATCH_HOOK_START EXTRACT'd (%d) ARM opcodes",(int)PATCH_HOOK_START_SIZE);
	
	//NDS7 RTC
	int NDS7_RTC_PROCESS_SIZE = extract_word(NDS7_RTC_PROCESS_PTR,(NDS7_RTC_PROCESS_PTR[0]),(int)(4*64),(u32*)(buf_wram+PATCH_BOOTCODE_SIZE+PATCH_START_SIZE+PATCH_HOOK_START_SIZE),0xe12fff1e,32); //bx lr end
	//printf(">NDS7_RTC_PROCESS EXTRACT'd (%d) ARM opcodes",(int)NDS7_RTC_PROCESS_SIZE);
	
	//mostly ARM code
	//writeu32gbarom(int offset,u32 * buffer_input,int size);
	//PATCH_BOOTCODE (entrypoint patch...)
	writeu32gbarom(0x00ff8000,(u32*)(buf_wram),PATCH_BOOTCODE_SIZE*4);
	
	//PATCH_START (patch on entrypoint action...)
	writeu32gbarom(0x00ff0000,(u32*)(buf_wram+PATCH_BOOTCODE_SIZE),PATCH_START_SIZE*4);
	
	//PATCH_HOOK_START (IRQ handler patch)
	writeu32gbarom(0x00fe0000,(u32*)(buf_wram+PATCH_BOOTCODE_SIZE+PATCH_START_SIZE),PATCH_HOOK_START_SIZE*4);
	
	//NDS7_RTC_PROCESS (IRQ handler patch)
	writeu32gbarom(0x00fe8000,(u32*)(buf_wram+PATCH_BOOTCODE_SIZE+PATCH_START_SIZE+PATCH_HOOK_START_SIZE),NDS7_RTC_PROCESS_SIZE*4);
	
	//ENTRYPOINT
	//printf("\n ENTRYPOINT:(%x):[%x]",
	//(unsigned int)PATCH_ENTRYPOINT[0],(unsigned int)PATCH_ENTRYPOINT[1]);
	
	printf("\n >ENTRYPOINT PATCH");
	writeu32gbarom(0x00000204,(u32*)&PATCH_ENTRYPOINT[0],sizeof(u32)); //entrypoint opcode patch
	writeu32gbarom(0x000000d0,(u32*)&PATCH_ENTRYPOINT[1],sizeof(u32)); //entrypoint new address
	
	//IRQ Handler@0x0:ldr pc,=PATCH_HOOK_START
	writeu32gbarom(0x00000240,(u32*)&PATCH_ENTRYPOINT[2],sizeof(u32)); //IRQ redirect opcode
	writeu32gbarom(0x00000244,(u32*)&PATCH_ENTRYPOINT[3],sizeof(u32)); //IRQ redirect @0x08FE0000 new address
	
	
	closegbarom();
	nopinlasm();

//so far code is stable, this will cause lockups definitely
#ifdef MPURECONFIG
	setgbamap(); //does not Reconfig MPU memory properties (VECTORS set to 0x00000000) anymore :)
	printf("setgbamap();\n"); //<-------CAUSES SERIOUS FREEZES
#endif

	while (1)
	{
		scanKeys();
		if (keysPressed() & KEY_A){
			printf("test:%d",rand()&0xff);
		}
		
		if (keysPressed() & KEY_B){
			GUI_clear();
		}
		
		IRQVBlankWait();
	}
	
	return 0;
}
