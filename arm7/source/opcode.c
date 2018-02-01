#include "opcode.h"

struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

//fifo
u32 buffer_input[16],buffer_output[16];

u32 sendwordipc(uint8 word){
	//checkreg writereg (add,val) static int REG_IPC_add=0x04000180,REG_IE_add=0x04000210,REG_IF_add=0x04000214;
	//return stru32inlasm(0x04000180,0x0,	 ((ldru32inlasm(0x04000180)&0xfffff0ff) | (word<<8)) ); //str[addr+index,value]
	*(u32*)(0x04000180)=((*((u32*)0x04000180)&0xfffff0ff) | (word<<8));
	return (*(u32*)(0x04000180));
}

u32 recvwordipc(){
	return (*(u32*)(0x04000180)&0xf);
}

void ipcidle(){
	sendwordipc(0x0);
}

//counts leading zeroes :)
inline __attribute__((always_inline))
u8 clzero(u32 var){
   
    u8 cnt=0;
    u32 var3;
    if (var>0xffffffff) return 0;
   
    var3=var; //copy
    var=0xFFFFFFFF-var;
    while((var>>cnt)&1){
        cnt++;
    }
    if ( (((var3&0xf0000000)>>28) >0x7) && (((var3&0xff000000)>>24)<0xf)){
        var=((var3&0xf0000000)>>28);
        var-=8; //bit 31 can't count to zero up to this point
            while(var&1) {
                cnt++; var=var>>1;
            }
    }
return cnt;
}
