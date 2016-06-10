/****************************************************************************

                THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <err.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "ukvm.h"


static int listen_socket_fd;
static int socket_fd;
static int stepping = 0;

#define MAX_BREAKPOINTS    8
static uint64_t breakpoints[MAX_BREAKPOINTS];

#define BUFSIZE 4096
#define MAX_CHAIN_LEN 10
static int gdb_fd[MAX_CHAIN_LEN];

int gdb_proxy_connect_to_ukvm(int ukvm_num, int portn)
{
    int sockfd, portno, n, r;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    assert(ukvm_num < MAX_CHAIN_LEN);

    /* check command line arguments */
    hostname = "localhost";
    portno = portn;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd > 0);

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    r = connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if (r != 0) {
        fprintf(stderr, "Could not connect to %s:%d\n", hostname, portn);
        return 1;
    }

#if 0
    /* get message line from the user */
    printf("Please enter msg: ");
    bzero(buf, BUFSIZE);
    fgets(buf, BUFSIZE, stdin);

    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    assert(n >= 0);

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    assert(n >= 0);
    printf("Echo from server: %s", buf);
#endif

    printf("Connected to ukvm %d at %s:%d\n", ukvm_num, hostname, portn);
    gdb_fd[ukvm_num] = sockfd;
    return 0;
}

void gdb_proxy_wait_for_connect(int portn)
{
    struct sockaddr_in sockaddr;
    socklen_t sockaddr_len;
    struct protoent *protoent;
    int r;
    int opt;

    printf("GDB trying to get a connection at port %d\n", portn);

    listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_socket_fd != -1);

    /* Allow rapid reuse of this port */
    opt = 1;
    r = setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt));
    if (r == -1) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    memset(&sockaddr, '\000', sizeof sockaddr);
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(portn);
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    r = bind(listen_socket_fd, (struct sockaddr *) &sockaddr,
             sizeof(sockaddr));
    if (r == -1) {
        perror("Failed to bind socket");
    }

    r = listen(listen_socket_fd, 0);
    if (r == -1) {
        perror("Failed to listen on socket");
    }

    sockaddr_len = sizeof sockaddr;
    socket_fd =
        accept(listen_socket_fd, (struct sockaddr *) &sockaddr,
               &sockaddr_len);
    if (socket_fd == -1) {
        perror("Failed to accept on socket");
    }
    close(listen_socket_fd);

    protoent = getprotobyname("tcp");
    if (!protoent) {
        perror("getprotobyname (\"tcp\") failed");
        return;
    }

    /* Disable Nagle - allow small packets to be sent without delay. */
    opt = 1;
    r = setsockopt(socket_fd, protoent->p_proto, TCP_NODELAY, &opt,
                   sizeof(opt));
    if (r == -1) {
        perror("setsockopt(TCP_NODELAY) failed");
    }
    int ip = sockaddr.sin_addr.s_addr;
    printf("GDB Connected to %d.%d.%d.%d\n", ip & 0xff, (ip >> 8) & 0xff,
           (ip >> 16) & 0xff, (ip >> 24) & 0xff);
}


static char buf[4096], *bufptr = buf;
static void flush_debug_buffer() 
{
    char *p = buf;
    while (p != bufptr) {
        int n = send(socket_fd, p, bufptr - p, 0);
        if (n == -1) {
            perror("error on debug socket: %m");
            break;
        }
        p += n;
    }
    bufptr = buf;
}


void putDebugChar(int ch)
{
    if (bufptr == buf + sizeof buf)
        flush_debug_buffer();
    *bufptr++ = ch;
}


int getDebugChar()
{
    char ch;

    recv(socket_fd, &ch, 1, 0);

    return (ch);
}


/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400 * 4

int remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

extern uint8_t *mem;

static const char hexchars[] = "0123456789abcdef";

/* Number of registers.  */
#define NUMREGS        32

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 8)

// list is here: gdb/amd64-linux-nat.c
enum regnames {
    RAX, RBX, RCX, RDX,
    RSI, RDI, RBP, RSP,
    R8, R9, R10, R11,
    R12, R13, R14, R15,
    RIP, EFLAGS, CS, SS,
    DS, ES, FS, GS
};

/*
 * these should not be static cuz they can be used outside this module
 */
long registers[NUMREGS];

/***************************  ASSEMBLY CODE MACROS *************************/
/*                                                                            */


int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return (ch - 'a' + 10);
    if ((ch >= '0') && (ch <= '9'))
        return (ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (ch - 'A' + 10);
    return (-1);
}


static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];


/* scan for the sequence $<data>#<checksum>     */

unsigned char *getpacket(void)
{
    unsigned char *buffer = &remcomInBuffer[0];
    unsigned char checksum;
    unsigned char xmitcsum;
    int count;
    char ch;

    while (1) {
        /* wait around for the start character, ignore all other characters */
        while ((ch = getDebugChar()) != '$');

      retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX - 1) {
            ch = getDebugChar();
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        buffer[count] = 0;

        if (ch == '#') {
            ch = getDebugChar();
            xmitcsum = hex(ch) << 4;
            ch = getDebugChar();
            xmitcsum += hex(ch);

            if (checksum != xmitcsum) {
                if (remote_debug) {
                    fprintf(stderr,
                            "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
                            checksum, xmitcsum, buffer);
                }
                putDebugChar('-');        /* failed checksum */
            } else {
                putDebugChar('+');        /* successful transfer */

                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':') {
                    putDebugChar(buffer[0]);
                    putDebugChar(buffer[1]);

                    return &buffer[3];
                }

                return &buffer[0];
            }
        }
    }
}

/* send the packet in buffer.  */

void putpacket(unsigned char *buffer)
{
    unsigned char checksum;
    int count;
    char ch;

    /*  $<packet info>#<checksum>.  */
    do {
        putDebugChar('$');
        checksum = 0;
        count = 0;

        while (ch = buffer[count]) {
            putDebugChar(ch);
            checksum += ch;
            count += 1;
        }

        putDebugChar('#');
        putDebugChar(hexchars[checksum >> 4]);
        putDebugChar(hexchars[checksum % 16]);
        flush_debug_buffer();
    }
    while (getDebugChar() != '+');
}

void debug_error(char *format, char *parm)
{
    if (remote_debug)
        fprintf(stderr, format, parm);
}


/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

void set_mem_err(void)
{
    mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
int get_char(char *addr)
{
    return *addr;
}


void set_char(char *addr, int val)
{
    *addr = val;
}


char *mem2hex(char *mem, char *buf, int count)
{
    int i;
    unsigned char ch;

    for (i = 0; i < count; i++) {
        ch = get_char(mem++);
        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch % 16];
    }
    *buf = 0;
    return (buf);
}


/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char *hex2mem(char *buf, char *mem, int count)
{
    int i;
    unsigned char ch;

    for (i = 0; i < count; i++) {
        ch = hex(*buf++) << 4;
        ch = ch + hex(*buf++);
        set_char(mem++, ch);
    }
    return (mem);
}


static int hexToLong(char **ptr, long *longValue)
{
    int numChars = 0;
    int hexValue;

    *longValue = 0;

    while (**ptr) {
        hexValue = hex(**ptr);
        if (hexValue < 0)
            break;

        *longValue = (*longValue << 4) | hexValue;
        numChars++;

        (*ptr)++;
    }

    return numChars;
}


int gdb_is_pc_breakpointing(uint64_t addr)
{
    int i;

    if (stepping)
        return 1;

    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (addr == breakpoints[i])
            return 1;
    }
    return 0;
}


int gdb_insert_breakpoint(uint64_t addr)
{
    int i;
    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (breakpoints[i] == 0) {
            breakpoints[i] = addr;
            return 1;
        }
    }
    return 0;
}


int gdb_remove_breakpoint(uint64_t addr)
{
    int i;
    for (i = 0; i < MAX_BREAKPOINTS; i++) {
        if (addr == breakpoints[i])
            breakpoints[i] = 0;
    }
    return 0;
}


void gdb_get_mem(uint64_t addr, int len,
                 char *obuf /* OUT */)
{
    char buf[1024], tbuf[1024];
    int n;
    int ukvm_num = 0;
    int count;
    char ch;
    int i;
    printf("%s:%d\n", __FUNCTION__, __LINE__);

    // $m0,10#2a

    unsigned char checksum = 0;
    sprintf(tbuf, "m%ld,%d",addr, len);

    count = 0;
    while (ch = tbuf[count]) {
        checksum += ch;
        count++;
    }

    sprintf(buf, "$%s#%c%c\n", tbuf,
                             hexchars[checksum >> 4],
                             hexchars[checksum % 16]);

    printf("sending %s\n", buf);

    /* send the message line to the server */
    n = write(gdb_fd[ukvm_num], buf, strlen(buf));
    assert(n >= 0);

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(gdb_fd[ukvm_num], buf, BUFSIZE);
    assert(n >= 0);
    //printf("Echo from server: %s\n", obuf);

    n = write(gdb_fd[ukvm_num], "++++", 5);
    assert(n >= 0);

    i = 0;
    while (buf[i++] != '$');
    memcpy(obuf, &buf[i], len);
}

void gdb_get_regs(char *obuf /* OUT */) {
    char buf[4096], tbuf[4096];
    int n;
    int ukvm_num = 0;
    int count;
    char ch;
    struct kvm_regs regs;
    struct kvm_sregs sregs;
    int i, ret;

    printf("%s:%d\n", __FUNCTION__, __LINE__);

    unsigned char checksum = 0;
    sprintf(tbuf, "g");

    count = 0;
    while (ch = tbuf[count]) {
        checksum += ch;
        count++;
    }

    sprintf(buf, "$%s#%c%c\n", tbuf,
                             hexchars[checksum >> 4],
                             hexchars[checksum % 16]);

    // $g#67
    printf("sending %s\n", buf);

    /* send the message line to the server */
    n = write(gdb_fd[ukvm_num], buf, strlen(buf));
    assert(n >= 0);

    /* print the server's reply */
    bzero(buf, NUMREGBYTES);
    n = read(gdb_fd[ukvm_num], buf, NUMREGBYTES);
    assert(n >= 0);
    printf("Echo from server: %s\n", buf);

    n = write(gdb_fd[ukvm_num], "++++", 5);
    assert(n >= 0);

    i = 0;
    while (buf[i++] != '$');
    memcpy(obuf, &buf[i], NUMREGBYTES);

    //mem2hex((char *) registers, obuf, NUMREGBYTES);
}
    
void gdb_proxy_handle_exception(int sig)
{
    char *buffer;
    char obuf[4096];
    int ne = 0;

    if (sig != 0) {
        snprintf(obuf, sizeof(obuf), "S%02x", 5);
        putpacket(obuf);
    }

    while (ne == 0) {
        buffer = getpacket();

        printf("command: %s\n", buffer);
        switch (buffer[0]) {
        case 's': {
            stepping = 1;
            return;
        }
        case 'c': {
            // Disable stepping for the next instruction
            stepping = 0;
            return; // Continue with program
        }
        case 'M': {
            putpacket("OK");
            break;
        }
        case 'm': {
            uint64_t addr;
            int len;
            char *ebuf;

            addr = strtoull(&buffer[1], &ebuf, 16);
            len = strtoul(ebuf + 1, NULL, 16);

            assert(sizeof(obuf) >= (len * 2) + 1);

            gdb_get_mem(addr, len, obuf);
            putpacket(obuf);
            
            break;
        }
        case 'P': {
            putpacket("OK");
            break;
        }
        case 'g': {
            assert(sizeof(obuf) >= (NUMREGBYTES * 2) + 1);
            gdb_get_regs(obuf);
            putpacket(obuf);
            break;
        }
        case '?': {
            sprintf(obuf, "S%02x", SIGTRAP);
            putpacket(obuf);
            break;
        }
        case 'H': {
            putpacket("OK");
            break;
        }
        case 'q': {
            // not supported
            putpacket("");
            break;
        }
        case 'Z': {
            // insert a breakpoint
            char *ebuf;
            uint64_t type = strtoull(buffer + 1, &ebuf, 16);
            uint64_t addr = strtoull(ebuf + 1, &ebuf, 16);
            uint64_t len = strtoull(ebuf + 1, &ebuf, 16);
            gdb_insert_breakpoint(addr);
            putpacket("OK");
            break;
        }
        case 'z': {
            // remove a breakpoint
            char *ebuf;
            uint64_t type = strtoull(buffer + 1, &ebuf, 16);
            uint64_t addr = strtoull(ebuf + 1, &ebuf, 16);
            uint64_t len = strtoull(ebuf + 1, &ebuf, 16);
            gdb_remove_breakpoint(addr);
            putpacket("OK");
            break;
        }
        case 'k': {
            printf("Debugger asked us to quit\n");
            exit(1);
        }
        case 'D': {
            printf("Debugger detached\n");
            putpacket("OK");
            return;
        }
        default:
            putpacket("");
            break;
        }
    }

    return;
}



