#include "opcode.h"
/*
//ldr/str (inline)
inline __attribute__((always_inline))
u32 ldru32inlasm(u32 x1){
u32 y1;
__asm__ volatile(
				"ldr %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u16 ldru16inlasm(u32 x1){
u16 y1;
__asm__ volatile(
				"ldrh %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u8 ldru8inlasm(u32 x1){
u8 y1;
__asm__ volatile(
				"ldrb %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

inline __attribute__((always_inline))
u8 stru8inlasm(u32 x1,u32 x2,u8 y1){
u8 out;
__asm__ volatile(
				"strb %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u16 stru16inlasm(u32 x1,u32 x2,u16 y1){
u16 out;
__asm__ volatile(
				"strh %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u32 stru32inlasm(u32 x1,u32 x2,u32 y1){
u32 out;
__asm__ volatile(
				"str %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

*/
inline __attribute__((always_inline))
u32 nopinlasm(){
__asm__ volatile(
				"nop""\n\t"
				: 	// = write only (value+=value) / + read/write with output / & output only reg
				:	//1st arg takes rw, 2nd and later ro
				:
				); 
return 0;
}

//works FINE
u32 u32store(u32 address1, u32 address2, u32 value){
	*(u32*)((u32)(address1+address2))=value;
	return 0;
}

u16 u16store(u32 address1, u32 address2, u16 value){
	*(u16*)((u32)(address1+address2))=value;
	//printf("value stored @(%x):[%x]",
	//(u32)(address1+address2),
	//(unsigned int)*(u16*)((u32)(address1+address2)));
	return 0;
}

u8 u8store(u32 address1, u32 address2, u8 value){
	*(u8*)((u32)(address1+address2))=value;
	return 0;
}

u32 u32read(u32 address1, u32 address2){
	return *(u32*)((u32)(address1+address2));
}

u16 u16read(u32 address1, u32 address2){
	return *(u16*)((u32)(address1+address2));
}

u8 u8read(u32 address1, u32 address2){
	return *(u8*)((u32)(address1+address2));
}



//fifo
u32 buffer_input[16],buffer_output[16];
struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

u32 sendwordipc(uint8 word){
	//checkreg writereg (add,val) static int REG_IPC_add=0x04000180,REG_IE_add=0x04000210,REG_IF_add=0x04000214;
	*((u32*)0x04000180)=((*(u32*)0x04000180)&0xfffff0ff) | (word<<8);
	return (*(u32*)0x04000180);
}

u32 recvwordipc(){
	return ((*(u32*)0x04000180)&0xf);
}

void ipcidle(){
	sendwordipc(0x0);
}

//fiforecv
void recvfifo(u32 * buffer, struct fifo_semaphore fifo_instance){

fifo_instance.REG_FIFO_RECVEMPTY=1<<8;
int i=0;
	while (! ((*(u32*)0x04000184) & fifo_instance.REG_FIFO_RECVEMPTY)){ //is not empty recv buffer?
			buffer[i]=*((u32*)0x04100000);
			i++;
		}
}

//fifosend
void sendfifo(u32 * buffer, struct fifo_semaphore fifo_instance){
fifo_instance.REG_FIFO_SENDEMPTY_STAT=1<<0; //0<<0 - is send queue buffer empty? 0 not empty, 1 empty
fifo_instance.REG_FIFO_SENDFULL_STAT=1<<1; //is full send? 0 not full, 1 full
fifo_instance.REG_FIFO_SENDEMPTY_CLR=1<<3; //0 nothing - 1 clears send fifo
int i=0;
	
	while(! ((*(u32*)0x04000184)  & fifo_instance.REG_FIFO_SENDFULL_STAT)){
			*((u32*)0x04000188)=buffer[i]; //str[addr+index,value]
			i++;
	}
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
