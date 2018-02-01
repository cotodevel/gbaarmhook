#define SBRKCACHEOFFSET 0x40000 //256K for sbrk

//#define ROMTEST 		//enables a default, and light gbarom

#define libdir

#define NOBIOS		//skip the GBA.BIOS requirement at startup

//#define DEBUGEMU	//enables LR tracing of every function called

//#define BIOSHANDLER		//Activate this to jump to BIOS and do handling there (can cause problems if bad bios or corrupted)
							//Deactivating this will BX LR as soon as swi code is executed

//#define STACKTEST //enables GBA stack tests (used by cpu virt)
				//if you enable this, disable #DEBUGEMU otherwise overload 2D system

//#define NONDS9HANDLERS //disable: set vectors @ 0xffff0000 // enable : set vectors @0x00000000

//#define OWNIRQSYS //enable for own IRQ handling / disable for LIBNDS IRQ (makefile is recompiled as ARM code, LIBNDS IRQ is THUMB...) so nope

//#define MPURECONFIG //enable: set dcache and mpu map / disable: as libnds provides default NDS MPU setup 
	//<-raises exceptions..
	//useful for enable: you want to detect wrong accesses, but breaks printf output..
	//so 	for disable: you want printf output regardless wrong accesses


//#define NDSTEST //set: for setting a default NDS9/NDS7 callback for stability / testing processors..
				//unset: disables test
//MEMORY {
//       rom     : ORIGIN = 0x08000000, LENGTH = 32M
//       gbarom  : ORIGIN = 0x02180000, LENGTH = 3M -512k
//       gbaew   : ORIGIN = 0x02000000, LENGTH = 256k
//       ewram   : ORIGIN = 0x02040000, LENGTH = 1M - 256k + 512k
//       dtcm    : ORIGIN = 0x027C0000, LENGTH = 16K
//       vectors : ORIGIN = 0x01FF8000, LENGTH = 16K
//       itcm    : ORIGIN = 0x01FFC000, LENGTH = 16K
//}

//ex_s.s <-- stack management from STACKGBA and link to C stacks.
//cr.s <-- arm thumb opcode translator in ... well thumb code