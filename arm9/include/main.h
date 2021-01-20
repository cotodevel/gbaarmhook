#ifndef __main_h__
#define __main_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "fatfslayerTGDS.h"
#include "utilsTGDS.h"

#endif

#ifdef __cplusplus
extern "C" {
#endif

//patches for ARM code
extern u32 PATCH_BOOTCODE();
extern u32 PATCH_START();
extern u32 PATCH_HOOK_START();
extern u32 NDS7_RTC_PROCESS();
extern u32 PATCH_ENTRYPOINT[4];

extern char curChosenBrowseFile[MAX_TGDSFILENAME_LENGTH+1];
extern char biospath[MAX_TGDSFILENAME_LENGTH+1];
extern char savepath[MAX_TGDSFILENAME_LENGTH+1];
extern char patchpath[MAX_TGDSFILENAME_LENGTH+1];
extern int main(int argc, char **argv);

#ifdef __cplusplus
}
#endif
