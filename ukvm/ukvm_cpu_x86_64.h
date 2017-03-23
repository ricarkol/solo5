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

#ifndef __UKVM_CPU_X86_64_H__
#define __UKVM_CPU_X86_64_H__

#ifndef _BITUL

#ifdef __ASSEMBLY__
#define _AC(X,Y)                X
#define _AT(T,X)                X
#else
#define __AC(X,Y)               (X##Y)
#define _AC(X,Y)                __AC(X,Y)
#define _AT(T,X)                ((T)(X))
#endif

#define _BITUL(x)               (_AC(1,UL) << (x))
#define _BITULL(x)              (_AC(1,ULL) << (x))

#endif

/*
 * Basic CPU control in CR0
 */
#define X86_CR0_PE_BIT          0 /* Protection Enable */
#define X86_CR0_PE              _BITUL(X86_CR0_PE_BIT)
#define X86_CR0_MP_BIT          1 /* Monitor Coprocessor */
#define X86_CR0_MP              _BITUL(X86_CR0_MP_BIT)
#define X86_CR0_EM_BIT          2 /* Emulation */
#define X86_CR0_EM              _BITUL(X86_CR0_EM_BIT)
#define X86_CR0_PG_BIT          31 /* Paging */
#define X86_CR0_PG              _BITUL(X86_CR0_PG_BIT)

/*
 * Intel CPU features in CR4
 */
#define X86_CR4_PAE_BIT         5 /* enable physical address extensions */
#define X86_CR4_PAE             _BITUL(X86_CR4_PAE_BIT)
#define X86_CR4_OSFXSR_BIT      9 /* OS support for FXSAVE/FXRSTOR */
#define X86_CR4_OSFXSR          _BITUL(X86_CR4_OSFXSR_BIT)
#define X86_CR4_OSXMMEXCPT_BIT  10 /* OS support for FP exceptions */
#define X86_CR4_OSXMMEXCPT      _BITUL(X86_CR4_OSXMMEXCPT_BIT)

/* 
 * Intel CPU features in EFER
 */
#define X86_EFER_LME_BIT        8 /* Long mode enable (R/W) */
#define X86_EFER_LME            _BITUL(X86_EFER_LME_BIT)
#define X86_EFER_LMA_BIT        10 /* Long mode active (R/O) */
#define X86_EFER_LMA            _BITUL(X86_EFER_LMA_BIT)

/*
 * Intel long mode page directory/table entries
 */
#define X86_PDPT_P_BIT          0 /* Present */
#define X86_PDPT_P              _BITUL(X86_PDPT_P_BIT)
#define X86_PDPT_RW_BIT         1 /* Writable */
#define X86_PDPT_RW             _BITUL(X86_PDPT_RW_BIT)
#define X86_PDPT_PS_BIT         7 /* Page size */
#define X86_PDPT_PS             _BITUL(X86_PDPT_PS_BIT)

/*
 * Abstract x86 segment descriptor.
 */
struct x86_seg {
    uint64_t base;
    uint32_t limit;
    uint8_t type;
    uint8_t p, dpl, db, s, l, g, avl;
};


#define GDT_DESC_OFFSET(n) ((n) * 0x8)

#define GDT_GET_BASE(x) (                      \
    (((x) & 0xFF00000000000000) >> 32) |       \
    (((x) & 0x000000FF00000000) >> 16) |       \
    (((x) & 0x00000000FFFF0000) >> 16))

#define GDT_GET_LIMIT(x) (uint32_t)(                                      \
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

struct _kvm_segment {
    uint64_t base;
    uint32_t limit;
    uint16_t selector;
    uint8_t type;
    uint8_t present, dpl, db, s, l, g, avl;
    uint8_t unusable;
    uint8_t padding;
};

#define GDT_GET_G(x)   (uint8_t)(((x) & 0x0080000000000000) >> 55)
#define GDT_GET_DB(x)  (uint8_t)(((x) & 0x0040000000000000) >> 54)
#define GDT_GET_L(x)   (uint8_t)(((x) & 0x0020000000000000) >> 53)
#define GDT_GET_AVL(x) (uint8_t)(((x) & 0x0010000000000000) >> 52)
#define GDT_GET_P(x)   (uint8_t)(((x) & 0x0000800000000000) >> 47)
#define GDT_GET_DPL(x) (uint8_t)(((x) & 0x0000600000000000) >> 45)
#define GDT_GET_S(x)   (uint8_t)(((x) & 0x0000100000000000) >> 44)
#define GDT_GET_TYPE(x)(uint8_t)(((x) & 0x00000F0000000000) >> 40)

#define GDT_TO_KVM_SEGMENT(seg, gdt_table, sel) \
    do {                                        \
        uint64_t gdt_ent = gdt_table[sel];         \
        seg.base = GDT_GET_BASE(gdt_ent);       \
        seg.limit = GDT_GET_LIMIT(gdt_ent);     \
        seg.selector = sel * 8;                 \
        seg.type = GDT_GET_TYPE(gdt_ent);       \
        seg.present = GDT_GET_P(gdt_ent);       \
        seg.dpl = GDT_GET_DPL(gdt_ent);         \
        seg.db = GDT_GET_DB(gdt_ent);           \
        seg.s = GDT_GET_S(gdt_ent);             \
        seg.l = GDT_GET_L(gdt_ent);             \
        seg.g = GDT_GET_G(gdt_ent);             \
        seg.avl = GDT_GET_AVL(gdt_ent);         \
    } while (0)

static const struct x86_seg ukvm_x86_seg_code = {
    .base = 0,
    .limit = 0xfffff,
    .type = 8, .p = 1, .dpl = 0, .db = 0, .s = 1, .l = 0, .g = 1
};

static const struct x86_seg ukvm_x86_seg_data = {
    .base = 0,
    .limit = 0xfffff,
    .type = 2, .p = 1, .dpl = 0, .db = 1, .s = 1, .l = 0, .g = 1
};
 
/*
 * x86 segment descriptor as seen by the CPU in the GDT.
 */
struct x86_gdt_desc {
    uint64_t limit_lo:16;
    uint64_t base_lo:24;
    uint64_t type:4;
    uint64_t s:1;
    uint64_t dpl:2;
    uint64_t p:1;
    uint64_t limit_hi:4;
    uint64_t avl:1;
    uint64_t l:1;
    uint64_t db:1;
    uint64_t g:1;
    uint64_t base_hi:8;
} __attribute__((packed));

struct gdt_entry
{
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

/* Special pointer which includes the limit: The max bytes
*  taken up by the GDT, minus 1. Again, this NEEDS to be packed */
struct gdt_ptr
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

enum x86_gdt_selector {
    X86_GDT_NULL,
    X86_GDT_CODE,
    X86_GDT_DATA,
    X86_GDT_TSS_LO,
    X86_GDT_TSS_HI,
    X86_GDT_MAX
};

#define X86_GDTR_LIMIT ((sizeof (struct x86_gdt_desc) * X86_GDT_MAX) - 1)

/*
 * Monitor memory map for x86 CPUs:
 *
 * (unused) X86_ZEROPAGE_BASE   0x0
 * (unused) X86_ZEROPAGE_SIZE   0x1000
 */
#define X86_GDT_BASE            0x1000
#define X86_GDT_SIZE            0x1000
#define X86_PML4_BASE           0x2000
#define X86_PML4_SIZE           0x1000
#define X86_PDPTE_BASE          0x3000
#define X86_PDPTE_SIZE          0x1000
#define X86_PDE_BASE            0x4000
#define X86_PDE_SIZE            0x1000
#define X86_BOOT_INFO_BASE      0x5000
#define X86_BOOT_INFO_SIZE      0x1000
#define X86_CMDLINE_BASE        0x6000
#define X86_CMDLINE_SIZE        0x2000
#define X86_GUEST_MIN_BASE      0x8000

#define X86_GUEST_PAGE_SIZE     0x200000

#define X86_INIT_EFER_SET       X86_EFER_LME
#define X86_INIT_CR0_SET        (X86_CR0_PE /*| X86_CR0_PG*/)
#define X86_INIT_CR0_CLEAR      X86_CR0_EM
#define X86_INIT_CR3            X86_PML4_BASE
#define X86_INIT_CR4_SET        (X86_CR4_PAE | X86_CR4_OSFXSR | \
                                X86_CR4_OSXMMEXCPT)
#define X86_INIT_RFLAGS         0x2    /* rflags bit 1 is reserved, must be 1 */

void ukvm_x86_setup_pagetables(uint8_t *mem, size_t mem_size);
void ukvm_x86_setup_gdt(uint8_t *mem);

#endif
