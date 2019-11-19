#include "fs.h"
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
#include "InterruptsARMCores_h.h"
#include "ipcfifoTGDSUser.h"
#include "ff.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

//gbaARMHook specific

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
	if(!fh){
		return 1;
	}
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
	u32 val = 0;
	if(!gbaromfile){
		printf("FATAL: GBAFH isn't open");
	}
	fseek(gbaromfile,(long int)offset, SEEK_SET);
	int sizeread=fread((void*)&val, 1, 4, gbaromfile); 
	if (sizeread!=4){
		printf("FATAL: GBAREAD isn't (%d) bytes",(int)4);
	}
	fseek(gbaromfile,0, SEEK_SET);									
	return val;
}


bool writeu32gbarom(int offset, u32 * buf_in, int size_elem){
	if(!gbaromfile){
		printf("FATAL: GBAFH isn't open");
		return false;
	}
	fseek(gbaromfile,(long int)offset, SEEK_SET);
	int sizewritten=fwrite(buf_in, 1, size_elem, gbaromfile);
	if (sizewritten!=size_elem){
		printf("FATAL: GBAWRITE isn't (%d) bytes, instead: (%x) bytes",(int)size_elem,(int)sizewritten);
		return false;
	}
	else{
		printf("write ok:%x",(unsigned int)buf_in[0x0]);
	}
	fseek(gbaromfile,0, SEEK_SET);
	return true;
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