#include <nds/ndstypes.h>

//fifo & other opcode parts


#ifndef FIFO_INSTANCE
#define FIFO_INSTANCE
//FIFO struct
struct fifo_semaphore{
u32 REG_FIFO_SENDEMPTY_STAT; 	//0<<0
u32 REG_FIFO_SENDFULL_STAT; 	//0<<1
u32 REG_FIFO_SENDEMPTY_IRQ; 	//0<<2
u32 REG_FIFO_SENDEMPTY_CLR; 	//0<<3
u32 REG_FIFO_RECVEMPTY; 		//0<<8
u32 REG_FIFO_RECVFULL; 			//0<<9
u32 REG_FIFO_RECVNOTEMPTY_IRQ; 	//0<<10
u32 REG_FIFO_ERRSENDRECVEMPTYFULL; //0<<14
u32 REG_FIFO_ENABLESENDRECV; 	//0<<15
};

struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;
#endif

extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

#ifdef __cplusplus
extern "C"{
#endif

//fifo
extern u32 buffer_input[0x10];
extern u32 buffer_output[0x10];
void recvfifo(u32 * buffer);
void sendfifo(u32 * buffer);

//IPC
void ipcidle();
u32 sendwordipc(uint8 word);
u32 recvwordipc();

//ldr/str inline
u32 ldru32inlasm(u32 x1);
u32 stru32inlasm(u32 x1,u32 y1);

u8	clzero(u32);

//asm opcodes
int sqrtasm(int);
u32 wramtstasm(u32 address,u32 top);
u32 branchtoaddr(u32 value,u32 address);

#ifdef __cplusplus
}
#endif