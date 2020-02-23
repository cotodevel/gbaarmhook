This is the Toolchain Generic DS gbaARMHook project:

Compile Toolchain: To compile this project you will need to follow the steps at https://bitbucket.org/Coto88/toolchaingenericds : Then simply extract the project somewhere.

Compile this project: Open msys, through msys commands head to the directory your extracted this project. Then write: make clean make

After compiling, run the example in NDS.

Project Specific description:

Story: Because I tried to reach in NDS (gba mode) the RTC. And of course... the reads would return 0xFF (iirc), which means, the RTC in gbamode is mapped to GBA cartridge.

This is a nds homebrew that patches the VBLANK Irq handler of GBA file:
Pokèmon Zafiro (SPA)
Size:	16,777,216
CRC-32:	5F35977E
MD-5:	3c793a77ea0c6064dc9fb040fb0fb377

The payload injected is at arm9/source/patches_to_arm.s (marker: User space Handler) and the ARM assembly function: NDS7_RTC_PROCESS hooks right into the GBA vblank handler.
You can write your ROP code here. Remember to save and restore context accordingly! Also beware! as the gba file will be rewritten. 
Put the above file in sd:/gba/ folder. Compile the sources, it will generate a file called gbaARMHook.nds, copy to sd.
Run gbaARMHook.nds from any slot 1 based cart. After a while, you will see onscreen: "File patched successfully. Power Off NDS.", by then, it's safe to turn off the DS.
Then, the new patched gba file has the ROP code integrated. And it works in real hardware or emulator.

Coto
