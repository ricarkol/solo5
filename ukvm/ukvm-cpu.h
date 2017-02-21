/* 
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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

#ifndef __UKVM_CPU_H__
#define __UKVM_CPU_H__

#ifndef _BITUL

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#define _BITUL(x)	(_AC(1,UL) << (x))
#define _BITULL(x)	(_AC(1,ULL) << (x))

#endif

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */



/*
 * CPU control registers
 */
#define X86_CR0_PE 0x00000001 /* Protected mode Enable */
#define X86_CR0_NE 0x00000020 /* Numeric Error enable (EX16 vs IRQ13) */
#define X86_CR0_PG 0x80000000 /* PaGing enable */
#define X86_CR0_NW 0x20000000 /* Not Write-through */
#define X86_CR0_CD 0x40000000 /* Cache Disable */
#define X86_CR0_MP 0x00000002 /* "Math" (fpu) Present */
#define X86_CR0_EM 0x00000004 /* EMulate FPU instructions. (trap ESC only) */

#define X86_CR4_TSD 0x00000004 /* Time Stamp Disable */
#define X86_CR4_PAE 0x00000020 /* Physical address extension */
#define X86_CR4_VMXE 0x00002000 /* enable VMX operation (Intel-specific) */
#define X86_CR4_FXSR 0x00000200 /* Fast FPU save/restore used by OS */
#define X86_CR4_XMM 0x00000400 /* enable SIMD/MMX2 to use except 16 */

#define X86_EFER_LME 0x000000100 /* Long mode enable (R/W) */
#define X86_EFER_LMA 0x000000400 /* Long mode active (R) */
#define X86_EFER_NXE		(1 << 11)


/*
 * Intel long mode page directory/table entries
 */
#define X86_PDPT_P_BIT          0 /* Present */
#define X86_PDPT_P              _BITUL(X86_PDPT_P_BIT)
#define X86_PDPT_RW_BIT         1 /* Writable */
#define X86_PDPT_RW             _BITUL(X86_PDPT_RW_BIT)
#define X86_PDPT_USR_BIT        2 /* usr */
#define X86_PDPT_USR             _BITUL(X86_PDPT_USR_BIT)
#define X86_PDPT_PS_BIT         7 /* Page size */
#define X86_PDPT_PS             _BITUL(X86_PDPT_PS_BIT)

/*
 * GDT and KVM segment manipulation
 */

#define GDT_DESC_OFFSET(n) ((n) * 0x8)

#define GDT_GET_BASE(x) (                      \
    (((x) & 0xFF00000000000000) >> 32) |       \
    (((x) & 0x000000FF00000000) >> 16) |       \
    (((x) & 0x00000000FFFF0000) >> 16))

#define GDT_GET_LIMIT(x) (__u32)(                                      \
                                 (((x) & 0x000F000000000000) >> 32) |  \
                                 (((x) & 0x000000000000FFFF)))

/* Constructor for a conventional segment GDT (or LDT) entry */
/* This is a macro so it can be used in initializers */
#define GDT_ENTRY(flags, base, limit)               \
    ((((base)  & _AC(0xff000000, ULL)) << (56-24)) | \
     (((flags) & _AC(0x0000f0ff, ULL)) << 40) |      \
     (((limit) & _AC(0x000f0000, ULL)) << (48-16)) | \
     (((base)  & _AC(0x00ffffff, ULL)) << 16) |      \
     (((limit) & _AC(0x0000ffff, ULL))))

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
#define I86_GDT_GRAND_64_MODE			0x20			//00100000

//! set if 32bit. default: 16 bit
#define I86_GDT_GRAND_32BIT			0x40			//01000000

//! 4k grandularity. default: none
#define I86_GDT_GRAND_4K			0x80			//10000000

struct _kvm_segment {
    __u64 base;
    __u32 limit;
    __u16 selector;
    __u8 type;
    __u8 present, dpl, db, s, l, g, avl;
    __u8 unusable;
    __u8 padding;
};

struct gdt_descriptor {
    //! bits 0-15 of segment limit
    uint16_t		limit;

    //! bits 0-23 of base address
    uint16_t		baseLo;
    uint8_t		baseMid;

    //! descriptor access flags
    uint8_t		flags;
    uint8_t		grand;

    //! bits 24-32 of base address
    uint8_t		baseHi;
} __attribute__((packed)) gdt_descriptor_t;

struct tss {
    uint32_t reserved;
    uint64_t rsp[3];
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed));


#define GDT_GET_G(x)   (__u8)(((x) & 0x0080000000000000) >> 55)
#define GDT_GET_DB(x)  (__u8)(((x) & 0x0040000000000000) >> 54)
#define GDT_GET_L(x)   (__u8)(((x) & 0x0020000000000000) >> 53)
#define GDT_GET_AVL(x) (__u8)(((x) & 0x0010000000000000) >> 52)
#define GDT_GET_P(x)   (__u8)(((x) & 0x0000800000000000) >> 47)
#define GDT_GET_DPL(x) (__u8)(((x) & 0x0000600000000000) >> 45)
#define GDT_GET_S(x)   (__u8)(((x) & 0x0000100000000000) >> 44)
#define GDT_GET_TYPE(x)(__u8)(((x) & 0x00000F0000000000) >> 40)

#define GDT_TO_KVM_SEGMENT(seg, gdt_table, sel) \
    do {                                        \
        __u64 gdt_ent = gdt_table[sel];         \
        seg.base = GDT_GET_BASE(gdt_ent);       \
        seg.limit = GDT_GET_LIMIT(gdt_ent);     \
        seg.selector = (sel * 8) +              \
                       GDT_GET_DPL(gdt_ent);    \
        seg.type = GDT_GET_TYPE(gdt_ent);       \
        seg.present = GDT_GET_P(gdt_ent);       \
        seg.dpl = GDT_GET_DPL(gdt_ent);         \
        seg.db = GDT_GET_DB(gdt_ent);           \
        seg.s = GDT_GET_S(gdt_ent);             \
        seg.l = GDT_GET_L(gdt_ent);             \
        seg.g = GDT_GET_G(gdt_ent);             \
        seg.avl = GDT_GET_AVL(gdt_ent);         \
    } while (0)

#endif
