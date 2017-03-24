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

#ifndef __UKVM_GUEST_H__
#define __UKVM_GUEST_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
/*
 * PIO base address used to dispatch hypercalls.
 */
#define UKVM_HYPERCALL_PIO_BASE 0x500

#    ifdef __UKVM_HOST__
/*
 * Non-dereferencable monitor-side type representing a guest physical address.
 */
typedef uint64_t ukvm_gpa_t;
#    else
/*
 * On x86, 32-bit PIO is used as the hypercall mechanism. This only supports
 * sending 32-bit pointers; raise an assertion if a bigger pointer is used.
 *
 * On x86 the compiler-only memory barrier ("memory" clobber) is sufficient
 * across the hypercall boundary.
 */
static inline void ukvm_do_hypercall(int n, volatile void *arg)
{
    assert(((uint64_t)arg <= UINT32_MAX));
    __asm__ __volatile__("outl %0, %1"
            :
            : "a" ((uint32_t)((uint64_t)arg)),
              "d" ((uint16_t)(UKVM_HYPERCALL_PIO_BASE + n))
            : "memory");
}
#    endif

#else

/*
 * PIO base address used to dispatch hypercalls.
 */
#define UKVM_HYPERCALL_PIO_BASE 0x500

#    ifdef __UKVM_HOST__
/*
 * Non-dereferencable monitor-side type representing a guest physical address.
 */
typedef uint32_t ukvm_gpa_t;
#    else
/*
 * On x86, 32-bit PIO is used as the hypercall mechanism. This only supports
 * sending 32-bit pointers; raise an assertion if a bigger pointer is used.
 *
 * On x86 the compiler-only memory barrier ("memory" clobber) is sufficient
 * across the hypercall boundary.
 */
static inline void ukvm_do_hypercall(int n, volatile void *arg)
{
    assert(((uint32_t)arg <= UINT32_MAX));
    __asm__ __volatile__("outl %0, %1"
            :
            : "a" ((uint32_t)((uint32_t)arg)),
              "d" ((uint16_t)(UKVM_HYPERCALL_PIO_BASE + n))
            : "memory");
}
#    endif


#endif

/*
 * Guest-provided pointers in all structures shared between the guest and
 * monitor MUST be declared with UKVM_GUEST_PTR(type), where type is the
 * desired guest-side pointer type. ukvm_gpa_t (defined earlier) will be used
 * as the monitor-side type.
 *
 * This ensures that these pointers are not directly dereferencable on the
 * monitor side.
 */
#ifdef __UKVM_HOST__
#    define UKVM_GUEST_PTR(T) ukvm_gpa_t
#else
#    define UKVM_GUEST_PTR(T) T
#endif

/*
 * A pointer to this structure is passed by the monitor as the sole argument to
 * the guest entrypoint.
 */
struct ukvm_boot_info {
    uint32_t mem_size;                  /* Memory size in bytes */
    uint32_t kernel_end;                /* Address of end of kernel */
    uint32_t cmdline;     /* Address of command line (C string) */
};
/*
 * Maximum size of guest command line, including the string terminator.
 */
#define UKVM_CMDLINE_SIZE 8192

/*
 * Canonical list of hypercalls supported by all monitor modules. Actual calls
 * supported at run time depend on module configuration at build time.
 */
enum ukvm_hypercall {
    /* UKVM_HYPERCALL_RESERVED=0 */
    UKVM_HYPERCALL_PUTS=1,
    UKVM_HYPERCALL_POLL,
    UKVM_HYPERCALL_BLKINFO,
    UKVM_HYPERCALL_BLKWRITE,
    UKVM_HYPERCALL_BLKREAD,
    UKVM_HYPERCALL_NETINFO,
    UKVM_HYPERCALL_NETWRITE,
    UKVM_HYPERCALL_NETREAD,
    UKVM_HYPERCALL_MAX
};

/*
 * Hypercall definitions follow.
 */

/* UKVM_HYPERCALL_PUTS */
struct ukvm_puts {
    /* IN */
    //UKVM_GUEST_PTR(const char *) data;
    uint32_t data;
    //uint32_t len;
    uint32_t len;
};

/* UKVM_HYPERCALL_BLKINFO */
struct ukvm_blkinfo {
    /* OUT */
    uint32_t sector_size;
    uint32_t num_sectors;
    int rw;
};

/* UKVM_HYPERCALL_BLKWRITE */
struct ukvm_blkwrite {
    /* IN */
    uint32_t sector;
    uint32_t data;
    uint32_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_BLKREAD */
struct ukvm_blkread {
    /* IN */
    uint32_t sector;
    uint32_t data;

    /* IN/OUT */
    uint32_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_NETINFO */
struct ukvm_netinfo {
    /* OUT */
    char mac_str[18];
};

/* UKVM_HYPERCALL_NETWRITE */
struct ukvm_netwrite {
    /* IN */
    uint32_t data;
    uint32_t len;

    /* OUT */
    int ret;
};

/* UKVM_HYPERCALL_NETREAD */
struct ukvm_netread {
    /* IN */
    uint32_t data;

    /* IN/OUT */
    uint32_t len;

    /* OUT */
    int ret;
};

/*
 * UKVM_HYPERCALL_POLL: Block until timeout_nsecs have passed or I/O is
 * possible, whichever is sooner. Returns 1 if I/O is possible, otherwise 0.
 *
 * TODO: Extend this interface to select which I/O events are of interest.
 */
struct ukvm_poll {
    /* IN */
    uint64_t timeout_nsecs;

    /* OUT */
    int ret;
};

#endif