/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2010
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#ifndef FILE_BROWSE_H

#include <nds.h>
#include <dirent.h>

#define FILE_BROWSE_H

struct gbaheader_t{
	u32 entryPoint;
	u8 logo[156];
	char title[0xC];
	char gamecode[0x4];
	char makercode[0x2];
	u8 is96h;
	u8 unitcode;
	u8 devicecode;
	u8 unused[7];
	u8 version;
	u8 complement;
	u16 res;
};

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

#ifdef __cplusplus
#include <string>
#endif

#endif //FILE_BROWSE_H

//Define the externs
extern struct gbaheader_t gbaheader;

#ifdef __cplusplus
extern "C" {
#endif

extern u32 fileRead (char* buffer, u32 cluster, u32 startOffset, u32 length);
extern void browseForFile (const char extension);
extern void printgbainfo (const char*);

extern	char temppath[255 * 2];
extern	char biospath[255 * 2];
extern	char savepath[255 * 2];
extern	char patchpath[255 * 2];

#ifdef __cplusplus
}
#endif