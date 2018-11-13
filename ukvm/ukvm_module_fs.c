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

/*
 * ukvm_module_blk.c: Block device module.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "ukvm.h"

static struct ukvm_blkinfo blkinfo;
static char *diskfile;
int diskfd;

static void hypercall_blkinfo(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkinfo *info =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkinfo));

    info->sector_size = blkinfo.sector_size;
    info->num_sectors = blkinfo.num_sectors;
    info->rw = blkinfo.rw;
}

static void hypercall_blkwrite(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkwrite *wr =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkwrite));
    ssize_t ret;
    off_t pos, end;

    assert(wr->len <= SSIZE_MAX);
    if (wr->sector >= blkinfo.num_sectors) {
        wr->ret = -1;
        return;
    }
    pos = (off_t)blkinfo.sector_size * (off_t)wr->sector;
    if (add_overflow(pos, wr->len, end)
            || (end > blkinfo.num_sectors * blkinfo.sector_size)) {
        wr->ret = -1;
        return;
    }

    ret = pwrite(diskfd, UKVM_CHECKED_GPA_P(hv, wr->data, wr->len), wr->len,
            pos);
    assert(ret == wr->len);
    wr->ret = 0;
}

static void hypercall_blkread(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_blkread *rd =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_blkread));
    ssize_t ret;
    off_t pos, end;

    assert(rd->len <= SSIZE_MAX);
    if (rd->sector >= blkinfo.num_sectors) {
        rd->ret = -1;
        return;
    }
    pos = (off_t)blkinfo.sector_size * (off_t)rd->sector;
    if (add_overflow(pos, rd->len, end)
            || (end > blkinfo.num_sectors * blkinfo.sector_size)) {
        rd->ret = -1;
        return;
    }

    ret = pread(diskfd, UKVM_CHECKED_GPA_P(hv, rd->data, rd->len), rd->len,
            pos);
    assert(ret == rd->len);
    rd->ret = 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (strncmp("--dir=", cmdarg, 6))
        return -1;
    diskfile = cmdarg + 6;

    return 0;
}

extern char memlfs_start;
void *addr;

extern int genlfs(char *directory, char *image);

static int setup(struct ukvm_hv *hv)
{
    if (diskfile == NULL)
        return -1;

    assert((uint64_t)&memlfs_start % 4096 == 0);

    uint64_t maddr;
    for (maddr = (uint64_t)&memlfs_start;
		maddr < ((uint64_t)&memlfs_start + 128*1024*1024*1024ULL);
		maddr += 1024*1024*1024ULL) {
	    addr = mmap((void *)maddr, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	    memlfs_start = 'a';
	    printf("%p==%p %c\n", addr, (void*)maddr, memlfs_start);
	    assert(addr == (void *)maddr);
    }

    genlfs(diskfile, "test.lfs");

    /* set up virtual disk */
    diskfd = open("test.lfs", O_RDWR);
    if (diskfd == -1)
        err(1, "Could not open disk: %s", "test.lfs");

    blkinfo.sector_size = 512;
    blkinfo.num_sectors = lseek(diskfd, 0, SEEK_END) / 512;
    blkinfo.rw = 1;

    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKINFO,
                hypercall_blkinfo) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKWRITE,
                hypercall_blkwrite) == 0);
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_BLKREAD,
                hypercall_blkread) == 0);

    return 0;
}

static char *usage(void)
{
    return "--dir=PATH (path exposed to the unikernel to be converted to a raw block device)";
}

static int get_fd(void)
{
    return diskfd;
}
struct ukvm_module ukvm_module_blk = {
    .name = "blk",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage,
    .get_fd = get_fd,
};
