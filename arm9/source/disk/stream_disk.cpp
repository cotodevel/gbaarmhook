#include <nds.h>

#include <fat.h>
#include <filesystem.h>
#include <dirent.h>
#include <unistd.h>    // for sbrk()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "..\util\opcode.h"
#include "..\util\buffer.h"
#include "..\arm9.h"

#include "stream_disk.h"

#include "disc.h"
#include "fatfile.h"
#include "cache.h"
#include "file_allocation_table.h"
#include "bit_ops.h"
#include "filetime.h"
#include "lock.h"

//ptr to sectortable
u32 	*	sector_table;
//ptrs to old FAT structs
void 	* 	lastopen;
void 	* 	lastopenlocked;

//u32 allocedfild[buffslots];
u32	current_pointer=0;

//filesystem nds cluster size
u32 ndsclustersize=0;

//filemapsize 
u32 fmapsize=0;

PARTITION* partitionlocked;
FN_MEDIUM_READSECTORS	readSectorslocked;

void generate_filemap(int size){
//very important to link
extern u32 * sector_table;
	

	FILE_STRUCT* file = (FILE_STRUCT*)(lastopen);
	lastopenlocked = lastopen; 	//copy
	PARTITION* partition;
	
	partition = file->partition;
	partitionlocked = partition;
	readSectorslocked = file->partition->disc->readSectors;
	
	iprintf("Generating Filemap: (sector_table size:[0x%x]) \n ",(size/sectorsize)*sizeof(u32)); //u32 because sector length is u32
	fmapsize=(size/sectorsize)*sizeof(u32);
	sector_table = (u32*)malloc((size/sectorsize)*sizeof(u32)); //filemap = (romsize / sectorsize) * sizeof(u32)
	
	//Cluster file start offset is file->startCluster
	
	u32 cluster = 0;
	u32 clusCount=0;
	int i=0,mappoffset=0;
	ndsclustersize=partition->bytesPerCluster;
	
	//tmp
	//u8 tempbuf[sectorsize];
	//u8* fetch = (u8*)&tempbuf[0];
	
	//for each file's cluster..
	for(clusCount=0; clusCount < size/(partition->bytesPerCluster); clusCount++){
		//Next Cluster.. (clusters must be checked because they may not be linear)
		//starts from zero
		cluster = _FAT_fat_nextCluster(partition, (file->startCluster) + clusCount) - 1;
		
		//And retrieve 1 cluster : N sector into sector_table
		for(i=0; i < (((int)partition->bytesPerCluster)/sectorsize) ;i++){
			sector_table[mappoffset] = _FAT_fat_clusterToSector(partition, cluster) + i;
			
			/*if (i==0){ //debug
				iprintf("first sector is: %x",sector_table[i]); 
				iprintf("first cluster:%x\n",file->startCluster + clusCount);
				readSectorslocked(sector_table[mappoffset / sectorsize], sectorscale, fetch);
				iprintf("and data[%x]",*(u32*)(&fetch[i % sectorsize]));
				while(1);
			}*/
			mappoffset++;
		}
	}
	/* //debug
	iprintf("File props: \n");
	iprintf("ROM Clusters:[0x%x] \n",size/(partition->bytesPerCluster));
	iprintf("ROM Filemap Cluster Size:[0x%x] \n",(((int)partition->bytesPerCluster)/sectorsize));
	iprintf("partition->bytesPerCluster:%x \n",partition->bytesPerCluster);
	iprintf("OK! \n");
	*/
}

u8 stream_readu8(u32 pos){
	//return (stream_readu32(pos) >> /*((pos % 4)+(24))*/ 24);
	return (u8)(stream_readu32(pos)&0xff); //byte swapped (big endian reads)
}

u16 stream_readu16(u32 pos){
	return (u16)(stream_readu32(pos)&0xffff);
}

u32 stream_readu32(u32 pos){
	return (u32)readu32gbarom((int) pos);
}

void free_map(){
	iprintf("filemapsectors[%lu] to be freed",fmapsize/sectorsize);
	free(sector_table);
	iprintf("cleaning filemap done!");
}

FILE * gbaromfile;

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
	iprintf("FATAL: GBAFH isn't open");
	return 1;
}

fclose(gbaromfile);

iprintf("GBARom closed!");
return 0;
}

u32 readu32gbarom(int offset){

if(!gbaromfile)
	iprintf("FATAL: GBAFH isn't open");

//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

int sizeread=fread((void*)disk_buf, 1, sectorsize,gbaromfile); //2) perform read (512bytes read (128 reads))

if (sizeread!=sectorsize){
		iprintf("FATAL: GBAREAD isn't (%d) bytes",(int)sectorsize);
	}
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return disk_buf[0];
}


u16 writeu16gbarom(int offset,u16 * buf_in,int size_elem){

if(!gbaromfile){
	iprintf("FATAL: GBAFH isn't open");
	return 1;
}
iprintf("\n trying to write: %x",(unsigned int)buf_in[0x0]);

//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

//size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
int sizewritten=fwrite((u16*)buf_in, 1, size_elem, gbaromfile); //2) perform read (512bytes read (128 reads))
if (sizewritten!=size_elem){
		iprintf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
	}
else{
	iprintf("write ok!");
}
	
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return 0;
}


u32 writeu32gbarom(int offset,u32 * buf_in,int size_elem){

if(!gbaromfile){
	iprintf("FATAL: GBAFH isn't open");
	return 1;
}
iprintf("\n trying to write: %x",(unsigned int)buf_in[0x0]);

//int fseek(FILE *stream, long int offset, int whence);
fseek(gbaromfile,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)

//size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
int sizewritten=fwrite((u32*)buf_in, 1, size_elem, gbaromfile); //2) perform read (512bytes read (128 reads))
if (sizewritten!=size_elem){
		iprintf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
	}
else{
	iprintf("write ok!");
}
	
fseek(gbaromfile,0, SEEK_SET);									//3) and set pointer to what it was

return 0;
}


u32 getfilesizegbarom(){
if(!gbaromfile){
	iprintf("FATAL: GBAFH isn't open");
	return 0;
}
fseek(gbaromfile,0,SEEK_END);
int filesize = ftell(gbaromfile);
fseek(gbaromfile,0,SEEK_SET);

return filesize;
}