/* 
 * copyright (c) 2015-2017 contributors as noted in the authors file
 *
 * this file is part of solo5, a unikernel base layer.
 *
 * permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * the software is provided "as is" and the author disclaims all
 * warranties with regard to this software including all implied
 * warranties of merchantability and fitness. in no event shall the
 * author be liable for any special, direct, indirect, or
 * consequential damages or any damages whatsoever resulting from loss
 * of use, data or profits, whether in an action of contract,
 * negligence or other tortious action, arising out of or in
 * connection with the use or performance of this software.
 */

#include "kernel.h"

void solo5_exec(unsigned char *elf_mem, size_t elf_mem_len)
{
    struct ukvm_exec buf;

    buf.elf_mem = (char *)elf_mem;
    buf.elf_mem_len = elf_mem_len;

    ukvm_do_hypercall(UKVM_HYPERCALL_EXEC, &buf);
}
