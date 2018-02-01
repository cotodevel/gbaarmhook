.align 4
.code 32
.arm
.cpu arm7tdmi
.text	

@load/store
.global ldru32extasm	
.type ldru32extasm STT_FUNC
ldru32extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldr r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldru16extasm	
.type ldru16extasm STT_FUNC
ldru16extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrh r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldru8extasm	
.type ldru8extasm STT_FUNC
ldru8extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrb r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr


@taken from: http://www.finesse.demon.co.uk/steven/sqrt.html
@ (r0)IN :  n 32 bit unsigned integer
@ (r5)OUT:  root = INT (SQRT (n))
@ (r4)TMP:  offset
@ (r7) i: 

.global sqrtasm
.type sqrtasm STT_FUNC
sqrtasm:
	STMFD sp!, {r1-r12,lr}		
	mov r2,r0	@IN
	
	mov r7,#0x0	@i
	mov r8,#2	@imm #2
	mov r4, #3 << 30
	mov r5, #1 << 30
	innerloop:
		muls	r9,	r8,	r7
		CMP    	r2, r5, ROR	r9
		SUBHS  	r2, r2, r5, ROR	r9
		ADC    	r5, r4, r5, LSL #1
		cmp 	r7	,#0xf
	addne r7,#1
	bne innerloop
	BIC    r5, r5, #3 << 30  	@ for rounding add: CMP n, root  ADC root, #1
	mov r0,r5
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global shift_word
shift_word:
	movs r6,r5,lsl #16	
	movs r6,r6,lsr #16	@0xabcd
	
	movs r7,r5,ror #16	@0xefgh0000
	movs r7,r7,lsl #16
	
	orr r6,r7,r6		@0xabcdefgh
bx lr

.global wramtstasm
.type wramtstasm STT_FUNC
wramtstasm:
	STMFD sp!, {r2-r12,lr}		
	
	mov r2,#0
	mov r3,#0
	ldr r4,=0xc172c374
	mov r5,#0
	@r0= wram_start / r1= wram_top / r2 = i / r3 = *(i+iwram) / r4 = stub
	@r5 = any
	b0: 
		cmp r2,r1		@i == top
		bne b1
		beq b3

	b1:  
		add r3,r0,r2
		str r4,[r3]
		ldr r3,[r3]		@r3 = *(i+iwram)
		
			//mov r5,r3
			//bl shift_word	@r5 = io
		
			cmp r3,r4 	@and test read
			bne b3
			add r2,#4	@i++
	b b0
		
	b3:
	mov r0,r2
	
	LDMFD sp!, {r2-r12,lr}
bx lr

@---------------------------------------------------------------------------------
.global branchtoaddr
.type branchtoaddr STT_FUNC
branchtoaddr:

@usage: r0 = arg0 for next call / r1 = address to jump
bx r1

.align
.pool
.end
