/*-
 * Copyright (c) 2016 Martin Lucina.  All Rights Reserved.
 *
 * Based on rumprun/hw arch/amd64/locore.S, which is:
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define ENTRY(x) .text; .globl x; .type x,%function; x:
#define END(x)   .size x, . - x

//.code32

.align 4

.data
.space 4096
bootstack:

/*
 * Bootloader entry point.
 *
 * Bootstrap is slightly different from i386.  Multiboot puts us only
 * in 32bit mode, so it's our responsibility to install a page table
 * and switch to long mode.  Notably, we can't call C code until
 * we've switched to long mode.
 */
ENTRY(_start3)
	cld
	cli

	//movl $0x1ffffff8, %esp
	//xorl %ebp, %ebp

        pushl $0x2000
        //call c_start32
END(_start3)


ENTRY(_start2)
	cld
	cli
	movl $bootstack, %esp

	//lgdt (gdtr)
	//pushw $0x0
	//pushw $0x8
	//pushl $1f
	//lret

	movl $0x10, %eax
	movl %eax, %ds
	movl %eax, %es
	movl %eax, %ss

	xorl %eax, %eax
	movl %eax, %fs
	movl %eax, %gs

	/* 4: enable protected mode */
	//movl %cr0, %eax
	//orl $0x1, %eax
	//movl %eax, %cr0

	/* 5: poetically longjump to 32bit */
	pushw $0x8
	pushl $_start32
	lret

	/* NOTREACHED */
	jmp haltme

haltme:
	cli
	hlt
	jmp haltme
END(_start2)

/*
 * amd64 programmer's manual:
 *
 * "In long mode, segmentation is not used ... except for a few exceptions."
 *
 * Uuuyea, exceptions.
 */

//.code32
.data

.align 4
.globl gdtr
gdtr:
	.word gdt32_end - gdt32 - 1
	.long gdt32
.align 4

.globl gdt32
gdt32:
	/* Entry 0x0: Null desriptor */
	.long 0x0
	.long 0x0
	/* Entry 0x8: Code segment */
	.word 0xffff		       //Limit
  	.word 0x0000		       //Base 15:00
	.byte 0x00			//Base 23:16
	.word 0xcf9a		       //Flags / Limit / Type [F,L,F,Type]
	.byte 0x00			       //Base 32:24
	// Entry 0x10: Data segment
	.word 0xffff		       //Limit
	.word 0x0000		       //Base 15:00
	.byte 0x00		       //Base 23:16
	.word 0xcf92		      //Flags / Limit / Type [F,L,F,Type]
	.byte 0x00			//Base 32:24
.globl gdt32_end
gdt32_end:
	.byte 0
.align 4

ENTRY(_start32)
        movl $0x1ffffff8, %esp
	xorl %ebp, %ebp

        pushl $0x2000

	/*call kernel_main*/
        //call c_start32

	cli
	hlt
END(_start32)

