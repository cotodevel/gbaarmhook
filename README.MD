This is the Toolchain Generic DS gbaARMHook project:

Compile Toolchain: To compile this project you will need to follow the steps at https://bitbucket.org/Coto88/toolchaingenericds : Then simply extract the project somewhere.

Compile this project: Open msys, through msys commands head to the directory your extracted this project. Then write: make clean make

After compiling, run the example in NDS.

Project Specific description:

This is a nds homebrew that patches the irq handler of the GBA game:
Pokèmon Zafiro (SPA)
Size:	16,777,216
CRC-32:	5F35977E
MD-5:	3c793a77ea0c6064dc9fb040fb0fb377

The payload injected is in arm9/source/patches_to_arm.s and the ARM assembly function: NDS7_RTC_PROCESS hooks right into the GBA vblank handler.
Because I tried to reach in NDS (gba mode) the RTC. And of course... the reads would return 0xFF (iirc), which means, the RTC in gbamode is mapped to GBA cartridge.
I thought I could share this homebrew because otherwise the effort inested would be wasted.

Put the above listed gba game in sd:/gba/ folder, rename it to : rs-pzs.gba
Compile the sources, it will generate a file called gbaARMHook.nds
Run gbaARMHook.nds from any slot 1 based cart. Beware! as the gba file will be rewritten. After patching is done, you will see onscreen: "GBARom closed!", thus, it's safe to turn off the DS.
Then, the new patched gba file has the routines above described, patched. So it will work in any GBA hardware / emulator!

Coto
