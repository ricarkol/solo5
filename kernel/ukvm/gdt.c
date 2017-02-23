/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a unikernel base layer.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "kernel.h"

/* granularity (23), long mode (21), present (15),
 * always 1 (12, 11), readable (9), limit (16-19)
 */
#define GDT_DESC_CODE_VAL (0x00af9a000000ffff)
/* granularity (23), big data seg (22), present (15),
 * type data rw (9), limit (16-19)
 */
#define GDT_DESC_DATA_VAL     (0x00cf92000000ffff)

#define GDT_DESC_USR_CODE_VAL (0x008ffb000000ffff)
#define GDT_DESC_USR_DATA_VAL (0x00cff3000000ffff)


struct __attribute__((__packed__)) gdtptr {
    uint16_t limit;
    uint64_t base;
};

uint64_t cpu_gdt64[GDT_NUM_ENTRIES] ALIGN_64_BIT;

//! set access bit
#define I86_GDT_DESC_ACCESS			0x0001			//00000001

//! descriptor is readable and writable. default: read only
#define I86_GDT_DESC_READWRITE			0x0002			//00000010

//! set expansion direction bit
#define I86_GDT_DESC_EXPANSION			0x0004			//00000100

//! executable code segment. Default: data segment
#define I86_GDT_DESC_EXEC_CODE			0x0008			//00001000

//! set code or data descriptor. defult: system defined descriptor
#define I86_GDT_DESC_CODEDATA			0x0010			//00010000

//! set dpl bits
#define I86_GDT_DESC_DPL			0x0060			//01100000

//! set "in memory" bit
#define I86_GDT_DESC_MEMORY			0x0080			//10000000

/**	gdt descriptor grandularity bit flags	***/

//! masks out limitHi (High 4 bits of limit)
#define I86_GDT_GRAND_LIMITHI_MASK		0x0f			//00001111

//! set os defined bit
#define I86_GDT_GRAND_OS			0x10			//00010000

//! set os defined bit
#define I86_GDT_GRAND_64BIT			0x20			//00100000

//! set if 32bit. default: 16 bit
#define I86_GDT_GRAND_32BIT			0x40			//01000000

//! 4k grandularity. default: none
#define I86_GDT_GRAND_4K			0x80			//10000000

struct gdt_descriptor {
	//! bits 0-15 of segment limit
	uint16_t		limit;

	//! bits 0-23 of base address
	uint16_t		baseLo;
	uint8_t			baseMid;

	//! descriptor access flags
	uint8_t			flags;

	uint8_t			grand;

	//! bits 24-32 of base address
	uint8_t			baseHi;
} __attribute__((packed)) gdt_descriptor_t;



// copied from https://searchco.de/file/101550660/p42/SysCore/Hal/gdt.cpp#l-87
void gdt_set_descriptor(struct gdt_descriptor *_gdt, uint8_t i, uint64_t base,
                        uint64_t limit, uint8_t access, uint8_t grand) {
	memset ((void*)&_gdt[i], 0, sizeof (struct gdt_descriptor));
	_gdt[i].baseLo	= (uint16_t)(base & 0xffff);
	_gdt[i].baseMid	= (uint8_t)((base >> 16) & 0xff);
	_gdt[i].baseHi	= (uint8_t)((base >> 24) & 0xff);
	_gdt[i].limit	= (uint16_t)(limit & 0xffff);
	_gdt[i].flags = access;
	_gdt[i].grand = (uint8_t)((limit >> 16) & 0x0f);
	_gdt[i].grand |= grand & 0xf0;
}

/*
 * Ukvm starts up with a bootstrap GDT which is "invisible" to the guest, init
 * and switch to our own GDT.
 */
void gdt_init(void)
{
    struct gdtptr gdtptr;
    struct gdt_descriptor *gdt = (struct gdt_descriptor *) &cpu_gdt64;

    /* initialize GDT "pointer" */
    gdtptr.limit = sizeof(cpu_gdt64) - 1;
    gdtptr.base = (uint64_t)&cpu_gdt64;

    /* clear structures */
    memset(cpu_gdt64, 0, sizeof(cpu_gdt64));
    //gdt_set_descriptor (gdt, GDT_DESC_NULL,0,0,I86_GDT_DESC_DPL,0);

    //cpu_gdt64[GDT_DESC_CODE] = GDT_DESC_CODE_VAL;
    //cpu_gdt64[GDT_DESC_DATA] = GDT_DESC_DATA_VAL;
    //cpu_gdt64[GDT_DESC_USR_CODE] = GDT_DESC_USR_CODE_VAL;
    //cpu_gdt64[GDT_DESC_USR_DATA] = GDT_DESC_USR_DATA_VAL;

    gdt_set_descriptor (gdt, GDT_DESC_CODE,0,0xfffff,
	I86_GDT_DESC_READWRITE|I86_GDT_DESC_EXEC_CODE|I86_GDT_DESC_CODEDATA|
	I86_GDT_DESC_MEMORY/*I86_GDT_DESC_ACCESS*/, I86_GDT_GRAND_4K |
	I86_GDT_GRAND_LIMITHI_MASK | I86_GDT_GRAND_64BIT);

    gdt_set_descriptor (gdt, GDT_DESC_DATA,0,0xfffff,
	I86_GDT_DESC_READWRITE|I86_GDT_DESC_CODEDATA|I86_GDT_DESC_MEMORY/*|
	I86_GDT_DESC_ACCESS*/,
	I86_GDT_GRAND_4K | /*I86_GDT_GRAND_32BIT |*/ I86_GDT_GRAND_LIMITHI_MASK);

    printf("code %p\n", (uint64_t) cpu_gdt64[GDT_DESC_CODE]);
    printf("data %p\n", (uint64_t) cpu_gdt64[GDT_DESC_DATA]);

    gdt_set_descriptor (gdt, GDT_DESC_USR_CODE,0,0xfffff,
	I86_GDT_DESC_READWRITE|I86_GDT_DESC_EXEC_CODE|I86_GDT_DESC_CODEDATA|
	I86_GDT_DESC_MEMORY|/*I86_GDT_DESC_ACCESS|*/(I86_GDT_DESC_DPL),
	I86_GDT_GRAND_4K | I86_GDT_GRAND_LIMITHI_MASK | I86_GDT_GRAND_64BIT);

    gdt_set_descriptor (gdt, GDT_DESC_USR_DATA,0,0xfffff,
	I86_GDT_DESC_READWRITE|I86_GDT_DESC_CODEDATA|I86_GDT_DESC_MEMORY/*|
	I86_GDT_DESC_ACCESS*/|(I86_GDT_DESC_DPL),
	I86_GDT_GRAND_4K | /*I86_GDT_GRAND_32BIT |*/ I86_GDT_GRAND_LIMITHI_MASK);

    printf("code %x\n", GDT_DESC_CODE*8);
    printf("data %x\n", GDT_DESC_DATA*8);
    printf("usr code %x\n", GDT_DESC_USR_CODE*8 + 3);
    printf("usr data %x\n", GDT_DESC_USR_DATA*8 + 3);
    printf("tss %x\n", GDT_DESC_TSS*8 + 3);

    cpu_gdt_load((uint64_t)&gdtptr);
}
