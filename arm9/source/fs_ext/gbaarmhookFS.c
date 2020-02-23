#include "gbaarmhookFS.h"
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "dldi.h"
#include "ipcfifoTGDSUSer.h"

#ifdef ROMTEST
#include "rom_pl.h"
#endif

FILE* globalfileHandle=NULL;
int globalfileSize=0;

u8 stream_readu8(u32 pos){
	#ifndef ROMTEST
		return readu8gbarom(pos);
	#endif
	#ifdef ROMTEST
		return (u8)*((u8*)&rom_pl[0] + ((pos % (32*1024*1024))/1) );
	#endif
}

u16 stream_readu16(u32 pos){
	#ifndef ROMTEST
		return readu16gbarom(pos);
	#endif
	#ifdef ROMTEST
		return (u16)*((u16*)&rom_pl[0] + ((pos % (32*1024*1024))/2) );
	#endif
}

u32 stream_readu32(u32 pos){
	#ifndef ROMTEST
		return readu32gbarom(pos);
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

u32 closegbarom(){
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
		return 1;
	}
	fclose(globalfileHandle);
	printf("GBARom closed!");
	return 0;
}

u32 readu32gbarom(u32 offset){
	u32 value = 0;
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
	}
	fseek(globalfileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizeread=fread((void*)&value, 1, sizeof(value), globalfileHandle); //2) perform read (512bytes read (128 reads))
	if (sizeread != sizeof(value)){
		printf("FATAL: GBAREAD isn't (%d) bytes",(int)sizeof(value));
	}
	return value;
}

u16 readu16gbarom(u32 offset){
	u16 value = 0;
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
	}
	fseek(globalfileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizeread=fread((void*)&value, 1, sizeof(value), globalfileHandle); //2) perform read (512bytes read (128 reads))
	if (sizeread != sizeof(value)){
		printf("FATAL: GBAREAD isn't (%d) bytes",(int)sizeof(value));
	}
	return value;
}

u8 readu8gbarom(u32 offset){
	u8 value = 0;
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
	}
	fseek(globalfileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizeread=fread((void*)&value, 1, sizeof(value), globalfileHandle); //2) perform read (512bytes read (128 reads))
	if (sizeread != sizeof(value)){
		printf("FATAL: GBAREAD isn't (%d) bytes",(int)sizeof(value));
	}
	return value;
}

u16 writeu16gbarom(int offset,u16 * buf_in, int size_elem){
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
		return 1;
	}
	fseek(globalfileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizewritten=fwrite((u16*)buf_in, 1, size_elem, globalfileHandle); //2) perform read (512bytes read (128 reads))
	if (sizewritten!=size_elem){
		printf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
	}
	else{
		printf("write ok:%x",(unsigned int)buf_in[0x0]);
	}
	return 0;
}

u32 writeu32gbarom(int offset,u32 * buf_in,int size_elem){
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
		return 1;
	}
	fseek(globalfileHandle,(long int)offset, SEEK_SET); 					//1) from start of file where (offset)
	int sizewritten=fwrite((u32*)buf_in, 1, size_elem, globalfileHandle); //2) perform read (512bytes read (128 reads))
	if (sizewritten!=size_elem){
			printf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
		}
	else{
		printf("write ok:%x",(unsigned int)buf_in[0x0]);
	}
	return 0;
}

int getfilesizegbarom(){
	if(!globalfileHandle){
		printf("FATAL: GBAFH isn't open");
		return 0;
	}
	fseek(globalfileHandle,0,SEEK_END);
	int filesize = ftell(globalfileHandle);
	fseek(globalfileHandle,0,SEEK_SET);
	return filesize;
}