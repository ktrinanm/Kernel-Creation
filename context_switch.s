.global activate
activate:
	mov ip, sp
	push {r4,r5,r6,r7,r8,r9,r10,fp,ip,lr}

	ldmfd r0!, {ip,lr} /*Get SPSR and lr */
	msr SPSR, ip
	
	msr CPSR_c, #0xDF  /*System Mode */
	mov sp, r0
	pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,fp,ip,lr}
	msr CPSR_c, #0xD3  /*Supervisor mode */

	movs pc, lr

.global svc_entry
svc_entry:
	/* Save User state */
	msr CPSR_c, #0xDF /*System mode */
	push {r0, r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,fp,ip,lr}
	mov r0, sp
	msr CPSR_c, #0xD3 /* Supervisor mode */

	mrs ip, SPSR
	stmfd r0!, {ip, lr}

	/* Load kernel state */
	pop {r4,r5,r6,r7,r8,r9,r10,fp,ip,lr}
	mov sp, ip
	bx lr
