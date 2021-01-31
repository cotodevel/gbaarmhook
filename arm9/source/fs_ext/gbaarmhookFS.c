#include "gbaarmhookFS.h"
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "dldi.h"
#include "ipcfifoTGDSUser.h"

#ifdef ROMTEST
#include "rom_pl.h"
#endif

u8 stream_readu8(u32 pos, FILE * curFileHandle){
	#ifndef ROMTEST
		return readu8gbarom(pos, curFileHandle);
	#endif
	#ifdef ROMTEST
		return (u8)*((u8*)&rom_pl[0] + ((pos % (32*1024*1024))/1) );
	#endif
}

u16 stream_readu16(u32 pos, FILE * curFileHandle){
	#ifndef ROMTEST
		return readu16gbarom(pos, curFileHandle);
	#endif
	#ifdef ROMTEST
		return (u16)*((u16*)&rom_pl[0] + ((pos % (32*1024*1024))/2) );
	#endif
}

u32 stream_readu32(u32 pos, FILE * curFileHandle){
	#ifndef ROMTEST
		return readu32gbarom(pos, curFileHandle);
	#endif
	#ifdef ROMTEST
		return (u32)*((u32*)&rom_pl[0] + ((pos % (32*1024*1024))/4) );
	#endif
}

FILE * opengbarom(const char * filename, char * fopenArg){
	FILE *fh = fopen(filename, fopenArg);
	if(fh != NULL){
		fseek(fh, 0, SEEK_SET);
	}
	return fh;
}

u32 closegbarom(FILE * curFileHandle){
	fclose(curFileHandle);
	return 0;
}

u32 readu32gbarom(u32 offset, FILE * curFileHandle){
	u32 value = 0;
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle, (long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	fread((void*)&value, 1, sizeof(value), curFileHandle); 				//2) perform read
	return value;
}

u16 readu16gbarom(u32 offset, FILE * curFileHandle){
	u16 value = 0;
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle, (long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	fread((void*)&value, 1, sizeof(value), curFileHandle); 				//2) perform read
	return value;
}

u8 readu8gbarom(u32 offset, FILE * curFileHandle){
	u8 value = 0;
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle, (long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	fread((void*)&value, 1, sizeof(value), curFileHandle); 				//2) perform read
	return value;
}

int writeu16gbarom(int offset, u16 * buf_in, int size_elem, FILE * curFileHandle){
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizewritten=fwrite((u16*)buf_in, 1, size_elem, curFileHandle); //2) perform read (512bytes read (128 reads))
	if (sizewritten != size_elem){
		return -1;
	}
	return sizewritten;
}

int writeu32gbarom(int offset, u32 * buf_in, int size_elem, FILE * curFileHandle){
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizewritten=fwrite((u32*)buf_in, 1, size_elem, curFileHandle); //2) perform read (512bytes read (128 reads))
	if (sizewritten != size_elem){
		return -1;
	}
	return sizewritten;
}

//Also resets the file internal offset to zero
int getfilesizegbarom(FILE * curFileHandle){
	if(!curFileHandle){
		return -1;
	}
	fseek(curFileHandle, 0, SEEK_END);
	int filesize = ftell(curFileHandle);
	fseek(curFileHandle, 0, SEEK_SET);
	return filesize;
}