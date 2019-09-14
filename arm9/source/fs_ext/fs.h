#ifndef __gbaemu4dsfatext_h__
#define __gbaemu4dsfatext_h__

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

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern FILE * gbaromfile;
extern u32 isgbaopen(FILE * gbahandler);
extern u32 opengbarom(const char * filename,const char * access_type);
extern u32 closegbarom();
extern u32 readu32gbarom(int offset);
extern bool writeu32gbarom(int offset, u32 * buf_in, int size_elem);
extern u32 getfilesizegbarom();

#ifdef __cplusplus
}
#endif


