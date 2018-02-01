.arm
.cpu arm7tdmi

@vectors
.global nds7_irqflags
nds7_irqflags:
	.word	__irq_flags

.global nds7_flagsaux
nds7_flagsaux:
	.word	__irq_flagsaux

.global nds7_irqvector
nds7_irqvector:
	.word	__irq_vector

.global irqbiosinstall
irqbiosinstall:
	
	//cotos
	//ldr r5,=nds7_irqvector
	//ldr r5,[r5]
	//adr r4,usrirqhandler
	//str r4,[r5]
	
	//bx lr
	
	ldr r6,=stackirq
	str r14,[r6,#(4*14)]
	
	@nogba
	ldr  r4,=0x0380FFFC              @-base address for below I/O registers
	ldr  r0,=usrirqhandler           @\install IRQ handler address
	str  r0,[r4]  					 @IRQ HANDLER @/at 0x0380FFFC
	
	mrs  r0,cpsr   	            	 @\
	bic  r0,r0,#0x80 	              @ cpu interrupt enable (clear i-flag)
	msr  cpsr,r0    		          	@/
	
	@install undef hdlr
	//bl undefhandler
	
	ldr r6,=stackirq
	ldr r14,[r6,#(4*14)]
	
	bx lr
	
.global stackirq
stackirq:
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	
.global usrirqhandler
usrirqhandler:
	
	@broken NDS7 BIOS 
	@1/4 backup
	@ldr r6,=stackirq
	@str r14,[r6,#(4*14)]
	
	@disable IME
	@ldr r6,=0x04000208
	@mov r0,#0
	@str r0,[r6]
	
	@get IF
	@ldr r0,=0x04000214
	@ldr r0,[r0]
	
	@2/4 callback IRQ serve,	r0 holds IF to be aknowledged
	@ldr r6,=bios_irqhandlerstub_C
	@ldr r6,[r6]
	
	@mov  r14, r15		@retaddress:	prefetch is always +0x8 so it acts as skipping next subroutine
	@bx r6
	
	@and orr served IF with RAM Check Bits @ 0x0380FFF8 (NDS7)
	@ldr r6,=0x0380FFF8
	@ldr r6,[r6]
	@orr r0,r0,r6
	
	@save IF
	@ldr r6,=0x04000214
	@str r0,[r6]
	
	@re-enable IME
	@ldr r6,=0x04000208
	@mov r0,#1
	@str r0,[r6]
	
	@4/4 go back
	@ldr r6,=stackirq
	@ldr r14,[r6,#(4*0)]
	@bx r14			@go back to IRQ BIOS (nds7) handler

	@new NDS7 bios handler
	@ldr r7,=stackirq
	@str r14,[r7,#(4*14)]
	
	@ldr r6,=0x04000208
	@mov r0,#0
	@str r0,[r6]
	
	@if nds7 read
	@ldr r5,=0x04000214
	@ldr r0,[r5]
	
	@ldr r3,=nds7_irqflags
	@ldr r2,[r3]
	
	@callback
	@ldr r7,=bios_irqhandlerstub_C
	@ldr r7,[r7]
		@mov  r14, r15		@retaddress:	prefetch is always +0x8 so it acts as skipping next subroutine
		@bx r7
	
	@bios & IF
	@and r2,r2,r0
	
	@str r2,[r3]
	@str r2,[r5]
	
	@mov r0,#1
	@str r1,[r6]
	
	@ldr r7,=stackirq
	@ldr r14,[r7,#(4*14)]
	
	@bx r14
	@SUBS PC,R14,#0x4
	
	@nocash irq handler
	
	@irq_handler:  @interrupt handler (note: r0-r3 are pushed by BIOS)
	//mov    r1,#0x4000000             @\get I/O base address,
	//ldr    r0,[r1,#0x200] @IE/IF     @ read IE and IF,
	//and    r0,r0,r0,lsr #16         @ isolate occurred AND enabled irqs,
	//add    r3,r1,#0x200   @IF        @ and acknowledge these in IF
	//strh   r0,[r3,#2]               @/
	//ldrh   r3,[r1,#-8]              @\mix up with BIOS irq flags at 3007FF8h,
	//orr    r3,r3,r0                @ aka mirrored at 3FFFFF8h, this is required
	//strh   r3,[r1,#-8]              @/when using the (VBlank-)IntrWait functions
	//and    r3,r0,#0x80 @IE/IF.7 SIO  @\
	//cmp    r3,#0x80                  @ check if it's a burst boot interrupt
	//ldreq  r2,[r1,#0x120] @SIODATA32 @ (if interrupt caused by serial transfer,
	@ldreq  r3,[msg_brst]           @ and if received data is "BRST",
	@cmpeq  r2,r3                   @ then jump to burst boot)
	@beq    burst_boot              @/
	
	@... insert your own interrupt handler code here ...
	//bx     lr                      @-return to the BIOS interrupt handler
	
@SWI on arm : When invoking SWIs from inside of ARM state specify SWI NN*10000h, instead of SWI NN as in THUMB state.

.global undefhandler
undefhandler:
	
	//ldr r0,=0x0380FFDC
	//ldr r1,=0xc070c070
	//str r1,[r0]
	bx lr