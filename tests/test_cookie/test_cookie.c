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

#include "solo5.h"
#include "../../kernel/lib.c"

#define NUM_ELEMS(x)  ((sizeof(x) / sizeof((x)[0])))

#include "cookies.h"

int rdrand32(unsigned int* result)
{
  int res = 0;
  while (res == 0) {
      res = __builtin_ia32_rdrand32_step(result);
  }
  return (res == 1);
}

int solo5_app_main(char *cmdline __attribute__((unused)))
{
    unsigned int rand = -1;
    unsigned int i;
    unsigned int last_cookie = NUM_ELEMS(cookies);
    char *cookie;

    rdrand32(&rand);
    i = (rand % last_cookie) + 1;
    cookie = cookies[i];

    puts("{\"type\":\"cookie-out\",\"data\":\"");
    puts(cookie);
    puts("\"}\n");

    return 0;
}
