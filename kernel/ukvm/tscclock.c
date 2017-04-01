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

/* Wall clock offset at monotonic time base. */
static uint64_t wc_epochoffset;

/*
 * Beturn monotonic time using TSC clock.
 */
uint64_t tscclock_monotonic(void)
{
    return 0;
}

/*
 * Initialise TSC clock.
 *
 * Implementation notes: This is a purely TSC-based clock with the following
 * requirements:
 *
 * 1. The host TSC MUST be invariant, as defined in Intel SDM section 17.15.1
 * "Invariant TSC".
 * 2. The host hypervisor MUST NOT implement any RDTSC emulation.
 *
 * It is up to the monitor to ensure that these requirements are met, and to
 * supply the TSC frequency to the guest.
 */
int tscclock_init(uint64_t tsc_freq)
{
    tsc_freq = tsc_freq;
    return 0;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
uint64_t tscclock_epochoffset(void)
{
	return wc_epochoffset;
}
