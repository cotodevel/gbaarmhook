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

#ifndef __main9_h__
#define __main9_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "fatfslayerTGDS.h"
#include "utilsTGDS.h"
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
extern int main(int argc, char **argv);
extern void ApplicationMainLoop();

//TGDS Soundstreaming API
extern int internalCodecType;
extern struct fd * _FileHandleVideo; 
extern struct fd * _FileHandleAudio;
extern bool stopSoundStreamUser();
extern void closeSoundUser();

extern int extract_word(u32 * buffer,u32 word,int size ,u32 * buffer_out,u32 delimiter,u8 type);

#ifdef __cplusplus
}
#endif