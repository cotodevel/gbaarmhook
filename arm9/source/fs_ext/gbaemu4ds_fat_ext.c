#include "gbaemu4ds_fat_ext.h"
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dir.h>
#include <fcntl.h>

#include "nds_cp15_misc.h"
#include "dldi.h"
#include "fatfslayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "ipcfifoTGDSUser.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

#include "buffer.h"

u32 *sectortabel;
void * lastopen;
void * lastopenlocked;
int PosixStructFDLastLoadFile;	//Coto: a StructFD non-posix file descriptor having the handle for the FILE * open at the time the sector table map was generated.
								//TGDS uses this to access the fatfs level file attributes
u32 current_pointer = 0;
u32 allocedfild[buffslots];
u8* greatownfilebuffer;

//DLDI Interface @ io_dldi_data->ioInterface.readSectors(sector, count, buff)

//part of fatfile.c
void generatefilemap(FILE * f, int size)	//FILE * f is already open at this point.
{
	clrscr();
	int clusCount;
	printf("generating file map (size %d Byte)",((size/chucksize) + 1)*8);
	sectortabel = (u32*)malloc(((size/chucksize) + 1)*8); //alloc for size every Sector has one u32
	greatownfilebuffer = (u8*)malloc(chucksize * buffslots);
	clusCount = size/getDiskClusterSizeBytes();
	
	sint32 fd = -1;
	struct fd * fdinst = NULL;
	if(f){
		fd = fileno(f);
		fdinst = getStructFD(fd);
		PosixStructFDLastLoadFile = fd;		//use getStructFD(PosixStructFDLastLoadFile) to access the File from now on!
	}
	else{
		clrscr();
		printf("the FILE * could not be opened.");
		while(1==1){}
	}
	
	//Coto Todo: chucksize replace by sectorSize so its SD dependant and implementation does not break on cards that aren't 512 bytes per sector lol
	//setblanc
	int i = 0;
	while(i < (getDiskClusterSizeBytes()/chucksize)*clusCount+1)
	{
		sectortabel[i*2] = 0x0;
		i++;
	}
	i = 0;
	while(i < buffslots)
	{
		allocedfild[i] = 0x1;
		i++;
	}

	//this actual Cluster
	int ClusterOffset = 0;
	
	int mappoffset = 0;
	i = 0;
	while(i < (getDiskClusterSizeBytes()/chucksize))
	{
		sectortabel[mappoffset*2 + 1] = getStructFDStartSectorByCluster(fdinst, ClusterOffset);
		mappoffset++;
		i++;
	}
	
	//Iterate over several Clusters
	ClusterOffset = 1;	//start from next Cluster please
	while (clusCount > 0) {
		clusCount--;
		i = 0;
		while(i < (getDiskClusterSizeBytes()/chucksize))
		{
			sectortabel[mappoffset*2 + 1] = getStructFDStartSectorByCluster(fdinst, ClusterOffset);
			mappoffset++;
			i++;
		}
		ClusterOffset++;
	}
	
	//debug
	/*
	//read sector table from original gbaemu4ds
	uint8 * sectortabel_read = (uint8*)malloc(((size/chucksize) + 1)*8);
	//compare the sector tables from original GBAEMU4DS against TGDS version
	FILE * fcompare = fopen(getfatfsPath((char*)"gbaemu4dsori_sectortabel.bin"),"r");
	//size_t fread ( void * ptr, size_t size, size_t count, FILE * stream )
	int sizeRead = fread((uint8*)sectortabel_read,1,(((size/chucksize) + 1)*8),fcompare);
	fclose(fcompare);
	
	//clrscr();
	printf("Read %d bytes!",sizeRead);
	//save TGDS sector table
	FILE * fsave = fopen(getfatfsPath((char*)"TGDS-sectortabel.bin"),"w+");
	fwrite((uint8*)sectortabel,1,(((size/chucksize) + 1)*8),fsave);
	fclose(fsave);
	
	uint32 * sectorPtr1 = (uint32*)sectortabel_read;
	uint32 * sectorPtr2 = (uint32*)sectortabel;
	
	//use sector offset 1 (uint32) because the sector 0 is always zero 
	if ((uint32)(sectorPtr1[1]) == (uint32)(sectorPtr2[1])){
		printf("sector table parse OK!");
	}
	else{
		printf("sector table TGDS malformed!");
	}
	printf("sectorTblTGDS:%x",sectorPtr2[1]);
	printf("sectorTblLIBFAT:%x",sectorPtr1[1]);
	printf("building sectortable end.");
	*/
	//debug end
}

void getandpatchmap(int offsetgba,int offsetthisfile,FILE * f)	//FILE * f is already open at this point.
{
	int clusCount = 0;
	sint32 fd = -1;
	struct fd * fdinst = NULL;
	clusCount = offsetthisfile/getDiskClusterSizeBytes();
	int offset1 = (offsetthisfile/chucksize) % getDiskClusterSizeBytes();
	int mappoffset = offsetthisfile/chucksize;
	if(f){
		fd = fileno(f);
		fdinst = getStructFD(fd);
		sectortabel[mappoffset*2 + 1] = getStructFDStartSectorByCluster(fdinst, clusCount) + offset1;
	}
	else{
		clrscr();
		printf("this is not a FIL * !!");
		while(1==1){}
	}
}


//part of gbaemu4ds_fat_ext.cpp

__attribute__((section(".itcm")))
u8 ichfly_readu8(unsigned int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	unsigned int sectoroffset = pos % chucksize;
	unsigned int mappoffset = pos / chucksize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2]);	
	if(asd != (u8*)0x0)return asd[sectoroffset]; //found exit here
	sectortabel[allocedfild[current_pointer]] = 0x0; //reset
	allocedfild[current_pointer] = mappoffset*2; //set new slot
	asd = greatownfilebuffer + current_pointer * chucksize;
	sectortabel[mappoffset*2] = (u32)asd;

	io_dldi_data->ioInterface.readSectors(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);	//readSectorslocked(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);
#ifdef countpagefalts
pagefehler++;
#endif
	current_pointer++;
	if(current_pointer == buffslots)current_pointer = 0;
	
	return asd[sectoroffset];
}

__attribute__((section(".itcm")))
u16 ichfly_readu16(unsigned int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	unsigned int sectoroffset = pos % chucksize;
	unsigned int mappoffset = pos / chucksize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2]);
	if(asd != (u8*)0x0)return *(u16*)(&asd[sectoroffset]); //found exit here
	sectortabel[allocedfild[current_pointer]] = 0x0; //clear old slot
	allocedfild[current_pointer] = mappoffset*2; //set new slot
	asd = greatownfilebuffer + current_pointer * chucksize;
	sectortabel[mappoffset*2] = (u32)asd;
	
	io_dldi_data->ioInterface.readSectors(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);	//readSectorslocked(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);
#ifdef countpagefalts
pagefehler++;
#endif
	current_pointer++;
	if(current_pointer == buffslots)current_pointer = 0;
	
	return *(u16*)(&asd[sectoroffset]);
}

__attribute__((section(".itcm")))
u32 ichfly_readu32(unsigned int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	unsigned int sectoroffset = pos % chucksize;
	unsigned int mappoffset = pos / chucksize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2]);
	if(asd != (u8*)0x0)return *(u32*)(&asd[sectoroffset]); //found exit here
	sectortabel[allocedfild[current_pointer]] = 0x0;
	allocedfild[current_pointer] = mappoffset*2; //set new slot
	asd = greatownfilebuffer + current_pointer * chucksize;
	sectortabel[mappoffset*2] = (u32)asd;

	io_dldi_data->ioInterface.readSectors(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);	//readSectorslocked(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);
#ifdef countpagefalts
pagefehler++;
#endif
	current_pointer++;
	if(current_pointer == buffslots)current_pointer = 0;
	return *(u32*)(&asd[sectoroffset]);
}

__attribute__((section(".itcm")))
void ichfly_readdma_rom(u32 pos,u8 *ptr,u32 c,int readal) //need lockup only alined is not working 
{
	// Calculate the sector and byte of the current position,
	// and store them
	int sectoroffset = 0;
	int mappoffset = 0;
	int currsize = 0;
	if(readal == 4) //32 Bit
	{
		while(c > 0)
		{
			sectoroffset = (pos % chucksize) /4;
			mappoffset = pos / chucksize;
			currsize = (chucksize / 4) - sectoroffset;
			if(currsize == 0)currsize = chucksize / 4;
			if(currsize > c) currsize = c;
			u32* asd = (u32*)(sectortabel[mappoffset*2]);
			
			if(asd != (u32*)0x0)//found exit here
			{
				int i = 0; //copy
				while(currsize > i)
				{
					*(u32*)(&ptr[i*4]) = asd[sectoroffset + i];
					i++;
				}
				c -= currsize;
				pos += (currsize * 4);
				ptr += (currsize * 4);
				continue;
			}

			sectortabel[allocedfild[current_pointer]] = 0x0;

			allocedfild[current_pointer] = mappoffset*2; //set new slot
			asd = (u32*)(greatownfilebuffer + current_pointer * chucksize);
			sectortabel[mappoffset*2] = (u32)asd;

			io_dldi_data->ioInterface.readSectors(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);	//readSectorslocked(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);
			#ifdef countpagefalts
pagefehler++;
#endif
			current_pointer++;
			if(current_pointer == buffslots)current_pointer = 0;

			int i = 0; //copy
			while(currsize > i)
			{
				*(u32*)(&ptr[i*4]) = asd[sectoroffset + i];
				i++;
			}
			c -= currsize;
			pos += (currsize * 4);
			ptr += (currsize * 4);
		}
	}
	else //16 Bit
	{
		while(c > 0)
		{
			sectoroffset = (pos % chucksize) / 2;
			mappoffset = pos / chucksize;
			currsize = (chucksize / 2) - sectoroffset;
			if(currsize == 0)currsize = chucksize / 2;
			if(currsize > c) currsize = c;

			u16* asd = (u16*)(sectortabel[mappoffset*2]);
			//printf("%X %X %X %X %X %X\n\r",sectoroffset,mappoffset,currsize,pos,c,chucksize);
			if(asd != (u16*)0x0)//found exit here
			{
				int i = 0; //copy
				while(currsize > i)
				{
					*(u16*)(&ptr[i*2]) = asd[sectoroffset + i];
					i++;
				}
				c -= currsize;
				ptr += (currsize * 2);
				pos += (currsize * 2);
				continue;
			}

			sectortabel[allocedfild[current_pointer]] = 0x0;

			allocedfild[current_pointer] = mappoffset*2; //set new slot
			asd = (u16*)(greatownfilebuffer + current_pointer * chucksize);
			sectortabel[mappoffset*2] = (u32)asd;

			io_dldi_data->ioInterface.readSectors(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);	//readSectorslocked(sectortabel[mappoffset*2 + 1], chucksizeinsec, asd);
			#ifdef countpagefalts
pagefehler++;
#endif
			current_pointer++;
			if(current_pointer == buffslots)current_pointer = 0;

			int i = 0; //copy
			while(currsize > i)
			{
				*(u16*)(&ptr[i*2]) = asd[sectoroffset + i];
				i++;
			}
			c -= currsize;
			ptr += (currsize * 2);
			pos += (currsize * 2);
		}
	}
}

//test case: 
//use original gbaemu4ds streaming code to recreate the file. Uses generatefilemap's sectortable. The output file should run in any other emu.
void testGBAEMU4DSFSTGDS(FILE * f,sint32 fileSize){	//FILE * f is already open at this point.
	
	clrscr();
	printf("begin test case");
	char * outputTestCaseFile = "TestCaseFile.bin";
	sint32 streamBufSize = 64*1024;	//higher buffer == faster testing code
	uint8 * streambuf = (uint8 *)malloc(streamBufSize);
	FILE * fout = fopen(getfatfsPath(outputTestCaseFile),"w+");
	
	//1st sector is bugged in gbaemu4ds filesystem code so we must recreate that one (it seems to fetch the disk sector?)
	uint8 firstSect[0x200];
	fseek(f,0,SEEK_SET);
	fread((uint8*)firstSect,1,sizeof(firstSect),f);
	
	//2nd sector onwards
	int FileOffsetChunks = fileSize / streamBufSize;
	int index = 0;
	for(index = 0; index < FileOffsetChunks ; index++){		
		uint32 * strmBuf = (uint32 *)streambuf;
		memset (streambuf, 0, streamBufSize);
		int indexBlock = 0;
		for(indexBlock = 0; indexBlock < (streamBufSize/sizeof(uint32)) ; indexBlock++){
			int fileOffset = (index*streamBufSize) + ( (indexBlock*sizeof(uint32)) );
			strmBuf[indexBlock] = ichfly_readu32(fileOffset);
		}
		fwrite((uint8*)strmBuf,1,streamBufSize,fout);		
	}
	
	int FileMapChunks = fileSize % streamBufSize;
	if(FileMapChunks > 0){
		//means this file is not log2 aligned, and we need to extract that modulus part as well
		memset (streambuf, 0, streamBufSize);
		int indexBlock = 0;
		for(indexBlock = 0; indexBlock < FileMapChunks ; indexBlock++){
			int fileOffset = (FileOffsetChunks * streamBufSize) + indexBlock;
			streambuf[indexBlock] = ichfly_readu8(fileOffset);
		}
		fwrite((uint8*)streambuf,1,FileMapChunks,fout);
		printf("this file is ODD!-Size:%d",fileSize);
	}
	else{
		printf("this file is EVEN!-Size:%d",fileSize);
	}
	
	//rewrite first sector & set end file
	fseek(fout,0,SEEK_SET);
	fwrite((uint8*)firstSect,1,sizeof(firstSect),fout);
	fseek(fout,fileSize,SEEK_SET);
	
	free(streambuf);
	fclose(fout);
	fseek(f,0,SEEK_SET);
	printf("done. Use %s file in other emu.",(char*)outputTestCaseFile);
}



//gbaARMHook specific

u8 stream_readu8(u32 pos){
	return ichfly_readu8((unsigned int)pos);
}

u16 stream_readu16(u32 pos){
	return ichfly_readu16((unsigned int)pos);
}

u32 stream_readu32(u32 pos){
	return ichfly_readu32((unsigned int)pos);
}


FILE * gbaromfile = NULL;

u32 isgbaopen(FILE * gbahandler){

if (!gbahandler)
	return 1;
else
	return 0;
}

/*
mode	Description
"r"	Open a file for reading. The file must exist.
"w"	Create an empty file for writing. If a file with the same name already exists its content is erased and the file is considered as a new empty file.
"a"	Append to a file. Writing operations append data at the end of the file. The file is created if it does not exist.
"r+"	Open a file for update both reading and writing. The file must exist.
"w+"	Create an empty file for both reading and writing.
"a+"	Open a file for reading and appending.
*/
u32 opengbarom(const char * filename,const char * access_type){

FILE *fh = fopen(filename, access_type); //r
if(!fh)
	return 1;

gbaromfile=fh;
return 0;
}

u32 closegbarom(){
if(!gbaromfile){
	printf("FATAL: GBAFH isn't open");
	return 1;
}

fclose(gbaromfile);

printf("GBARom closed!");
return 0;
}

u32 readu32gbarom(int offset){

if(!gbaromfile)
	printf("FATAL: GBAFH isn't open");

//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

int sizeread=fread((void*)disk_buf, 1, chucksize,gbaromfile); //2) perform read (512bytes read (128 reads))

if (sizeread!=chucksize){
		printf("FATAL: GBAREAD isn't (%d) bytes",(int)chucksize);
	}
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return disk_buf[0];
}


u16 writeu16gbarom(int offset,u16 * buf_in,int size_elem){

if(!gbaromfile){
	printf("FATAL: GBAFH isn't open");
	return 1;
}

//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

//size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
int sizewritten=fwrite((u16*)buf_in, 1, size_elem, gbaromfile); //2) perform read (512bytes read (128 reads))
if (sizewritten!=size_elem){
		printf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
	}
else{
	printf("write ok:%x",(unsigned int)buf_in[0x0]);
}
	
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return 0;
}


u32 writeu32gbarom(int offset,u32 * buf_in,int size_elem){

if(!gbaromfile){
	printf("FATAL: GBAFH isn't open");
	return 1;
}
//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

//size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
int sizewritten=fwrite((u32*)buf_in, 1, size_elem, gbaromfile); //2) perform read (512bytes read (128 reads))
if (sizewritten!=size_elem){
		printf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
	}
else{
	printf("write ok:%x",(unsigned int)buf_in[0x0]);
}
	
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return 0;
}


u32 getfilesizegbarom(){
if(!gbaromfile){
	printf("FATAL: GBAFH isn't open");
	return 0;
}
fseek(gbaromfile,0,SEEK_END);
int filesize = ftell(gbaromfile);
fseek(gbaromfile,0,SEEK_SET);

return filesize;
}