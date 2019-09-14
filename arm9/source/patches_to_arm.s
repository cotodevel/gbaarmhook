.align 4
.code 32
.arm
.cpu arm7tdmi
.text

@patches (from 20/02/15 outdated)
@ldr #imm -4095 and +4095 (inclusive). / remember pc is 8 bytes ahead (prefetch + next opcode)
.global dummyopcode
dummyopcode:
	ldr pc,[pc, #(-(0x204+0x8)+(0x000000d0))]		@replace 0x204 with current PC address + offset (word size) of the word location on rom (must be 4095+- from base)
	ldr pc,[pc,#-0x4]
	bx r1
	pop  {r0,r2,r4,r5,r14}
	
.global PATCH_ENTRYPOINT
.type	PATCH_ENTRYPOINT STT_FUNC
PATCH_ENTRYPOINT:
	@bootrom opcode entrypoint
	.word 0xe51ff13c	@ldr pc,[pc,#0x4] opcode 1/2/ should be installed @ rom entrypoint
	.word 0x08ff8000	@value 2/2 (PATCH_BOOTCODE) / should be installed @ rom_start+0xd0
	@IRQ handler entrypoint
	
	.word 0xe51ff004	@IRQ handler + 0 ARM opcode (to replace with ldr pc,=0x08FE0000)
	.word 0x08FE0000	@0x08FE0000 from PC (where you want to load this word)
	
@entrypoint approach patcher
.global PATCH_BOOTCODE
.type	PATCH_BOOTCODE STT_FUNC
PATCH_BOOTCODE:
	push {r0,r1,r2,lr}					@rd is temp
	mov r0,#((0x08000000 + 0x1000000)) 	@(ARM code)
	sub r0,#(0x10000)					@PATCH_START @0x08ff0000
	mov lr,pc
	bx  r0 
	pop {r0,r1,r2,lr}
	
	mov r0,#0xe3000000		@/
	orr r0,#0x00a00000		@|replace with entrypoint value here
	orr r0,#0x00000012		@\
	
	//mov r1,#0xe5000000		@/@0xe51ff138
	//orr r1,#0x001f0000		@|replace with opcode ldr pc,[label] (LR or entrypoint address +0x4 ) here
	//orr r1,#0x0000f100
	//orr r1,#0x00000038
	
	mov r1,#0x08000000		@assemble 0x08000208 into r1
	orr r1,#0x00000208
	
	mov r2,#0xe1000000		@create ARM opcode that bx r1 (exit! from installer)
	orr r2,#0x002f0000		
	orr r2,#0x0000ff00
	orr r2,#0x00000011
	
	stmdb r13!,{r0,r2}		@jump to stack
	mov pc, r13
	
@installs re-entrant IRQ routine variables 
@must be installed after filling stack target (0x03001c00) content
.global PATCH_START
.type	PATCH_START STT_FUNC
PATCH_START:
	mov r1,#(0x03000000) 		@\ 0x03007ffc -> 0x03001c00 (irq handler is fetched from 0x08000240) (ARM mode)
	orr r1,#0x00001b00
	orr r1,#0x000000fc
	
	mov r2,#((0x08000000 + 0x1000000)) 	@(ARM code)
	sub r2,#(0x20000)					@PATCH_HOOK_START @0x08FE0000
	
	str r2,[r1]
	
	bx lr
	
.global PATCH_HOOK_START
.type	PATCH_HOOK_START STT_FUNC
PATCH_HOOK_START:
	push  {r0,r2,r4,r5,r14}
	
		//do any_cback here
		mov r0,#0x08000000		@NDS7_RTC_PROCESS
		orr r0,#0x00fe0000
		orr r0,#0x00008000
		
		mov lr,pc
		bx r0
		
	@(SP from wram stores)
	mov r3,#0x03000000 			@save on IRQ stack: ((0x03007fff-0x200))
	orr r3,#0x00008000
	add r3,r3,#-((0x100)+(4*16))	@use irq stack reserving at least 16 opcodes
	
	mov r0,#0xe3000000		 	@0x03001c00: create ARM opcode that mov r3,0x4000000 //irq code
	orr r0,#0x00a00000		
	orr r0,#0x00003300
	orr r0,#0x00000001
	
	mov r2,#0xe2000000		 	@0x03001c00: create ARM opcode that add r3,r3,0x200 //irq code
	orr r2,#0x00830000		
	orr r2,#0x00003c00
	orr r2,#0x00000002
	
	mov r1,#(0x03000000) 		@\build: 0x03007ffc -> 0x03001c08 (irq stack hook end) (ARM mode)
	orr r1,#0x2000
	sub r1,#0x3f8
	
	mov r4,#0xe8000000			@create ARM opcode  pop {r0,r2,r4,r5,r14} from here
	orr r4,#0x00bd0000		
	orr r4,#0x00004000
	orr r4,#0x00000035
	
	mov r5,#0xe1000000			@create ARM opcode that bx r1 (exit! from installer)
	orr r5,#0x002f0000		
	orr r5,#0x0000ff00
	orr r5,#0x00000011
	
	stmdb r3!,{r0,r2,r4,r5}			@jump to stack redirect /old stack cont doesnt matter
	mov pc, r3

@OUR NDS7/GBA ARM7TDMI vblank IRQ hook process.. write what you need here
.global NDS7_RTC_PROCESS
NDS7_RTC_PROCESS:
	
	@entry: save on IRQ stack: ((0x03007fff-0x200))
	mov r0,#0x03000000 			
	orr r0,#0x00008000
	add r0,r0,#-((0x100)+(4*16)+(4*16))
	stmdb r0,{r1-r14}
	
	mov r0,#0
	mov r1,#0
	mov r2,#0
	mov r3,#0
	mov r4,#0
	mov r5,#0
	mov r6,#0
	mov r7,#0
	mov r8,#0
	mov r9,#0
	mov r10,#0
	mov r11,#0
	mov r12,#0
	mov r13,#0
	mov r14,#0
	
	@leave:save on IRQ stack: ((0x03007fff-0x200))
	mov r0,#0x03000000 			
	orr r0,#0x00008000
	add r0,r0,#-((0x100)+(4*16)+(4*16))
	ldmdb r0,{r1-r14}
	
	bx lr
.align
.pool
.end