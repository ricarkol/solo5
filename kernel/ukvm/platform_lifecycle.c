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

void platform_init(void *arg)
{
    process_bootinfo(arg);
}

void platform_exit(int status, void *cookie, size_t len)
{
    struct ukvm_halt h;
    memset((void *)&h, 0, sizeof(struct ukvm_halt));

    h.exit_status = status;

    if (len && len <= UKVM_HYPERCALL_MAX_DUMP_INFO_SIZE) {
        memcpy((void *)&h.data, cookie, len);
        h.len = len;
    }

    ukvm_do_hypercall(UKVM_HYPERCALL_HALT, &h);
    for(;;);
}
