.arm
.cpu arm7tdmi
.text

@vars
.global cpsrasm
cpsrasm:
	.word 0x00000000

.global divswiarr
divswiarr:
	.word 0x00000000	@DIV
	.word 0x00000000	@MOD
	.word 0x00000000	@ABS

.global bios_irqhandlerstub_C
bios_irqhandlerstub_C:
	.word 0x00000000


@swi opcodes
@---------------------------------------------------------------------------------
.global nds7_div
.type nds7_div STT_FUNC
nds7_div:
@---------------------------------------------------------------------------------
	swi	0x09
	@coto: copy results to array divswiarr
	ldr r5,=divswiarr
	str r0,[r5,#0]	@DIV
	str r1,[r5,#4]	@MOD
	str r3,[r5,#8]	@ABS
	
	bx	lr

@Interrupt Wait for request
.global nds7intrwait
.type nds7intrwait STT_FUNC
nds7intrwait:
	//r0    	0=Return immediately if an old flag was already set (NDS9: bugged!)
	//			1=Discard old flags, wait until a NEW flag becomes set
	//r1 	    Interrupt flag(s) to wait for (same format as IE/IF registers)
	//swi 4*0x10000
	
	bx	lr
