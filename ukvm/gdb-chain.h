#ifndef __GDB_CHAIN_H__
#define __GDB_CHAIN_H__

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#define GDB_CHAIN_BUF_LEN 20


/* gdb_proxy.c */
void gdb_proxy_wait_for_connect(int portn);
void gdb_proxy_handle_exception(int sig);

#endif
