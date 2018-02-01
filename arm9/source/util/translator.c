//coto: this translator is mostly mine work. (except gbareads[ichfly] & cpuread_data from VBA)

#include "translator.h"

#include "opcode.h"
#include "util.h"

#include "pu.h"
#include "supervisor.h"
#include "settings.h"
#include "main.h"

//filesystem
#include "fsfatlayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "specific_shared.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

/*
 GBA Memory Map

General Internal Memory
  00000000-00003FFF   BIOS - System ROM         (16 KBytes)
  00004000-01FFFFFF   Not used
  02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
  02040000-02FFFFFF   Not used
  03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
  03008000-03FFFFFF   Not used
  04000000-040003FE   I/O Registers
  04000400-04FFFFFF   Not used
Internal Display Memory
  05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
  05000400-05FFFFFF   Not used
  06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
  06018000-06FFFFFF   Not used
  07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
  07000400-07FFFFFF   Not used
External Memory (Game Pak)
  08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
  0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
  0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
  0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
  0E010000-0FFFFFFF   Not used
*/

extern struct GBASystem gba;

u32 * cpubackupmode(u32 * branch_stackfp, u32 cpuregvector[0x16], u32 cpsr){

	//if frame pointer reaches TOP don't increase SP anymore
	// (ptrpos - ptrsrc) * int depth < = ptr + size struct (minus last element bc offset 0 uses an element)
	if( (int)( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4) <  (int)(gba_branch_table_size - gba_branch_elemnt_size)){ 
	
	//printf("gba_branch_offst[%x] +",( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4)); //debug
	
		stmiavirt((u8*)(&cpuregvector[0]), (u32)(u32*)branch_stackfp, 0xffff, 32, 0, 0);
		
		//for(i=0;i<16;i++){
		//	printf(">>%d:[%x]",i,cpuregvector[i]);
		//}
		//while(1);
		
		//printf("\n \n \n \n 	--	\n");
		//move 16 bytes ahead fp and store cpsr
		//debug: stmiavirt((u8*)cpsr, (u32)(u32*)(branch_stackfp+0x10), 0x1 , 32, 0);
		
		//printf("curr_used fp:%x / top: %x / block size: %x \n",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
		branch_stackfp=(u32*)addasm((u32)(u32*)branch_stackfp,(u32)0x10*4);
		
		//save cpsr int SPSR_mode slot
		switch(cpsr&0x1f){
			
			case(0x10): //user
				spsr_usr=cpsr;
			break;
			case(0x11): //fiq
				spsr_fiq=cpsr;
			break;
			case(0x12): //irq
				spsr_irq=cpsr;
			break;
			case(0x13): //svc
				spsr_svc=cpsr;
			break;
			case(0x17): //abt
				spsr_abt=cpsr;
			break;
			case(0x1b): //und
				spsr_und=cpsr;
			break;
			case(0x1f): //sys
				spsr_sys=cpsr;
			break;
		}
		//and save spsr this way for fast spsr -> cpsr restore (only for cpusave/restore modes), otherwise save CPSR manually
		gbastckfpadr_spsr=cpsr;
	}
	
	else{ 
		//printf("branch stack OVERFLOW! \n"); //debug
	}

return branch_stackfp;

}
//this one restores cpsrvirt by itself (restores CPU mode)
u32 * cpurestoremode(u32 * branch_stackfp, u32 cpuregvector[0x16]){

	//if frame pointer reaches bottom don't decrease SP anymore:
	//for near bottom case
	if ( (u32*)branch_stackfp > (u32*)&branch_stack[0]) { 
	
	//printf("gba_branch_block_size[%x] -",gba_branch_block_size); //debug

		//printf("curr_used fp:%x / top: %x / block size: %x \n",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
		branch_stackfp=(u32*)subasm((u32)(u32*)branch_stackfp,0x10*4);
		
		ldmiavirt((u8*)(&cpuregvector[0x0]), (u32)(u32*)(branch_stackfp), 0xffff, 32, 0, 0);
		
	}
	
	//if frame pointer reaches bottom don't decrease SP anymore:
	//for bottom case
	else if ( (u32*)branch_stackfp == (u32*)&branch_stack[0]) { 

		//move 4 slots behinh fp (32 bit *) and restore cpsr
		ldmiavirt((u8*)(&cpuregvector[0]), (u32)(u32*)(branch_stackfp), 0xffff, 32, 0, 0);
		
		//don't decrease frame pointer anymore.
		
	}
	
	else{ 
		//printf("branch stack has reached bottom! ");
	}
	
return branch_stackfp;
}

//Update CPU status flags (Z,N,C,V, THUMB BIT)
//mode: 0 = hardware asm cpsr update (cpsrvirt & cpu cond flags) / 1 = virtual CPU mode change,  CPSR , change to CPU mode
// / 2 = Writes to IO MAP for GBA environment variable updates
u32 updatecpuflags(u8 mode ,u32 cpsr , u32 cpumode){
switch(mode){
	case (0):
		//1) if invoked from hardware asm function, update flags to virtual environment
		z_flag=(lsrasm(cpsr,0x1e))&0x1;
		n_flag=(lsrasm(cpsr,0x1f))&0x1;
		c_flag=(lsrasm(cpsr,0x1d))&0x1;
		v_flag=(lsrasm(cpsr,0x1c))&0x1;
		cpsrvirt&=~0xF0000000;
		cpsrvirt|=(n_flag<<31|z_flag<<30|c_flag<<29|v_flag<<28);
		//cpsr = latest cpsrasm from virtual asm opcode
		//printf("(0)CPSR output: %x \n",cpsr);
		//printf("(0)cpu flags: Z[%x] N[%x] C[%x] V[%x] \n",z_flag,n_flag,c_flag,v_flag);
	
	break;
	
  	case (1):{
		//1)check if cpu<mode> swap does not come from the same mode
		if((cpsr&0x1f)!=cpumode){
	
		//2a)save stack frame pointer for current stack 
		//and detect old cpu mode from current loaded stack, then store LR , PC into stack (mode)
		//printf("cpsr:%x / spsr:%x",cpsr&0x1f,spsr_last&0x1f);
		
			if( ((cpsrvirt&0x1f) == (0x10)) || ((cpsrvirt&0x1f) == (0x1f)) ){ //detect usr/sys (0x10 || 0x1f)
				spsr_last=spsr_usr=cpsrvirt;
				
				//gbastckadr_usr=gbastckmodeadr_curr=; //not required this is to know base stack usr/sys
				gbastckfp_usr=gbastckfpadr_curr;
				gbavirtreg_r13usr[0x0]=gbavirtreg_cpu[0xd]; //user/sys is the same stacks
				gbavirtreg_r14usr[0x0]=gbavirtreg_cpu[0xe];
				#ifdef DEBUGEMU
					printf("stacks backup usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13usr[0x0]);
				#endif
				//printf("before nuked SP usr:%x",(unsigned int)gbavirtreg_r13usr[0x0]);
				
				/* //deprecated
					//if framepointer does not reach the top:
					if((u32)updatestackfp(gbastckfp_usr,gbastckadr_usr) != (u32)0){
				
					//store LR to actual (pre-SPSR'd) Stack
					dummyreg=gbavirtreg_cpu[0xe];
					//stmiavirt((u8*)&dummyreg, (u32)(u32*)gbastckfp_usr, 1 << 0xe, 32, 0);
					faststr((u8*)&dummyreg, ((u32*)gbastckfp_usr[0]), (0xe), 32,0);
					
					//store PC to actual (pre-SPSR'd stack)
					dummyreg=(u32)(u32*)rom; //PC
					//stmiavirt((u8*)&dummyreg, (u32)(u32*)gbastckfp_usr+0x1, 1 << 0xf, 32, 0);
					faststr((u8*)&dummyreg, 
					//(u32)(u32*)gbastckfp_usr+0x1
					((u32*)gbastckfp_usr[1])
					, (0xf), 32,0);
					
					//increase fp by the ammount of regs added
					gbastckfp_usr=(u32*)addspvirt((u32)(u32*)gbastckfp_usr,2);
				}
				*/
			}
			else if((cpsrvirt&0x1f)==0x11){
				spsr_last=spsr_fiq=cpsrvirt;
				gbastckfp_fiq=gbastckfpadr_curr;
				gbavirtreg_r13fiq[0x0]=gbavirtreg_cpu[0xd];
				gbavirtreg_r14fiq[0x0]=gbavirtreg_cpu[0xe];
				
				//save
				//5 extra regs r8-r12 for fiq
				gbavirtreg_fiq[0x0] = gbavirtreg_cpu[0x0 + 8];
				gbavirtreg_fiq[0x1] = gbavirtreg_cpu[0x1 + 8];
				gbavirtreg_fiq[0x2] = gbavirtreg_cpu[0x2 + 8];
				gbavirtreg_fiq[0x3] = gbavirtreg_cpu[0x3 + 8];
				gbavirtreg_fiq[0x4] = gbavirtreg_cpu[0x4 + 8];
				
				//restore 5 extra reg subset for other modes
				gbavirtreg_cpu[0x0 + 8]=gbavirtreg_cpubup[0x0];
				gbavirtreg_cpu[0x1 + 8]=gbavirtreg_cpubup[0x1];
				gbavirtreg_cpu[0x2 + 8]=gbavirtreg_cpubup[0x2];
				gbavirtreg_cpu[0x3 + 8]=gbavirtreg_cpubup[0x3];
				gbavirtreg_cpu[0x4 + 8]=gbavirtreg_cpubup[0x4];
				#ifdef DEBUGEMU
					printf("stacks backup fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13fiq[0x0]);
				#endif
			}
			else if((cpsrvirt&0x1f)==0x12){
				spsr_last=spsr_irq=cpsrvirt;
				gbastckfp_irq=gbastckfpadr_curr;
				gbavirtreg_r13irq[0x0]=gbavirtreg_cpu[0xd];
				gbavirtreg_r14irq[0x0]=gbavirtreg_cpu[0xe];
				#ifdef DEBUGEMU
					printf("stacks backup irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13irq[0x0]);
				#endif
			}
			
			else if((cpsrvirt&0x1f)==0x13){
				spsr_last=spsr_svc=cpsrvirt;
				gbastckfp_svc=gbastckfpadr_curr;
				gbavirtreg_r13svc[0x0]=gbavirtreg_cpu[0xd];
				gbavirtreg_r14svc[0x0]=gbavirtreg_cpu[0xe];
				#ifdef DEBUGEMU
					printf("stacks backup svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13svc[0x0]);
				#endif
			}
			else if((cpsrvirt&0x1f)==0x17){
				spsr_last=spsr_abt=cpsrvirt;
				gbastckfp_abt=gbastckfpadr_curr;
				gbavirtreg_r13abt[0x0]=gbavirtreg_cpu[0xd];
				gbavirtreg_r14abt[0x0]=gbavirtreg_cpu[0xe];
				#ifdef DEBUGEMU
					printf("stacks backup abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13abt[0x0]);
				#endif
			}
			
			else if((cpsrvirt&0x1f)==0x1b){
				spsr_last=spsr_und=cpsrvirt;
				gbastckfp_und=gbastckfpadr_curr;
				gbavirtreg_r13und[0x0]=gbavirtreg_cpu[0xd];
				gbavirtreg_r14und[0x0]=gbavirtreg_cpu[0xe];
				#ifdef DEBUGEMU
					printf("stacks backup und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13und[0x0]);
				#endif
			}
			
			//default (no previous PSR) sets
			else {
			
				// disable FIQ & set SVC
				// gba->reg[16].I |= 0x40;
				cpsr|=0x40;
				cpsr|=0x13;
				
				//#ifdef DEBUGEMU
				//	printf("ERROR CHANGING CPU MODE/STACKS : CPSR: %x \n",(unsigned int)cpsrvirt);
				//	while(1);
				//#endif
			}
		
		//update SPSR on CPU change <mode> (this is exactly where CPU change happens)
		spsr_last=cpsr;
		
		//3)setup current CPU mode working set of registers and perform stack swap
		//btw2: gbastckfp_usr can hold up to 0x1ff (511 bytes) of data so pointer must not exceed that value
		
		//case gbastckfp_mode  
		//load LR & PC from regs then decrease #1 for each
		
		//unique cpu registers : gbavirtreg_cpu[n];
		
		//user/sys
		if ( ((cpumode&0x1f) == (0x10)) || ((cpumode&0x1f) == (0x1f)) ){	
				
				gbastckmodeadr_curr=gbastckadr_usr;
				gbastckfpadr_curr=gbastckfp_usr;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13usr[0x0]; //user SP/LR registers for cpu<mode> (user/sys is the same stacks)
				gbavirtreg_cpu[0xe]=gbavirtreg_r14usr[0x0];
				#ifdef DEBUGEMU
					printf("| stacks swap to usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		
		else if((cpumode&0x1f)==0x11){
				gbastckmodeadr_curr=gbastckadr_fiq;
				gbastckfpadr_curr=gbastckfp_fiq;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13fiq[0x0]; //fiq SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14fiq[0x0];
				
				//save register r8-r12 subset before entering fiq
				gbavirtreg_cpubup[0x0]=gbavirtreg_cpu[0x0 + 8];
				gbavirtreg_cpubup[0x1]=gbavirtreg_cpu[0x1 + 8];
				gbavirtreg_cpubup[0x2]=gbavirtreg_cpu[0x2 + 8];
				gbavirtreg_cpubup[0x3]=gbavirtreg_cpu[0x3 + 8];
				gbavirtreg_cpubup[0x4]=gbavirtreg_cpu[0x4 + 8];
				
				//restore: 5 extra regs r8-r12 for fiq restore
				gbavirtreg_cpu[0x0 + 8]=gbavirtreg_fiq[0x0];
				gbavirtreg_cpu[0x1 + 8]=gbavirtreg_fiq[0x1];
				gbavirtreg_cpu[0x2 + 8]=gbavirtreg_fiq[0x2];
				gbavirtreg_cpu[0x3 + 8]=gbavirtreg_fiq[0x3];
				gbavirtreg_cpu[0x4 + 8]=gbavirtreg_fiq[0x4];
				#ifdef DEBUGEMU
					printf("| stacks swap to fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		//irq
		else if((cpumode&0x1f)==0x12){
				gbastckmodeadr_curr=gbastckadr_irq;
				gbastckfpadr_curr=gbastckfp_irq;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13irq[0x0]; //irq SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14irq[0x0];
				#ifdef DEBUGEMU
					printf("| stacks swap to irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		//svc
		else if((cpumode&0x1f)==0x13){
				gbastckmodeadr_curr=gbastckadr_svc;
				gbastckfpadr_curr=gbastckfp_svc;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13svc[0x0]; //svc SP/LR registers for cpu<mode> (user/sys is the same stacks)
				gbavirtreg_cpu[0xe]=gbavirtreg_r14svc[0x0];
				#ifdef DEBUGEMU
					printf("| stacks swap to svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		//abort
		else if((cpumode&0x1f)==0x17){
				gbastckmodeadr_curr=gbastckadr_abt;
				gbastckfpadr_curr=gbastckfp_abt;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13abt[0x0]; //abt SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14abt[0x0];
				#ifdef DEBUGEMU
					printf("| stacks swap to abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		//undef
		else if((cpumode&0x1f)==0x1b){
				gbastckmodeadr_curr=gbastckadr_und;
				gbastckfpadr_curr=gbastckfp_und;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13und[0x0]; //und SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14und[0x0];
				#ifdef DEBUGEMU
					printf("| stacks swap to und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
		}
		
		//then update cpsr (mode) <-- cpsr & spsr case dependant
		cpsr&=~0x1f;
		cpsr|=(cpumode&0x1f);
		
		//->switch to arm/thumb mode depending on cpsr for virtual env
		if( ((cpsr>>5)&1) == 0x1 )
			armstate=0x1;
		else
			armstate=0x0;
		
		cpsr&=~(1<<0x5);
		cpsr|=((armstate&1)<<0x5);
		
		//save changes to CPSR
		cpsrvirt=cpsr;
		
	}	//end if current cpumode != new cpu mode
	
	else{
		#ifdef DEBUGEMU
			printf("cpsr(arg1)(%x) == cpsr(arg2)(%x)",(unsigned int)(cpsr&0x1f),(unsigned int)cpumode);
		#endif
		
		//any kind of access case:
		
		//->can't rewrite SAME cpu<mode> slots!!
		//->switch to arm/thumb mode depending on cpsr
		if( ((cpsr>>5)&1) == 0x1 )
			armstate=0x1;
		else
			armstate=0x0;
		
		cpsr&=~(1<<0x5);
		cpsr|=((armstate&1)<<0x5);
		
		//save changes to CPSR
		cpsrvirt=cpsr;
	}
	
	}
	break;
	
	
	default:
	break;
}

return 0;
}

///////////////////////////////////////THUMB virt/////////////////////////////////////////

u32 __attribute__ ((hot)) disthumbcode(u32 thumbinstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
//drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//testing gba accesses translation to allocated ones
//printf("output: %x \n",addresslookup( 0x070003ff, (u32*)&addrpatches[0],(u32*)&addrfixes[0]));

//debug addrfixes
//int i=0;
//for(i=0;i<16;i++){
//	printf("\n patch : %x",*((u32*)&addrfixes+(i)));
//	if (i==15) printf("\n");
//}

//Low regs
switch(thumbinstr>>11){
	////////////////////////5.1
	//LSL opcode
	case 0x0:
		//extract reg
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&7), 32,0);
		
		dummyreg2=lslasm(dummyreg,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("LSL r%d[%x],r%d[%x],#%x (5.1)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//update desired stack reg	
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	break;
	
	//LSR opcode
	case 0x1: 
		//extract reg
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&7), 32,0);
		
		dummyreg2=lsrasm(dummyreg,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("LSR r%d[%x],r%d[%x],#%x (5.1)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//update desired stack reg	
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	break;
	
	//ASR opcode
	case 0x2:
		//extract reg
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&7), 32,0);
		
		dummyreg2=(u32)asrasm((int)dummyreg,((thumbinstr>>6)&0x1f)); //dummyreg=lslasm(dummyreg,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("ASR r%d[%x],r%d[%x],#%x (5.1)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//update desired stack reg	
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	break;
	
	//5.3
	//mov #imm bit[0-8] / move 8 bit value into rd
	case 0x4:
		//mov
		#ifdef DEBUGEMU
		printf("mov r%d,#0x%x (5.3)\n",(int)((thumbinstr>>8)&0x7),(unsigned int)thumbinstr&0xff);
		#endif
		dummyreg=movasm((u32)(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//update desired stack reg	
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	break;
	
	//cmp / compare contents of rd with #imm bit[0-8] / gba flags affected
	case 0x5:
		//rs
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("cmp r%d[%x],#0x%x (5.3)\n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg,(unsigned int)(thumbinstr&0xff));
		#endif
		cmpasm(dummyreg,(u32)thumbinstr&0xff);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	break;
	
	//add / add #imm bit[7] value to contents of rd and then place result on rd
	case 0x6:
		//rn
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("add r%d[%x], #%x (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg,(unsigned int)(thumbinstr&0xff));
		#endif
		dummyreg=addasm(dummyreg,(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	break;
	
	//sub / sub #imm bit[0-8] value from contents of rd and then place result on rd
	case 0x7:	
	//rn
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("sub r%d[%x], #%x (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg,(unsigned int)(thumbinstr&0xff));
		#endif
		dummyreg=subasm(dummyreg,(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	break;

	//5.6
	//PC relative load WORD 10-bit Imm
	case 0x9:
		dummyreg2=cpuread_word((rom+0x4)+((thumbinstr&0xff)<<2)); //[PC+0x4,#(8<<2)Imm] / because prefetch and alignment
		#ifdef DEBUGEMU
		printf("(WORD) LDR r%d[%x], [PC:%x,#%x] (5.6) \n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg2,(unsigned int)dummyreg,(unsigned int)(thumbinstr&0xff));
		#endif
		//store read onto Rd
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	break;
	
	////////////////////////////////////5.9 LOAD/STORE low reg with #Imm
	
	/* STR RD,[RB,#Imm] */
	case(0xc):{
		//1)read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("\n STR r%d(%x), [r%d(%x),#0x%x] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
		#endif
		//2b) RB = #Imm + RB 
		dummyreg=addsasm(dummyreg,(u32)(((thumbinstr>>6)&0x1f)<<2));
		
		//store RD into [RB,#Imm]
		cpuwrite_word(dummyreg, dummyreg2);
		#ifdef DEBUGEMU
		printf("content @%x:[%x]",(unsigned int)dummyreg,(unsigned int)*((u32*)dummyreg));
		#endif
	return 0;
	}
	break;
	
	/* LDR RD,[RB,#Imm] */
	//warning: small error on arm7tdmi docs (this should be LDR, but is listed as STR) as bit 11 set is load, and unset store
	case(0xd):{ //word quantity (#Imm is 7 bits, filled with bit[0] & bit[1] = 0 by shifting >> 2 )
		#ifdef DEBUGEMU
			printf("\n LDR r%d, [r%d,#0x%x] (5.9)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<2)); //if freeze undo this
		#endif
		
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//get #Imm
		dummyreg4=(((thumbinstr>>6)&0x1f)<<2);
		
		//add with #imm
		dummyreg2=addsasm(dummyreg,dummyreg4);
		
		dummyreg=cpuread_word(dummyreg2);
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//STRB rd, [rs,#IMM] (5.9)
	case(0xe):{
		//1)read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
			printf("\n strb r%d(%x), [r%d(%x),#0x%x] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)((thumbinstr>>6)&0x1f)<<2);
		#endif
		//2b) RB = #Imm + RB 
		dummyreg=addsasm(dummyreg,(u32)(((thumbinstr>>6)&0x1f)<<2));
		
		//store RD into [RB,#Imm]
		cpuwrite_byte(dummyreg,dummyreg2&0xff);
		#ifdef DEBUGEMU
			printf("content @%x:[%x]",(unsigned int)dummyreg,(unsigned int)*((u8*)dummyreg));
		#endif
	return 0;
	}
	break;
	
	//LDRB rd, [Rb,#IMM] (5.9)
	case(0xf):{ //byte quantity (#Imm is 5 bits)
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//get #Imm
		dummyreg2=(((thumbinstr>>6)&0x1f)<<2);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_byte(dummyreg3);
		
		#ifdef DEBUGEMU
			printf("\n ldrb Rd(%d)[%x], [Rb(%d)[%x],#(0x%x)] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
			(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
			(unsigned int)(((thumbinstr>>6)&0x1f)<<2)); //if freeze undo this
		#endif
		
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	/////////////////////5.10
	//store halfword from rd to low reg rs
	// STRH rd, [rb,#IMM] (5.10)
	case(0x10):{
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//RD
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		dummyreg3=addsasm(dummyreg,(((thumbinstr>>6)&0x1f)<<1)); // Rb + #Imm bit[6] depth (adds >>0)
		
		//store RD into [RB,#Imm]
		cpuwrite_hword(dummyreg3,dummyreg2);
		
		#ifdef DEBUGEMU
		printf("strh r(%d)[%x] ,[Rb(%d)[%x],#[%x]] (5.7)\n",(int)((thumbinstr)&0x7),(unsigned int)dummyreg2,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,(unsigned int)(((thumbinstr>>6)&0x1f)<<1));
		#endif
		
	return 0;
	}
	break;
	
	//load halfword from rs to low reg rd
	//LDRH Rd, [Rb,#IMM] (5.10)
	case(0x11):{
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//get #Imm
		dummyreg2=(((thumbinstr>>6)&0x1f)<<1);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_hword(dummyreg3);
		
		#ifdef DEBUGEMU
			printf("\n ldrh Rd(%d)[%x], [Rb(%d)[%x],#(0x%x)] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
			(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
			(unsigned int)(((thumbinstr>>6)&0x1f)<<1)); //if freeze undo this
		#endif
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	/////////////////////5.11
	//STR RD, [SP,#IMM]
	case(0x12):{
		//retrieve SP
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		//RD
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0); 
		
		//#imm 
		dummyreg3=((thumbinstr&0xff)<<2);
		
		#ifdef DEBUGEMU
			printf("str rd(%d)[%x], [SP:(%x),#[%x]] \n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg2,(unsigned int)dummyreg,(unsigned int)dummyreg3);
		#endif
		
		//printf("str: content: %x \n",dummyreg2);
		cpuwrite_word((dummyreg+dummyreg3),dummyreg2);
		
		//note: this opcode doesn't increase SP
	return 0;
	}
	break;
	
	//LDR RD, [SP,#IMM]
	case(0x13):{
		//retrieve SP
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		//#imm 
		dummyreg2=((thumbinstr&0xff)<<2);
		
		dummyreg3=cpuread_word((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
			printf("ldr rd(%d)[%x], [SP:(%x),#[%x]] \n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg3,(unsigned int)dummyreg,(unsigned int)dummyreg2);
		#endif
		
		//save Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		//note: this opcode doesn't increase SP
	return 0;
	}
	break;
	
	
	/////////////////////5.12
	//add #Imm to the current PC value and load the result in rd
	//ADD  Rd, [PC,#IMM] (5.12)	/*VERY HEALTHY AND TESTED GBAREAD CODE*/
	case(0x14):{
		//PC
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xf), 32,0);
		
		//get #Imm
		dummyreg2=((thumbinstr&0xff)<<2);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_word(dummyreg3);
		
		#ifdef DEBUGEMU
			printf("add rd(%d)[%x], [PC:(%x),#[%x]] (5.12) \n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg3,(unsigned int)dummyreg,(unsigned int)dummyreg2);
		#endif
		
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}
	break;
	
	//add #Imm to the current SP value and load the result in rd
	//ADD  Rd, [SP,#IMM] (5.12)
	case(0x15):{	
		//SP
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		
		//get #Imm
		dummyreg2=((thumbinstr&0xff)<<2);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_word(dummyreg3);
		
		#ifdef DEBUGEMU
			printf("add rd(%d)[%x], [r13:(%x),#[%x]] (5.12) \n",(int)((thumbinstr>>8)&0x7),(unsigned int)dummyreg3,(unsigned int)dummyreg,(unsigned int)dummyreg2);
		#endif
		
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}	
	break;
	//coto
	/////////////////////5.15 multiple load store
	//STMIA rb!,{Rlist}
	case(0x18):{
			//Rb
			fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
			
			#ifdef DEBUGEMU
			printf("STMIA r%d![%x], {R: %d %d %d %d %d %d %d %x }:regs op:%x (5.15)\n",
			(int)(thumbinstr>>8)&0x7,
			(unsigned int)dummyreg,
			(int)(thumbinstr&0xff)&0x80,
			(int)(thumbinstr&0xff)&0x40,
			(int)(thumbinstr&0xff)&0x20,
			(int)(thumbinstr&0xff)&0x10,
			(int)(thumbinstr&0xff)&0x08,
			(int)(thumbinstr&0xff)&0x04,
			(int)(thumbinstr&0xff)&0x02,
			(int)(thumbinstr&0xff)&0x01,
			(unsigned int)(thumbinstr&0xff)
			);
			#endif
			
			//deprecated
			//stack operation STMIA
			//stmiavirt( ((u8*)(u32)&gbavirtreg_cpu[0]), (u32)dummyreg2, (thumbinstr&0xff), 32, 3, 0);	//special LDMIA/STMIA mode transfer	
			
			//new
			int cntr=0;	//enum thumb regs
			int offset=0; //enum found regs
			while(cntr<0x8){ //8 working low regs for thumb cpu 
					if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
						//stmia reg! is (forcefully for thumb) descendent
						cpuwrite_word(dummyreg-(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
						offset++;
					}
				cntr++;
			}
			
			//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
			dummyreg=(u32)subsasm((u32)dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4);		//get decimal value from registers selected
			
			//writeback always the new Rb
			faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}
	break;
	
	//LDMIA rd!,{Rlist}
	case(0x19):{
			//Rb
			fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
			
			#ifdef DEBUGEMU
			printf("LDMIA r%d![%x], {R: %d %d %d %d %d %d %d %d }:regs op:%x (5.15)\n",
			(int)((thumbinstr>>8)&0x7),
			(unsigned int)dummyreg2,
			(int)(thumbinstr&0xff)&0x80,
			(int)(thumbinstr&0xff)&0x40,
			(int)(thumbinstr&0xff)&0x20,
			(int)(thumbinstr&0xff)&0x10,
			(int)(thumbinstr&0xff)&0x08,
			(int)(thumbinstr&0xff)&0x04,
			(int)(thumbinstr&0xff)&0x02,
			(int)(thumbinstr&0xff)&0x01,
			(unsigned int)(thumbinstr&0xff)
			);
			#endif
			
			//new
			int cntr=0;	//enum thumb regs
			int offset=0; //enum found regs
			while(cntr<0x8){ //8 working low regs for thumb cpu 
					if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
						//ldmia reg! is (forcefully for thumb) ascendent
						cpuwrite_word(dummyreg+(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
						offset++;
					}
				cntr++;
			}
			//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
			dummyreg=(u32)addsasm((u32)dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4);		//get decimal value from registers selected
			
			//update Rb
			faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}
	break;
	
	
	//				5.18 BAL (branch always) PC-Address (+/- 2048 bytes)
	//must be half-word aligned (bit 0 set to 0)
	case(0x1c):{
			//address patch is required for virtual environment
			//hword=addresslookup(hword, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (hword & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0x3ff)<<1)+0x4; //bit[11] but word-aligned so assembler puts 0>>1
			#ifdef DEBUGEMU
			printf("[BAL] label[%x] THUMB mode / CPSR:%x (5.18) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
	return 0;	
	}
	break;
	
	
	//	5.19 long branch with link
	
	case(0x1e):{
	
		/* //deprecated branch with link
		//1 / 2
		//H bit[11<0]
		//Instruction 1:
		//In the first instruction the offset field contains the upper 11 bits
		//of the target address. This is shifted left by 12 bits and added to
		//the current PC address. The resulting address is placed in LR.
	
		//offset high part
		gbavirtreg_cpu[0xe]=((thumbinstr&0x7ff)<<12)+(rom);
	
		//2 / 2
		//H bit[11<1]
		//Instruction 2:
		//In the second instruction the offset field contains an 11-bit 
		//representation lower half of the target address. This is shifted
		//by 1 bit and added to LR. LR which now contains the full 23-bit
		//address, is placed in PC, the address of the instruction following
		//the BL is placed in LR and bit[0] of LR is set.
	
		//The branch offset must take account of the prefetch operacion, which
		//causes the PC to be 1 word (4 bytes) ahead of the current instruction.
	
		//fetch 2/2
		//(dual u16 reads are broken so we cast a u32 read once again for the sec part)
		#ifndef ROMTEST
			u32 u32read=stream_readu32(rom ^ 0x08000000);
		#endif
		#ifdef ROMTEST
			u32 u32read=(u32)*(u32*)(&rom_pl_bin+((rom ^ 0x08000000)/4 ));
		#endif
		
		u32 temppc=rom;
	
		//rebuild PC from LR + (low part) + prefetch (0x4-2 because PC will +2 after this)
		rom=gbavirtreg_cpu[0xe]+(((u32read>>16)&0x7ff)<<1)+(0x2);
	
		//update LR
		gbavirtreg_cpu[0xe]=((temppc+(0x4)) | (1<<0));
		#ifdef DEBUGEMU
			printf("LONG BRANCH WITH LINK: PC:[%x],LR[%x] (5.19) \n",(unsigned int)(rom+(0x2)),(unsigned int)gbavirtreg_cpu[0xe]);
		#endif
	
		*/
		
		//new branch with link
		
		/* //deprecated in favour of cpuread_word
		#ifndef ROMTEST
			u32 u32read=stream_readu32(rom ^ 0x08000000);
			u32read=rorasm(u32read,0x10); // :) fix
		#endif
		#ifdef ROMTEST
			u32 u32read=(u32)*(u32*)(&rom_pl_bin+((rom ^ 0x08000000)/4 ));
		#endif
		*/
		u32 u32read=0;
		//if only a gbarom area read..
		if( ((rom>>24) == 0x8) || ((rom>>24) == 0x9) ){
			
			//new read method (reads from gba cpu core)
			u32read=cpuread_word(rom);
			
			#ifndef ROMTEST //gbareads (stream) are byte swapped, we swap bytes here if streamed data
				printf("byteswap gbaread! ");
				u32read=rorasm(u32read,0x10);
			#endif
		}
		else{
			
			//new read method (reads from gba cpu core)
			u32read=cpuread_word((rom>>2)<<2);
			
		}
		
		printf("BL rom @(%x):[%x] \n",(unsigned int)rom,(unsigned int)u32read);
		
		//original PC
		u32 oldpc=rom;
		
		//H = 0 (high ofst)
		u32 part1=rom+((((u32read>>16)&0xffff)&0x7ff)<<12);
		gbavirtreg_cpu[0xe]=part1;
		
		//H = 1 (low ofst)
		u32 part2=((((((u32read))&0xffff)&0x7ff) << 1) + gbavirtreg_cpu[0xe] );
		rom = part2+(0x4)-0x2; //prefetch - emulator alignment
		
		gbavirtreg_cpu[0xe]=(oldpc+(0x4)) | 1; //+0x4 for prefetch
		
		#ifdef DEBUGEMU
			printf("LONG BRANCH WITH LINK: PC:[%x],LR[%x] (5.19) \n",
			(unsigned int)(rom+0x2),(unsigned int)gbavirtreg_cpu[0xe]); //+0x2 (pc++ fixup)
		#endif
		
		return 0;
		
	break;
	}
}

switch(thumbinstr>>9){
	
	//5.2
	//add rd, rs , rn
	case(0xc):{
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0); 

		//rn
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("add rd(%d),rs(%d)[%x],rn(%d)[%x] (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg2=addasm(dummyreg2,dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	
	//sub rd, rs, rn
	case(0xd):{
	
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//rn
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("sub r%d,r%d[%x],r%d[%x] (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg2=subasm(dummyreg2,dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//add rd, rs, #imm
	case(0xe):{
	
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("add r%d,r%d[%x],#0x%x (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		dummyreg2=addasm(dummyreg2,(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//sub rd, rs, #imm
	case(0xf):{
	
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("sub r(%d),r(%d)[%x],#0x%x (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		dummyreg2=subasm(dummyreg2,(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	
	/////////////////////5.7
	
	//STR RD, [Rb,Ro]
	case(0x28):{ //40dec
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//dummyreg4=addsasm(dummyreg,dummyreg2);
		
		#ifdef DEBUGEMU
		printf("str rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//store RD into [RB,#Imm]
		cpuwrite_word((dummyreg+dummyreg2),dummyreg3);
	return 0;
	}	
	break;
	
	//STRB RD ,[Rb,Ro] (5.7) (little endian lsb <-)
	case(0x2a):{ //42dec
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//dummyreg4=addsasm(dummyreg,dummyreg2);
		
		#ifdef DEBUGEMU
		printf("strb rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//store RD into [RB,#Imm]
		cpuwrite_byte((dummyreg+dummyreg2),(dummyreg3&0xff));
	return 0;
	}	
	break;
	
	
	//LDR rd,[rb,ro] (correct method for reads)
	case(0x2c):{ //44dec
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_word((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		printf("LDR rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ldrb rd,[rb,ro]
	case(0x2e):{ //46dec
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_byte((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		printf("LDRB rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	
	//////////////////////5.8
	//halfword
	//printf("STRH RD ,[Rb,Ro] (5.8) \n"); //thumbinstr
	case(0x29):{ //41dec strh
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		#ifdef DEBUGEMU
		printf("strh rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//store RD into [RB,#Imm]
		cpuwrite_hword((dummyreg+dummyreg2),(dummyreg3&0xffff));	
	return 0;
	}	
	break;
	
	// LDRH RD ,[Rb,Ro] (5.8)\n
	case(0x2b):{ //43dec ldrh
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_hword((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		printf("LDRB rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//LDSB RD ,[Rb,Ro] (5.8)
	
	case(0x2d):{ //45dec ldsb	
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		s8 sbyte=cpuread_byte(dummyreg+dummyreg2);
		
		#ifdef DEBUGEMU
		printf("ldsb rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(signed int)sbyte,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2);
		#endif
		
		//Rd
		faststr((u8*)&sbyte, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//LDSH RD ,[RS0,RS1] (5.8) //kept to use hardware opcode
	case(0x2f):{ //47dec ldsh
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		s16 shword=cpuread_hword(dummyreg+dummyreg2);
		
		#ifdef DEBUGEMU
		printf("ldsh rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(signed int)shword,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2);
		#endif
		
		//Rd
		faststr((u8*)&shword, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
}

switch(thumbinstr>>8){
	///////////////////////////5.14
	//b: 10110100 = PUSH {Rlist} low regs (0-7)
	case(0xB4):{
		
		//gba stack method (stack pointer) / requires descending pointer
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		#ifdef DEBUGEMU
		printf("[THUMB] PUSH {R: %d %d %d %d %d %d %d %x }:regout:%x (5.14)\n",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff))
		); 
		#endif
		//new
			int cntr=0;	//enum thumb regs
			int offset=0; //enum found regs
			while(cntr<0x8){ //8 working low regs for thumb cpu 
					if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
						//ldmia reg! is (forcefully for thumb) descendent
						cpuwrite_word(dummyreg-(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
						offset++;
					}
				cntr++;
			}
			
		//full descending stack
		dummyreg=subsasm(dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4); 
		
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (0xd) , 32,0);
		
	return 0;
	}
	break;
	
	//b: 10110101 = PUSH {Rlist,LR}  low regs (0-7) & LR
	case(0xB5):{
		
		//gba r13 descending stack operation
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x9){ //8 working low regs for thumb cpu 
			if(cntr!=0x8){
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//push is descending stack
					cpuwrite_word(dummyreg-(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
					offset++;
				}
			}
			else{ //our lr operator
				cpuwrite_word(dummyreg-(offset*4), gbavirtreg_cpu[0xe]); //word aligned
				//#ifdef DEBUGEMU
				//	printf("offset(%x):LR! ",(int)cntr);
				//#endif
			}
		cntr++;
		}
		
		dummyreg=subsasm(dummyreg,(lutu16bitcnt(thumbinstr&0xff)+1)*4); //+1 because LR push
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (0xd) , 32,0);
		
		#ifdef DEBUGEMU
		printf("[THUMB] PUSH {R: %x %x %x %x %x %x %x %x },LR :regout:%x (5.14)\n",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff)+0x1) //because LR
		);
		#endif
		
	return 0;
	}
	break;
	
	//b: 10111100 = POP  {Rlist} low regs (0-7)
	case(0xBC):{
		//gba r13 ascending stack operation
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		dummyreg=addsasm(dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4);
		
		int cntr=0;
		int offset=0;
		while(cntr<0x8){ //8 working low regs for thumb cpu
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
				//pop is ascending
				gbavirtreg_cpu[(1<<cntr)]=cpuread_word(dummyreg+(offset*4)); //word aligned
				offset++;
			}
			cntr++;
		}
		
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (0xd) , 32,0);
	return 0;
	}
	break;
	
	//b: 10111101 = POP  {Rlist,PC} low regs (0-7) & PC
	case(0xBD):{
		//gba r13 ascending stack operation
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		
		//restore is ascending (so fixup offset to restore n registers)
		dummyreg=addsasm(dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4);
		
		int cntr=0;
		int offset=0;
		while(cntr<0x9){ //8 working low regs for thumb cpu
			if(cntr!=0x8){
				if(((1<<cntr) & (thumbinstr&0xff)) > 0){
					//restore is ascending (so Fixup stack offset address, to restore n registers)
					gbavirtreg_cpu[(1<<cntr)]=cpuread_word(dummyreg+(offset*4)); //word aligned
					offset++;
				}
			}
			else{//our pc operator
				rom=gbavirtreg_cpu[0xf]=cpuread_word(dummyreg+(offset*4)); //word aligned
			}
			cntr++;
		}
	return 0;
	}
	break;
	
	
	///////////////////5.16 Conditional branch
	//b: 1101 0000 / BEQ / Branch if Z set (equal)
	//BEQ label (5.16)
	
	//these read NZCV VIRT FLAGS (this means opcodes like this must be called post updatecpuregs(0);
	
	case(0xd0):{
		if (z_flag==1){
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BEQ] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BEQ not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0001 / BNE / Branch if Z clear (not equal)
	//BNE label (5.16)
	case(0xd1):{
		if (z_flag==0){ 
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BNE] label[%x] THUMB mode (5.16) \n",(unsigned int)rom); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BNE not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0010 / BCS / Branch if C set (unsigned higher or same)
	//BCS label (5.16)
	case(0xd2):{
		if (c_flag==1){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BCS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BCS not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0011 / BCC / Branch if C unset (lower)
	case(0xd3):{
		if (c_flag==0){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
	
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BCC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BCC not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0100 / BMI / Branch if N set (negative)
	case(0xd4):{
		if (n_flag==1){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BMI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
	
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BMI not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0101 / BPL / Branch if N clear (positive or zero)
	case(0xd5):{
		if (n_flag==0){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BPL] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BPL not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0110 / BVS / Branch if V set (overflow)
	case(0xd6):{
		if (v_flag==1){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BVS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BVS not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0111 / BVC / Branch if V unset (no overflow)
	case(0xd7):{
		if (v_flag==0){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BVC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BVC not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1000 / BHI / Branch if C set and Z clear (unsigned higher)
	case(0xd8):{
		if ((c_flag==1)&&(z_flag==0)){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BHI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else{
			#ifdef DEBUGEMU
			printf("THUMB: BHI not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1001 / BLS / Branch if C clr or Z Set (lower or same [zero included])
	case(0xd9):{
		if ((c_flag==0)||(z_flag==1)){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;	
			
			#ifdef DEBUGEMU
			printf("[BLS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLS not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1010 / BGE / Branch if N set and V set, or N clear and V clear
	case(0xda):{
		if ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BGE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BGE not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1011 / BLT / Branch if N set and V clear, or N clear and V set
	case(0xdb):{
		if ( ((n_flag==1)&&(v_flag==0)) || ((n_flag==0)&&(v_flag==1)) ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);	
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BLT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLT not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1100 / BGT / Branch if Z clear, and either N set and V set or N clear and V clear
	case(0xdc):{
		if ( (z_flag==0) && ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ) ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BGT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BGT not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1101 / BLE / Branch if Z set, or N set and V clear, or N clear and V set (less than or equal)
	case(0xdd):{	
		if ( ((z_flag==1) || (n_flag==1)) && ((v_flag==0) || ((n_flag==0) && (v_flag==1)) )  ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			rom=cpuread_word((thumbinstr&0xff)<<1)+0x4;
			
			#ifdef DEBUGEMU
			printf("[BLE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLE not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	
	//5.17 SWI software interrupt changes into ARM mode and uses SVC mode/stack (SWI 14)
	//SWI #Value8 (5.17)
	case(0xDF):{
		
		//printf("[thumb 1/2] SWI #(%x)",(unsigned int)cpsrvirt);
		gba.armirqenable=false;
		
		u32 stack2svc=gbavirtreg_cpu[0xe];	//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//ori: updatecpuflags(1,temp_arm_psr,0x13);
		updatecpuflags(1,cpsrvirt,0x13);
		
		gbavirtreg_cpu[0xe]=stack2svc;		//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//we force ARM mode directly regardless cpsr
		armstate=0x0; //1 thmb / 0 ARM
		
		//printf("[thumb] SWI #0x%x / CPSR: %x(5.17)\n",(thumbinstr&0xff),cpsrvirt);
		swi_virt((thumbinstr&0xff));
		
		//if we don't use the BIOS handling, restore CPU mode inmediately
		#ifndef BIOSHANDLER
			//Restore CPU<mode> / SPSR (spsr_last) keeps SVC && restore SPSR T bit (THUMB/ARM mode)
				//note cpsrvirt is required because we validate always if come from same PSR mode or a different. (so stack swaps make sense)
			updatecpuflags(1,cpsrvirt | (((spsr_last>>5)&1)),spsr_last&0x1F);
		#endif
		
		//-0x2 because PC THUMB rom alignment / -0x2 because prefetch
		#ifdef BIOSHANDLER
			rom  = (u32)(0x08-0x2-0x2);
		#else
			//otherwise executes a possibly BX LR (callback ret addr) -> PC increases correctly later
			//rom = gbavirtreg_cpu[0xf] = (u32)(gbavirtreg_cpu[0xe]-0x2-0x2);
		#endif
		
		gba.armirqenable=true;
		
		//restore correct SPSR (deprecated because we need the SPSR to remember SVC state)
		//spsr_last=spsr_old;
		
		//printf("[thumb 2/2] SWI #(%x)",(unsigned int)cpsrvirt);
		
		//swi 0x13 (ARM docs)
		
	return 0;	
	}
	break;
}

switch(thumbinstr>>7){
	////////////////////////////5.13
	case(0x160):{ //dec 352 : add bit[9] depth #IMM to the SP, positive offset 	
		//gba stack method
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		#ifdef DEBUGEMU
		printf("ADD SP:%x, +#%d (5.13) \n",(unsigned int)dummyreg,(signed int)dbyte_tmp);
		#endif
		
		dummyreg2=addsasm(dummyreg,dbyte_tmp);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (0xd), 32,0); 
	return 0;
	}	
	break;
	
	case(0x161):{ //dec 353 : add bit[9] depth #IMM to the SP, negative offset
		//gba stack method
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		#ifdef DEBUGEMU
			printf("ADD SP:%x, -#%d (5.13) \n",(unsigned int)dummyreg,(signed int) dbyte_tmp);
		#endif
		
		dummyreg2=subsasm(dummyreg,dbyte_tmp);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (0xd), 32,0); 
	return 0;
	}
	break;
	
}

switch(thumbinstr>>6){
	//5.4
	//ALU OP: AND rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x100):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: AND r%d[%x], r%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=andasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}	
	break;
	
	//ALU OP: EOR rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x101):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: EOR rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=eorasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
	faststr((u8*)&dummyreg,gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: LSL rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x102):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: LSL r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=lslasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);	
	return 0;
	}
	break;
	
	//ALU OP: LSR rd, rs
	case(0x103):{
	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: LSR r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=lsrasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: ASR rd, rs (5.4)
	case(0x104):{
	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: ASR r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=asrasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: ADC rd, rs (5.4)
	case(0x105):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: ADC r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=adcasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: SBC rd, rs (5.4)
	case(0x106):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: SBC r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=sbcasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: ROR rd, rs (5.4)
	case(0x107):{
	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: ROR r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=rorasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: TST rd, rs (5.4) (and with cpuflag output only)
	case(0x108):{
	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: TST rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=tstasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
	return 0;
	}	
	break;
	
	//ALU OP: NEG rd, rs (5.4)
	case(0x109):{	
	
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		dummyreg=negasm(dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		#ifdef DEBUGEMU
		printf("ALU OP: NEG rd(%d)[%x], rs%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: CMP rd, rs (5.4)
	case(0x10a):{	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: CMP rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=cmpasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
	return 0;
	}
	break;
	
	//ALU OP: CMN rd, rs (5.4)
	case(0x10b):{	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: CMN rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=cmnasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
	return 0;
	}
	break;
	
	//ALU OP: ORR rd, rs (5.4)
	case(0x10c):{	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: ORR r%d[%x], r%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=orrasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: MUL rd, rs (5.4)
	case(0x10d):{	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: MUL r%d[%x], r%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=mulasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: BIC rd, rs (5.4)
	case(0x10e):{	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("ALU OP: BIC r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=bicasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//ALU OP: MVN rd, rs (5.4)
	case(0x10f):{	
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		dummyreg=mvnasm(dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		printf("ALU OP: MVN rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		//printf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	
	//high regs <-> low regs
	////////////////////////////5.5
	//ADD rd,hs (5.5)
	case(0x111):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+8), 32,0);
		
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		#ifdef DEBUGEMU
		printf("HI reg ADD rd(%d)[%x], hs(%d)[%x] (5.5)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+8),(unsigned int)dummyreg2);
		#endif
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;

	//ADD hd,rs (5.5)	
	case(0x112):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		#ifdef DEBUGEMU
		printf("HI reg op ADD hd%d[%x], rs%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+8),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
	return 0;
	}
	break;
	
	//ADD hd,hs (5.5) 
	case(0x113):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		printf("HI reg op ADD hd%d[%x], hs%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
	return 0;
	}
	break;
	
	//CMP rd,hs (5.5)
	case(0x115):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		printf("HI reg op CMP rd%d[%x], hs%d[%x] (5.5)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);	
	return 0;
	}
	break;
	
	//CMP hd,rs (5.5)
	case(0x116):{
	
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("HI reg op CMP hd(%d)[%x], rs(%d)[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
	return 0;
	}
	break;
	
	//CMP hd,hs (5.5)  /* only CMP opcodes set CPSR flags */
	case(0x117):{
		
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		printf("HI reg op CMP hd%d[%x], hd%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x \n",cpsrvirt);
	return 0;
	}
	break;
	
	//MOV
	//MOV rd,hs (5.5)	
	case(0x119):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (((thumbinstr>>0x3)&0x7)+0x8), 32,0);
		
		#ifdef DEBUGEMU
		printf("mov rd(%d),hs(%d)[%x] \n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)dummyreg);
		#endif
		
		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	//MOV Hd,Rs (5.5) 
	case(0x11a):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7), 32,0);
		#ifdef DEBUGEMU
		printf("mov hd%d,rs%d[%x] \n",(int)(((thumbinstr)&0x7)+0x8),(int)(thumbinstr>>0x3)&0x7,(unsigned int)dummyreg);
		#endif

		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2,gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
	return 0;
	}
	break;
	
	//MOV hd,hs (5.5)
	case(0x11b):{
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (((thumbinstr>>0x3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		printf("mov hd(%d),hs(%d)[%x] \n",(int)(((thumbinstr)&0x7)+0x8),(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)dummyreg);
		#endif
		
		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
	}
	break;
	
	//						thumb BX branch exchange (rs)
	case(0x11c):{
		//rs
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7), 32,0);
		
		//unlikely (r0-r7) will never be r15
		if(((thumbinstr>>0x3)&0x7)==0xf){
			//this shouldnt happen!
			#ifdef DEBUGEMU
				printf("thumb BX tried to be PC! (from RS) this is not supposed to HAPPEN!");
			#endif
			while(1);
		}
		
		u32 temppsr;
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((dummyreg&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&0x1f);
	
		rom=(u32)(dummyreg&0xfffffffe);
	
		#ifdef DEBUGEMU
			printf("BX rs(%d)[%x]! cpsr:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)dummyreg,(unsigned int)temppsr);
		#endif
	
		return 0;
	}
	break;
	
	//						thumb BX (hs)
	case(0x11D):{
		//hs
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7)+0x8, 32,0);
		
		//BX PC sets bit[0] = 0 and adds 4
		if((((thumbinstr>>0x3)&0x7)+0x8)==0xf){
			dummyreg+=0x2; //+2 now because thumb prefetch will later add 2 more
		}
		
		u32 temppsr;
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((dummyreg&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&0x1f);
	
		rom=(u32)((dummyreg&0xfffffffe)-0x2); //prefetch & align two boundaries
	
		#ifdef DEBUGEMU
		printf("BX hs(%d)[%x]! cpsr:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)dummyreg,(unsigned int)temppsr);
		#endif
		return 0;
	}
	break;
}

//default:
//printf("unknown OP! %x\n",thumbinstr>>9); //debug
//break;


return thumbinstr;
}

/////////////////////////////////////ARM virt/////////////////////////////////////////
//5.1
u32 __attribute__ ((hot)) disarmcode(u32 arminstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
//drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//validate conditional execution flags:
switch(dummyreg5=((arminstr>>28)&0xf)){
case(0):
	//z set EQ (equ)
	if(z_flag!=1){ //already cond_mode == negate current status (wrong)
		#ifdef DEBUGEMU
		printf("EQ not met! ");
		#endif
		return 0;
	}
	
break;

case(1):
//z clear NE (not equ)
	if(z_flag!=0){
		#ifdef DEBUGEMU
		printf("NE not met!");
		#endif
		return 0;
	}
break;

case(2):
//c set CS (unsigned higher)
	if(c_flag!=1) {
		#ifdef DEBUGEMU
		printf("CS not met!");
		#endif
		return 0;
	}
break;

case(3):
//c clear CC (unsigned lower)
	if(c_flag!=0){
		#ifdef DEBUGEMU
		printf("CC not met!");
		#endif
		return 0;
	}
break;

case(4):
//n set MI (negative)
	if(n_flag!=1){
		#ifdef DEBUGEMU
		printf("MI not met!");
		#endif
		return 0;
	}
break;

case(5):
//n clear PL (positive or zero)
	if(n_flag!=0) {
		#ifdef DEBUGEMU
		printf("PL not met!");
		#endif
		return 0;
	}
break;

case(6):
//v set VS (overflow)
	if(v_flag!=1) {
		#ifdef DEBUGEMU
		printf("VS not met!");
		#endif
		return 0;
	}
break;

case(7):
//v clear VC (no overflow)
	if(v_flag!=0){
		#ifdef DEBUGEMU
		printf("VC not met!");
		#endif
		return 0;
	}
break;

case(8):
//c set and z clear HI (unsigned higher)
	if((c_flag!=1)&&(z_flag!=0)){
		#ifdef DEBUGEMU
		printf("HI not met!");
		#endif
		return 0;
	}
break;

case(9):
//c clear or z set LS (unsigned lower or same)
	if((c_flag!=0)||(z_flag!=1)){
		#ifdef DEBUGEMU
		printf("LS not met!");
		#endif
		return 0;
	}
break;

case(0xa):
//(n set && v set) || (n clr && v clr) GE (greater or equal)
	if( ((n_flag!=1) && (v_flag!=1)) || ((n_flag!=0) && (v_flag!=0)) ) {
		#ifdef DEBUGEMU
		printf("GE not met!");
		#endif
		return 0;
	}
break;

case(0xb):
//(n set && v clr) || (n clr && v set) LT (less than)
	if( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ){
		#ifdef DEBUGEMU
		printf("LT not met!");
		#endif
		return 0;
	}
break;

case(0xc):
// (z clr) && ((n set && v set) || (n clr && v clr)) GT (greater than)
if( (z_flag!=0) && ( ((n_flag!=1) && (v_flag!=1))  || ((n_flag!=0) && (v_flag!=0)) ) ) {
	#ifdef DEBUGEMU
	printf("CS not met!");
	#endif
	return 0;
}
break;

case(0xd):
//(z set) || ((n set && v clear) || (n clr && v set)) LT (less than or equ)
if( (z_flag!=1) || ( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ) ) {
	#ifdef DEBUGEMU
	printf("CS not met!");
	#endif
	return 0;
}
break;

case(0xe): 
//always AL
break;

case(0xf):
//never NV
	return 0;
break;

default:
	return 0;
break;

}

//5.3 Branch & Branch with Link
switch((dummyreg=(arminstr)) & 0xff000000){

	//EQ equal
	case(0x0a000000):{
		if (z_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BEQ ");
			#endif
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	}
	break;
	
	//NE not equal
	case(0x1a000000):
		if (z_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BNE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CS unsigned higher or same
	case(0x2a000000):
		if (c_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BCS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CC unsigned lower
	case(0x3a000000):
		if (c_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BCC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//MI negative
	case(0x4a000000):
	if (n_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BMI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//PL Positive or Zero
	case(0x5a000000):
		if (n_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BPL ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VS overflow
	case(0x6a000000):
		if (v_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BVS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VC no overflow
	case(0x7a000000):
		if (v_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BVC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//HI usigned higher
	case(0x8a000000):
		if ( (c_flag==1) && (z_flag==0) ){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BHI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LS unsigned lower or same
	case(0x9a000000):
		if ( (c_flag==0) || (z_flag==1) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BLS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GE greater or equal
	case(0xaa000000):
		if ( ((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BGE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LT less than
	case(0xba000000):
		if ( ((n_flag==1) && (v_flag==0)) || ((n_flag==0) && (v_flag==1)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BLT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GT greather than
	case(0xca000000):
		if ( (z_flag==0) && (((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0))) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BGT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LE less than or equal
	case(0xda000000):
		if ( 	((z_flag==0) || ( (n_flag==1) && (v_flag==0))) 
				||
				((n_flag==0) && (v_flag==1) )
			){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
			printf("(5.3) BLE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//AL always
	case(0xea000000):{
		
		//new
		s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom+(0x8)); //+/- 32MB of addressing for branch offset / prefetch is considered here
		//after that LDR is required (requires to be loaded on a register).
		//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
		rom=(u32)s_word-(0x4); //fixup next IP handled by the emulator
		
		
		#ifdef DEBUGEMU
		printf("(5.3) BAL ");
		#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
	}
	return 0;
	break;
	
	//NV never
	case(0xf):
	
	#ifdef DEBUGEMU
	printf("(5.3) BNV ");
	#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
	return 0;
	break;
	
}

//ARM 5.3 b (Branch exchange) BX
switch(((arminstr) & 0x012fff10)){
	case(0x012fff10):
	//coto
	//Rn
	fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr)&0xf), 32,0); 
	
	if(((arminstr)&0xf)==0xf){
		printf("BX rn%d[%x]! PC is undefined behaviour!",(int)((arminstr)&0xf),(unsigned int)dummyreg);
		while(1);
	}
	u32 temppsr;
	
	gbavirtreg_cpu[0xf]=rom=(u32)dummyreg;
	
	//(BX ARM-> THUMB value will always add +4)
	if((dummyreg&0x1)==1){
		//bit[0] RAM -> bit[5] PSR
		temppsr=((dummyreg&0x1)<<5)	| cpsrvirt;		//set bit[0] rn -> PSR bit[5]
		rom&=~(1<<0); //align to log2 (because memory access/struct are always 4 byte)
		rom+=(0x2);   //align for next THUMB opcode
	}
	else{
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5]
		//alignment for ARM case is done below
	}
	
	//due ARM's ARM mode post +0x4 opcode execute, instruction pointer is reset
	rom-=(0x4);
	
	//set CPU <mode> (included bit[5])
	updatecpuflags(1,temppsr,temppsr&0x1f);
	
	#ifdef DEBUGEMU
	printf("BX rn(%d)[%x]! set_psr:%x",(int)((arminstr)&0xf),(unsigned int)dummyreg,(unsigned int)temppsr);
	#endif
	
	return 0;
	break;
}

//5.4 ARM
u8 isalu=0;
//1 / 2 step for executing ARM opcode
//extract bit sequence of bit[20] && bit[25] and perform count
if (((arminstr>>26)&3)==0) {
	
	switch((arminstr>>20)&0x21){
		
		case(0x0):{
			
			//prevent MSR/MRS opcodes mistakenly call 5.4
			if( 
			((arminstr&0x3f0000) == 0xf0000) ||
			((arminstr&0x3ff000) == 0x29f000) ||
			((arminstr&0x3ff000) == 0x28f000 )
			){
				break;
			}
			//it is opcode such..
			//MOV register!
			if(((arminstr>>21)&0xf)==0xd){
				isalu=1;
			}
		}
		
		case(0x1):{
			setcond_arm=1;
			immop_arm=0;
			isalu=1;
		}
		break;
		case(0x20):{
			setcond_arm=0;
			immop_arm=1;
			isalu=1;
		}
		break;
		case(0x21):{
			setcond_arm=1;
			immop_arm=1;
			isalu=1;
		}
		break;
	}
	//printf("ARM opcode output %x \n",((arminstr>>20)&0x21));
}
//printf("s:%d,i:%d \n",setcond_arm,immop_arm);


//2 / 2 process ARM opcode by set bits earlier
if(isalu==1){

//printf("ARM opcode output %x \n",((arminstr>>21)&0xf));
	
	switch((arminstr>>21)&0xf){
	
		//AND rd,rs / AND rd,#Imm
		case(0x0):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=andasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("AND rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
				(int)(arminstr>>12)&0xf,(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));

					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=andasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("AND rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)(arminstr>>12)&0xf,(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)(arminstr)&0xf,(unsigned int)dummyreg3
				);
				#endif
			}
		
		//check for S bit here and update (virt<-asm) processor flags
		if(setcond_arm==1)
			updatecpuflags(0,cpsrasm,0x0);
		
		return 0;
		}
		break;
	
		//EOR rd,rs
		case(0x1):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=eorasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("EOR rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
				(int)(arminstr>>12)&0xf,(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=eorasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("EOR rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)(arminstr>>12)&0xf,(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
		return 0;
		}
		break;
	
		//sub rd,rs
		case(0x2):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=subasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("SUB rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=subasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("SUB rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
		return 0;
		}
		break;
	
		//rsb rd,rs
		case(0x3):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=rsbasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("RSB rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=rsbasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("RSB rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
		return 0;
		}
		break;
	
		//add rd,rs (addarm)
		case(0x4):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				//PC directive (+0x8 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					//printf("[imm]PC fetch!");
					dummyreg2+=0x8;
					dummyreg4=addasm(dummyreg,dummyreg2); //+0x8 for prefetch
				}
				else{
					dummyreg4=addasm(dummyreg,dummyreg2);
				}
				//dummyreg4=0xc0707070;
				//rd destination reg	 bit[15]---bit[12] ((arminstr>>12)&0xf)
				
				//faststr((u8*)&dummyreg4, gbavirtreg_cpu,((arminstr>>12)&0xf) , 32,0);
				
				gbavirtreg_cpu[(arminstr>>12)&0xf]=dummyreg4;
				#ifdef DEBUGEMU
				printf("ADD rd%d[%x]",(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[(arminstr>>12)&0xf]);
				#endif
				return 0;
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d),rs(%x)[%x] \n",(int)dummyreg3,(int)((dummyreg2>>4)&0xf),(int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4,gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d),rs(%d)[%x] \n",(int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d),rs(%d)[%x] \n",(int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d),rs(%d)[%x] \n",(int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
		
				//PC directive (+0x12 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					#ifdef DEBUGEMU
					printf("[reg]PC fetch!");
					#endif
					
					dummyreg3=addasm(dummyreg,dummyreg3+(0x12)); //+0x12 for prefetch
			
					//check for S bit here and update (virt<-asm) processor flags
					if(setcond_arm==1)
					updatecpuflags(0,cpsrasm,0x0);
				}
				else{
					dummyreg3=addasm(dummyreg,dummyreg3);
					//check for S bit here and update (virt<-asm) processor flags
					if(setcond_arm==1)
						updatecpuflags(0,cpsrasm,0x0);
				}
				
				//rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,1);
				gbavirtreg_cpu[(arminstr>>12)&0xf]=dummyreg3;
				
				#ifdef DEBUGEMU
				printf("ADD rd(%d)<-rn(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)((arminstr>>12)&0xf),
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
				
			}
		return 0;
		}
		break;
	
		//adc rd,rs
		case(0x5):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=adcasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("ADC rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=adcasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("ADC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//sbc rd,rs
		case(0x6):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=sbcasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("SBC rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4,gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=sbcasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("SBC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//rsc rd,rs
		case(0x7):{
			
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=rscasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("RSC rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] \n",(unsigned int)dummyreg3,(unsigned int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] \n",(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=rscasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("RSC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//tst rd,rs //set CPSR
		case(0x8):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=tstasm(dummyreg,dummyreg2);
				#ifdef DEBUGEMU
				printf("TST [and] rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				dummyreg2=tstasm(dummyreg,dummyreg3);
				#ifdef DEBUGEMU
				printf("TST [%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//teq rd,rs //set CPSR
		case(0x9):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg2=teqasm(dummyreg,dummyreg2);
				#ifdef DEBUGEMU
				printf("TEQ rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				teqasm(dummyreg,dummyreg3);
				#ifdef DEBUGEMU
				printf("TEQ rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//cmp rd,rs //set CPSR
		case(0xa):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				cmpasm(dummyreg,dummyreg2);
				#ifdef DEBUGEMU
				printf("CMP rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				cmpasm(dummyreg,dummyreg3);
				#ifdef DEBUGEMU
				printf("CMP rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//cmn rd,rs //set CPSR
		case(0xb):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				cmnasm(dummyreg,dummyreg2);
				#ifdef DEBUGEMU
				printf("CMN rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				cmnasm(dummyreg,dummyreg3);
				#ifdef DEBUGEMU
				printf("CMN rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//orr rd,rs	//set CPSR
		case(0xc):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=orrasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("ORR rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=orrasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("ORR rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//mov rd,rs
		case(0xd):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				
				dummyreg=movasm(dummyreg2);
				#ifdef DEBUGEMU
				printf("MOV rn(%d)[%x],#Imm[%x] (ror:%x[%x])(5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
				//rd (1st op reg) 		 bit[19]---bit[16] 
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//opcode rd,rm
				dummyreg=movasm(dummyreg3);
				
				//if PC align for prefetch
				if(((arminstr)&0xf)==0xf)
					dummyreg+=0x8;
				
				//rd (1st op reg) 		 bit[19]---bit[16] 
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("MOV rd(%x)[%x],rm(%d)[%x] (5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
				//check for S bit here and update (virt<-asm) processor flags
				if(setcond_arm==1)
					updatecpuflags(0,cpsrasm,0x0);
					//printf("\n CPSR:%x",cpsrvirt);
			}
		return 0;
		}
		break;
	
		//bic rd,rs
		case(0xe):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg,gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg3=bicasm(dummyreg,dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("BIC rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				dummyreg2=bicasm(dummyreg,dummyreg3);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("BIC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>16)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//mvn rd,rs
		case(0xf):{
			
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] / unused because of #Imm 
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				dummyreg=mvnasm(dummyreg2);
				#ifdef DEBUGEMU
				printf("MVN rn(%d)[%x],#Imm[%x] (ror:%x[%x])(5.4) \n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg,
				(unsigned int)(arminstr&0xff),
				(int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			
				//rd (1st op reg) 		 bit[19]---bit[16] 
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
			}
			else{	//for Register (operand 2) operator / shift included	
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(dummyreg3,(dummyreg4&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(dummyreg3,(dummyreg4&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(dummyreg3,(dummyreg4&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg3,(int)((dummyreg2>>4)&0xf),(unsigned int)dummyreg4);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(dummyreg3,(dummyreg4&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lslasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=lsrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=asrasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						dummyreg4=dummyreg3;
						dummyreg3=rorasm(dummyreg3,(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//opcode rd,rm
				dummyreg=mvnasm(dummyreg3);
		
				//rd (1st op reg) 		 bit[19]---bit[16] 
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("MVN rd(%d)[%x],rm(%d)[%x] (5.4)\n",
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg,
				(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	} //end switch conditional 5.4 flags

}//end if 5.4

//5.5 MRS / MSR (use TEQ,TST,CMN and CMP without S bit set)
//xxxxxx - xxxx / bit[21]--bit[12]
//printf("PSR:%x",((arminstr>>16)&0x3f));

switch((arminstr>>16)&0x3f){

	//only 11 bit (N,Z,C,V,I,F & M[4:0]) are defined for ARM processor
	//bits PSR(27:8:5) are reserved and must not be modified.
	//1) so reserved bits must be preserved when changing values in PSR
	//2) programs will never check the reserved status bits while PSR check.
	//so read, modify , write should be employed
	
	case(0xf):{ 		//MRS (transfer PSR to register)
		//printf("MRS (transf PSR to reg!) \n");
		
		//source PSR is: CPSR & save cond flags
		if( ((dummyreg2=((arminstr>>22)&0x3ff)) &0x1) == 0){
			#ifdef DEBUGEMU
			printf("CPSR save!:%x",(unsigned int)cpsrvirt);
			#endif
			dummyreg=cpsrvirt;
			faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
		}
		//source PSR is: SPSR<mode> & save cond flags
		else{
			//printf("SPSR save!:%x",spsr_last);
			dummyreg=spsr_last;
			faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
		}
	return 0;
	}
	break;
	
	case(0x29):{ 	//MSR (transfer reg content to PSR)
		//printf("MSR (transf reg to PSR!) \n");
		//CPSR
		if( ((dummyreg2=((arminstr>>22)&0x3ff)) &0x1) == 0){
			fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
			//dummy PSR
			dummyreg&=0xf90f03ff; //important PSR bits
			#ifdef DEBUGEMU
			printf("CPSR restore!:%x",(unsigned int)dummyreg);
			#endif
			//cpsrvirt=dummyreg;
			
			//modified (cpu state id updated)
			updatecpuflags(1,cpsrvirt,dummyreg&0x1f);
		}
		//SPSR
		else{
			fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
			//dummy PSR
			dummyreg&=0xf90f03ff; //important PSR bits
			#ifdef DEBUGEMU
			printf("SPSR restore!:%x",(unsigned int)dummyreg);
			#endif
			spsr_last=dummyreg;
			
		}
	return 0;
	}
	break;
	
	case(0x28):{ 	//MSR (transfer reg content or #imm to PSR flag bits)
		//printf("MRS (transf reg or #imm to PSR flag bits!) \n");
		
		//CPSR
		if( ((dummyreg2=((arminstr>>22)&0x3ff)) &0x1) == 0){
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
				//dummy PSR
				dummyreg&=0xf90f03ff; //important PSR bits
				#ifdef DEBUGEMU
				printf("CPSR restore from rd(%d)!:%x \n",(int)(arminstr&0xf),(unsigned int)dummyreg);
				#endif
				cpsrvirt=dummyreg;
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				//printf("CPSR restore from #imm!:%x \n",dummyreg2);
				cpsrvirt=dummyreg2;
			}
		
		}
		//SPSR
		else{
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
				//dummy PSR
				dummyreg&=0xf90f03ff; //important PSR bits
				#ifdef DEBUGEMU
				printf("SPSR restore from rd(%d)!:%x \n ",(int)(arminstr&0xf),(unsigned int)dummyreg);
				#endif
				spsr_last=dummyreg;
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				#ifdef DEBUGEMU
				printf("SPSR restore from #imm!:%x \n",(unsigned int)dummyreg2);
				#endif
				spsr_last=dummyreg2;
			}
		}
	return 0;
	}
	break;

}

//5.6 multiply and accumulate

//Condition flags
//If S is specified, these instructions:
//update the N and Z flags according to the result
//do not affect the V flag
//corrupt the C flag in ARMv4 and earlier
//do not affect the C flag in ARMv5 and later.

//MUL     r10,r2,r5
//MLA     r10,r2,r1,r5
//MULS    r0,r2,r2
//MULLT   r2,r3,r2
//MLAVCS  r8,r6,r3,r8

//take bit[27] ... bit[22] & 0 and bit[4] ... bit[0] = 9 for MUL opc 
switch( ((arminstr>>22)&0x3f) + ((arminstr>>4)&0xf) ){
	case(0x9):{
		//printf("MUL/MLA opcode! (5.6)\n");
		switch((arminstr>>20)&0x3){
			//btw: rn is ignored as whole
			//multiply only & dont alter CPSR cpu flags
			case(0x0):
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				printf("mul rd(%d),rm(%d)[%x],rs(%d)[%x]",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)dummyreg,
				(int)((arminstr>>8)&0xf),(unsigned int)dummyreg2
				);
				#endif
				
				dummyreg2=mulasm(dummyreg,dummyreg2);
				
				//rd
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
			//multiply only & set CPSR cpu flags
			case(0x1):
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				printf("mul rd(%d),rm(%d)[%x],rs(%d)[%x] (PSR s)",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)dummyreg,
				(int)((arminstr>>8)&0xf),(unsigned int)dummyreg2
				);
				#endif
				dummyreg2=mulasm(dummyreg,dummyreg2);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
				
				//rd
				faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
			//mult and accum & dont alter CPSR cpu flags
			case(0x2):
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				//rn
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				printf("mla rd(%d),rm(%d)[%x],rs(%d)[%x],rn(%d)[%x] ",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)dummyreg,
				(int)((arminstr>>8)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3
				);
				#endif
				
				dummyreg=mlaasm(dummyreg,dummyreg2,dummyreg3);
				
				//rd
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
			//mult and accum & set CPSR cpu flags
			case(0x3):
				//rm
				fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				//rn
				fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				printf("mla rd(%d),rm(%d)[%x],rs(%d)[%x],rn(%d)[%x] (PSR s)",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)dummyreg,
				(int)((arminstr>>8)&0xf),(unsigned int)dummyreg2,
				(int)((arminstr>>12)&0xf),(unsigned int)dummyreg3
				);
				#endif
				
				dummyreg=mlaasm(dummyreg,dummyreg2,dummyreg3);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
				
				//rd
				faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
		}
	return 0;
	}
	break;
}

//5.7 LDR / STR
//take bit[26] & 0x40 (if set) and save contents if found 
switch( ((dummyreg=((arminstr>>20)&0xff)) &0x40) ){
	//it is indeed a LDR/STR opcode
	case(0x40):{
		//rd
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0); 
		
		//rn
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0); 
		
		//IF NOT INMEDIATE (i=1)
		//decode = dummyreg / rd = dummyreg2 / rn = dummyreg3 / #imm|rm index /
		//post shifted OR #imm shifted = dummyreg4
		
		//dummyreg4 = calculated address/value (inside #imm/reg)
		//If shifted register? (offset is register Rm)
		if((dummyreg&0x20)==0x20){
			
			//R15/PC must not be chosen at Rm!
			if (((arminstr)&0xf)==0xf) return 0;
			
			//rm (operand 2 )		 bit[11]---bit[0]
			fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
			//printf("rm%d(%x) \n",((arminstr)&0xf),dummyreg4);
			
			//(currently at: shift field) rs shift opcode to Rm
			if( ((dummyreg5=((arminstr>>4)&0xff)) &0x1) == 1){
			
				//lsl
				if((dummyreg5&0x6)==0x0){
					//rs loaded
					fastldr((u8*)&gbachunk, gbavirtreg_cpu, ((dummyreg5>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("LSL rm(%d),rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbachunk);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg4=lslasm(dummyreg4,(gbachunk&0xff));
				}
				//lsr
				else if ((dummyreg5&0x6)==0x2){
					//rs loaded
					fastldr((u8*)&gbachunk, gbavirtreg_cpu, ((dummyreg5>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("LSR rm(%d),rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbachunk);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg4=lsrasm(dummyreg4,(gbachunk&0xff));
				}
				//asr
				else if ((dummyreg5&0x6)==0x4){
					//rs loaded
					fastldr((u8*)&gbachunk, gbavirtreg_cpu, ((dummyreg5>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("ASR rm(%d),rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbachunk);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg4=asrasm(dummyreg4,(gbachunk&0xff));
				}
				//ror
				else if ((dummyreg5&0x6)==0x6){
					//rs loaded
					fastldr((u8*)&gbachunk, gbavirtreg_cpu, ((dummyreg5>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("ROR rm(%d),rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbachunk);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg4=rorasm(dummyreg4,(gbachunk&0xff));
				}
			//compatibility: refresh CPU flags when barrel shifter is used
			updatecpuflags(0,cpsrasm,0x0);
			}
			//#Imm ammount shift & opcode to Rm
			else{
				//show arminstr>>4
				//printf("dummyreg2:%x",dummyreg2);
			
				//lsl
				if((dummyreg5&0x6)==0x0){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=lslasm(dummyreg4,((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("LSL rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//lsr
				else if ((dummyreg5&0x6)==0x2){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=lsrasm(dummyreg4,((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("LSR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//asr
				else if ((dummyreg5&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=asrasm(dummyreg4,((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("ASR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//ror
				else if ((dummyreg5&0x6)==0x6){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=rorasm(dummyreg4,((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("ROR rm(%d)[%x],#imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)dummyreg4,(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
			//compatibility: refresh CPU flags when barrel shifter is used
			updatecpuflags(0,cpsrasm,0x0);
			}
		}
		
		//ELSE IF INMEDIATE (i=0)(absolute #Imm value)
		else{
			//#Imm value (operand 2)		 bit[11]---bit[0]
			dummyreg4=(arminstr&0xfff);
			#ifdef DEBUGEMU
			printf(" #Imm(0x%x) \n",(unsigned int)dummyreg4);			
			#endif
			
		}
		
		//pre indexing bit 	(add offset before transfer)
		if((dummyreg&0x10)==0x10){
			
			//up / increase  (Rn+=Rm)
			if((dummyreg&0x8)==0x8){
				dummyreg3+=dummyreg4;
			}
		
			//down / decrease bit (Rn-=Rm)
			else{
				dummyreg3-=dummyreg4;
			}
			
			//pre indexed says base is updated [!] (writeback = 1)
			dummyreg|= (1<<1);
			
			#ifdef DEBUGEMU
			printf("pre-indexed bit! \n");
			#endif
		}
		
		//keep original rn and copy
		//dummyreg5=(dummyreg3);
		
		//1)LDR/STR dummyreg as rd 
		
		//decode = dummyreg / rd = dummyreg2 / rn = dummyreg3 / 
		//dummyreg4 = either #Imm OR register offset
		
		//BEGIN MODIFIED 26 EN
		
		//2a) If register opcode?
		if((dummyreg&0x20)==0x20){
		
			//bit[20]
			//STR
			if((dummyreg&0x1)==0x0){
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//dereference Rn+offset
					//store RD into [Rn,#Imm]
					cpuwrite_byte(dummyreg3,(dummyreg2&0xff));
					#ifdef DEBUGEMU
						printf("ARM:5.7 trying STRB rd(%d), [b:rn(%d)[%x],xxx] (5.9) \n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)dummyreg3);
					#endif
				}
				//transfer word quantity
				else{
					//store RD into [RB,#Imm]
					cpuwrite_word(dummyreg3,dummyreg2);
					#ifdef DEBUGEMU
						printf("ARM:5.7 trying to STR rd(%d), [b:rn(%d)[%x],xxx] (5.9)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)dummyreg3);
					#endif
				}
			}
			
			//LDR is taken care on cpuxxx_xxx opcodes
			//LDR
			else{
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//dereference Rn+offset
					dummyreg2=cpuread_byte(dummyreg3);
					
					#ifdef DEBUGEMU
						printf("\n GBA LDRB rd(%d)[%x], [#0x%x] (5.9)\n",
						(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,(unsigned int)(dummyreg3));
					#endif
					faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				}
			
				//transfer word quantity
				else{
					//dereference Rn+offset
					dummyreg2=cpuread_word(dummyreg3);
					
					#ifdef DEBUGEMU
						printf("\n GBA LDR rd(%d)[%x], [#0x%x] (5.9)\n",
						(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,(unsigned int)(dummyreg3));
					#endif
					faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				}
			}
		}//end if register/shift opcode
		
		// END MODIFIED 26 EN
		
		//modified 27 en
		//2b) it's #Imm, (label) Rn or other inmediate value for STR/LDR
		else{
			//bit[20]
			//STR
			if((dummyreg&0x1)==0x0){
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					#ifdef DEBUGEMU
					printf("STRB rd(%d)[%x],[rn(%d)](%x)",
					(int)((arminstr>>12)&0xf),(unsigned int)(dummyreg2&0xff),(int)((arminstr>>16)&0xf),(unsigned int)dummyreg3);
					#endif
					//str rd,[rn]
					cpuwrite_byte(dummyreg3, dummyreg2&0xff);
				}
				//word quantity
				else{
					#ifdef DEBUGEMU
					printf("STR rd(%d)[%x],[rn(%d)](%x)",
					(int)((arminstr>>12)&0xf),(unsigned int)dummyreg2,(int)((arminstr>>16)&0xf),(unsigned int)dummyreg3);
					#endif
					//str rd,[rn]
					cpuwrite_word(dummyreg3, dummyreg2); //broken
				}
			}
			
			//LDR : 
			else{
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//printf("\n LDRB #imm");
					//dummyreg2=((arminstr&0xfff)); //dummyreg4 already has this
					
					/* //old: because we use r15 and fetch directly from faststr/fastldr
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						dummyreg2=(rom)+dummyreg4+(0x8); //align +8 for prefetching
						gbachunk=cpuread_byte(dummyreg2);
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						dummyreg2=dummyreg3; //rd is gbachunk now, old rd is rewritten
						gbachunk=cpuread_byte(dummyreg2);
					}
					*/
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						dummyreg2=(dummyreg3+(0x8)); //align +8 for prefetching
						gbachunk=cpuread_byte(dummyreg2);
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						dummyreg2=dummyreg3; //rd is gbachunk now, old rd is rewritten
						gbachunk=cpuread_byte(dummyreg2);
					}
					
					#ifdef DEBUGEMU
						printf("LDRB rd(%d)[%x]<-LOADED [Rn(%d),#IMM]:(%x)",(int)((arminstr>>12)&0xf),(unsigned int)gbachunk,(unsigned int)((arminstr>>16)&0xf),(unsigned int)dummyreg2);
					#endif
				}
				//transfer word quantity
				else{
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						dummyreg2=(dummyreg3+(0x8)); //align +8 for prefetching
						gbachunk=cpuread_word(dummyreg2);
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						dummyreg2=dummyreg3; //rd is gbachunk now, old rd is rewritten
						gbachunk=cpuread_word(dummyreg2);
					}
					
					#ifdef DEBUGEMU
						printf("LDR rd(%d)[%x]<-LOADED [Rn(%d),#IMM]:(%x)",(int)((arminstr>>12)&0xf),(unsigned int)gbachunk,(unsigned int)((arminstr>>16)&0xf),(unsigned int)dummyreg2);
					#endif
				}
				faststr((u8*)&gbachunk, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
			}
		}
		
		//modified 27 en end
		
		//3)post indexing bit (add offset after transfer)
		if((dummyreg&0x10)==0x0){
			#ifdef DEBUGEMU
			printf("post-indexed bit!");
			#endif
			dummyreg&= ~(1<<1); //forces the writeback post indexed base to be zero (base address isn't updated, basically)
		}
		
		//4)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		// (ALWAYS (if specified) except when R15)
		if( ((dummyreg&0x2)==0x2) && (((arminstr>>16)&0xf)!=0xf) ){
			
			//up / increase  (Rn+=Rm)
			if((dummyreg&0x8)==0x8){
				dummyreg3+=dummyreg4;
			}
		
			//down / decrease bit (Rn-=Rm)
			else{
				dummyreg3-=dummyreg4;
			}
			
			#ifdef DEBUGEMU
			printf("(new) rn writeback base addr! [%x]",(unsigned int)dummyreg3);
			#endif
			
			//old: faststr((u8*)&dummyreg5, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
		}
		//else: don't write-back address into base
		#ifdef DEBUGEMU
		printf("/******************************/");
		#endif
	}
	break;
} //end 5.7

	
//5.8 STM/LDM
switch( ( (dummyreg=((arminstr>>20)&0xff)) & 0x80)  ){
	case(0x80):{
		u32 savedcpsr=0;
		u8 writeback=0;
		
		//rn
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0); 
		
		//1a)force 0x10 usr mode 
		if( ((dummyreg&0x4)==0x4) && ((cpsrvirt&0x1f)!=0x10)){
			savedcpsr=cpsrvirt;
			updatecpuflags(1,cpsrvirt,0x10);
			#ifdef DEBUGEMU
			printf("FORCED TO USERMODE!CPSR: %x",(unsigned int)cpsrvirt);
			#endif
			writeback=1;
		}
		//else do not load PSR or force usr(0x10) mode
		
		int cntr=0; 	//CPU register enum
		int offset=0;
		
		//4)STM/LDM
		//bit[20]
		//STM
		if((dummyreg&0x1)==0x0){
			#ifdef DEBUGEMU
			printf("STMIA r(%d)[%x], {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
			(int)((arminstr>>16)&0xf),
			(unsigned int)dummyreg3,
			
			(int)((arminstr&0xffff)&0x8000),
			(int)((arminstr&0xffff)&0x4000),
			(int)((arminstr&0xffff)&0x2000),
			(int)((arminstr&0xffff)&0x1000),
			(int)((arminstr&0xffff)&0x800),
			(int)((arminstr&0xffff)&0x400),
			(int)((arminstr&0xffff)&0x200),
			(int)((arminstr&0xffff)&0x100),
			
			(int)((arminstr&0xffff)&0x80),
			(int)((arminstr&0xffff)&0x40),
			(int)((arminstr&0xffff)&0x20),
			(int)((arminstr&0xffff)&0x10),
			(int)((arminstr&0xffff)&0x08),
			(int)((arminstr&0xffff)&0x04),
			(int)((arminstr&0xffff)&0x02),
			(int)((arminstr&0xffff)&0x01));
			
			printf(":pushd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation STMIA
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((dummyreg&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((dummyreg&0x8)==0x8){
					#ifdef DEBUGEMU
						printf("asc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu 
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							//ascending stack
							cpuwrite_word(dummyreg2+(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						printf("desc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							//descending stack
							cpuwrite_word(dummyreg2-(offset*4), gbavirtreg_cpu[(1<<cntr)]); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			}
		}
		
		//LDM
		else{
			#ifdef DEBUGEMU
			printf("LDMIA rd(%d)[%x], {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
			(int)((arminstr>>16)&0xf),
			(unsigned int)dummyreg3,
			
			(int)((arminstr&0xffff)&0x8000),
			(int)((arminstr&0xffff)&0x4000),
			(int)((arminstr&0xffff)&0x2000),
			(int)((arminstr&0xffff)&0x1000),
			(int)((arminstr&0xffff)&0x800),
			(int)((arminstr&0xffff)&0x400),
			(int)((arminstr&0xffff)&0x200),
			(int)((arminstr&0xffff)&0x100),
			
			(int)((arminstr&0xffff)&0x80),
			(int)((arminstr&0xffff)&0x40),
			(int)((arminstr&0xffff)&0x20),
			(int)((arminstr&0xffff)&0x10),
			(int)((arminstr&0xffff)&0x08),
			(int)((arminstr&0xffff)&0x04),
			(int)((arminstr&0xffff)&0x02),
			(int)((arminstr&0xffff)&0x01));
			
			printf(":popd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation LDMIA
			int cntr=0;
			int offset=0;
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((dummyreg&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((dummyreg&0x8)==0x8){
					#ifdef DEBUGEMU
						printf("asc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							gbavirtreg_cpu[(1<<cntr)]=cpuread_word(dummyreg2+(offset*4)); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						printf("desc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							gbavirtreg_cpu[(1<<cntr)]=cpuread_word(dummyreg2-(offset*4)); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			}
		}
		
		//4)post indexing bit (add offset after transfer) (by default on stmia/ldmia opcodes)
		if((dummyreg&0x10)==0x0){
			dummyreg|=0x2; //forces the writeback post indexed base
			#ifdef DEBUGEMU
			printf("post indexed (default)! \n");
			#endif
		}
		
		//5)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		if((dummyreg&0x2)==0x2){
			
			//if asc/ up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
			if((dummyreg&0x8)==0x8){
				dummyreg3=(u32)addsasm((u32)dummyreg2,(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			
			//else descending stack 1 bit
			else{
				dummyreg3=(u32)subsasm((u32)dummyreg2,(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			
			faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
			#ifdef DEBUGEMU
			printf(" updated addr: %x / Bytes workd onto stack: %x \n", (unsigned int)dummyreg3,((lutu32bitcnt(arminstr&0xffff))*4));
			#endif
		}
		//else: don't write-back address into base
		
		//1b)forced 0x10 usr mode go back to SPSR mode
		if(writeback==1){
			#ifdef DEBUGEMU
			printf("RESTORED MODE:CPSR %x",(unsigned int)savedcpsr);
			#endif
			updatecpuflags(1,cpsrvirt,savedcpsr&0x1f);
			writeback=0;
		}
		
	break;
	}
}

//5.9 SWP Single Data Swap (swp rd,rm,[rn])
switch( ( (dummyreg=(arminstr)) & 0x1000090)  ){
	case(0x1000090):{
		//printf("SWP opcode!");
		//rn (address)
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((dummyreg>>16)&0xf), 32,0); 
		//rd is writeonly
		//rm
		fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg)&0xf), 32,0); 

		//patch addresses read for GBA intermediate data transfers
		//deprecated:dummyreg2=addresslookup(dummyreg2, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dummyreg2 & 0x3FFFF);
		
		//swap byte
		if(dummyreg & (1<<22)){
			//printf("byte quantity!\n");
			//[rn]->rd
			//deprecated:dummyreg3=ldru8extasm(dummyreg2,0x0);
			dummyreg3=cpuread_byte(dummyreg2);
			faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((dummyreg>>12)&0xf), 32,0);
			#ifdef DEBUGEMU
			printf("SWPB 1/2 [rn(%d):%x]->rd(%d)[%x] \n",(int)((dummyreg>>16)&0xf),(unsigned int)dummyreg2,(int)((dummyreg>>12)&0xf),(unsigned int)dummyreg3);
			#endif
			//rm->[rn]
			//deprecated:stru8asm(dummyreg2,0x0,dummyreg4);
			cpuwrite_byte(dummyreg2,dummyreg4&0xff);
			#ifdef DEBUGEMU
			printf("SWPB 2/2 rm(%d):[%x]->[rn(%d):[%x]] \n",(int)((dummyreg)&0xf),(unsigned int)(dummyreg4&0xff),(int)((dummyreg>>16)&0xf),(unsigned int)dummyreg2);
			#endif
		}
		else{
			//printf("word quantity!\n");
			//[rn]->rd
			//deprecated:dummyreg3=ldru32extasm(dummyreg2,0x0);
			dummyreg3=cpuread_word(dummyreg2);
			faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((dummyreg>>12)&0xf), 32,0);
			#ifdef DEBUGEMU
			printf("SWP 1/2 rm(%d):[%x]->[rn(%d):[%x]] \n",(int)((dummyreg)&0xf),(unsigned int)(dummyreg4&0xff),(int)((dummyreg>>16)&0xf),(unsigned int)dummyreg2);
			#endif
			//rm->[rn]
			//deprecated:stru32asm(dummyreg2,0x0,dummyreg4);
			cpuwrite_word(dummyreg2,dummyreg4);
			#ifdef DEBUGEMU
			printf("SWP 2/2 rm(%d):[%x]->[rn(%d):[%x]] \n",(int)((dummyreg)&0xf),(unsigned int)(dummyreg4&0xff),(int)((dummyreg>>16)&0xf),(unsigned int)dummyreg2);
			#endif
		}
		
	}
}

//5.10 software interrupt
switch( (arminstr) & 0xf000000 ){
	case(0xf000000):{
		/*
		//required because SPSR saved is not SVC neccesarily
		//u32 spsr_old=spsr_last;
		
		//Enter SVC<mode>
		//updatecpuflags(1,cpsrvirt,0x13);
		
		//printf("CPSR(entrymode):%x \n",cpsrvirt&0x1f);
		//#ifdef DEBUGEMU
		//printf("[ARM] swi call #0x%x! (5.10)",arminstr&0xffffff);
		//#endif
		//swi_virt(arminstr&0xffffff);
		
		//Restore CPU<mode>
		//updatecpuflags(1,cpsrvirt,spsr_last&0x1F);
		
		//printf("CPSR(restoremode):%x \n",cpsrvirt&0x1f);
		
		//restore correct SPSR
		//spsr_last=spsr_old;
		*/
		
		#ifdef DEBUGEMU
		printf("[ARM] swi call #0x%x! (5.10)",(unsigned int)(arminstr&0xffffff));
		#endif
		
		armstate = 0;
		gba.armirqenable=false;
		
		/* //deprecated (because we need the SPSR to remember SVC state)
		//required because SPSR saved is not SVC neccesarily
		u32 spsr_old=spsr_last; //(see below SWI bios case)
		*/
		
		updatecpuflags(1,cpsrvirt,0x13);
		
		//printf("CPSR(entrymode):%x \n",cpsrvirt&0x1f);
		
		//printf("SWI #0x%x / CPSR: %x(5.17)\n",(thumbinstr&0xff),cpsrvirt);
		swi_virt(arminstr&0xffffff);
		
		gbavirtreg_cpu[0xe] = rom - (armstate ? 4 : 2);
		
		#ifdef BIOSHANDLER
			rom  = (u32)(0x08-0x4);
		#else
			//rom  = gbavirtreg_cpu[0xf] = (u32)(gbavirtreg_cpu[0xe]-0x4);
			//continue til BX LR (ret address cback)
		#endif
		
		//printf("swi%x",(unsigned int)(arminstr&0xff));
		
		//we let SWI bios decide when to go back from SWI mode
		//Restore CPU<mode>
		updatecpuflags(1,cpsrvirt,spsr_last&0x1F);
		
		/* //deprecated	(because we need the SPSR to remember SVC state)
		//restore correct SPSR
		spsr_last=spsr_old;
		*/
	}
}

//5.11 (Co- Processor calls) gba has not CP

//5.12 CDP not implemented on gba

//5.13 STC / LDC (copy / restore memory to cause undefined exception) not emulated

//5.14 MRC / MCR not implemented / used on GBA ( Co - processor)

//5.15 UNDEFINED INSTRUCTION (conditionally executed). If true it will be executed
switch( (dummyreg=(arminstr)) & 0x06000010 ){
	case(0x06000010):
	//printf("undefined instruction!");
	exceptundef(arminstr);
	break;
}

return arminstr;
}