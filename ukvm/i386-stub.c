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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <linux/kvm.h>

/************************************************************************
 *
 * external low-level support routines
 */


static int listen_socket_fd;
static int socket_fd;

static void wait_for_connect(int portn)
{
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_len;
  struct protoent *protoent;
  int r;
  int opt;

  printf("trying to get a connection at port %d\n", portn);

  listen_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  assert(listen_socket_fd != -1);

  /* Allow rapid reuse of this port */
  opt = 1;
#if __MINGW32__
  r = setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
  r = setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
  if (r == -1)
  {
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  memset (&sockaddr, '\000', sizeof sockaddr);
#if BX_HAVE_SOCKADDR_IN_SIN_LEN
  // if you don't have sin_len change that to #if 0.  This is the subject of
  // bug [ 626840 ] no 'sin_len' in 'struct sockaddr_in'.
  sockaddr.sin_len = sizeof sockaddr;
#endif
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(portn);
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  r = bind(listen_socket_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
  if (r == -1)
  {
    perror("Failed to bind socket");
  }

  r = listen(listen_socket_fd, 0);
  if (r == -1)
  {
    perror("Failed to listen on socket");
  }

  sockaddr_len = sizeof sockaddr;
  socket_fd = accept(listen_socket_fd, (struct sockaddr *)&sockaddr, &sockaddr_len);
  if (socket_fd == -1)
  {
    perror("Failed to accept on socket");
  }
  close(listen_socket_fd);

  protoent = getprotobyname ("tcp");
  if (!protoent)
  {
    perror("getprotobyname (\"tcp\") failed");
    return;
  }

  /* Disable Nagle - allow small packets to be sent without delay. */
  opt = 1;
#ifdef __MINGW32__
  r = setsockopt (socket_fd, protoent->p_proto, TCP_NODELAY, (const char *)&opt, sizeof(opt));
#else
  r = setsockopt (socket_fd, protoent->p_proto, TCP_NODELAY, &opt, sizeof(opt));
#endif
  if (r == -1)
  {
    perror("setsockopt(TCP_NODELAY) failed");
  }
  int ip = sockaddr.sin_addr.s_addr;
  printf("Connected to %d.%d.%d.%d\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
}


void debug_loop(int, int);

void gdb_stub_start(int vcpufd)
{
    wait_for_connect(1234);
    debug_loop(vcpufd, 0);
}

static char buf[4096], *bufptr = buf;
static void flush_debug_buffer()
{
  char *p = buf;
  while (p != bufptr) {
    int n = send(socket_fd, p, bufptr-p, 0);
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

    return(ch);
}

void exceptionHandler()
{}

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400 * 4

static char initialized;  /* boolean flag. != 0 means we've been initialized */

int     remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

extern uint8_t *mem;

static const char hexchars[]="0123456789abcdef";

/* Number of registers.  */
#define NUMREGS	32

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

#define STACKSIZE 10000
int remcomStack[STACKSIZE/sizeof(int)];
static int* stackPtr = &remcomStack[STACKSIZE/sizeof(int) - 1];

/***************************  ASSEMBLY CODE MACROS *************************/
/* 									   */

#if 0
extern void
return_to_prog ();

/* Restore the program's registers (including the stack pointer, which
   means we get the right stack and don't have to worry about popping our
   return address and any stack frames and so on) and return.  */
asm(".text");
asm(".globl _return_to_prog");
asm("_return_to_prog:");
asm("        movw _registers+44, %ss");
asm("        movq _registers+16, %rsp");
asm("        movq _registers+4, %rcx");
asm("        movq _registers+8, %rdx");
asm("        movq _registers+12, %rbx");
asm("        movq _registers+20, %rbp");
asm("        movq _registers+24, %rsi");
asm("        movq _registers+28, %rdi");
asm("        movw _registers+48, %ds");
asm("        movw _registers+52, %es");
asm("        movw _registers+56, %fs");
asm("        movw _registers+60, %gs");
asm("        movq _registers+36, %rax");
asm("        pushq %rax");  /* saved eflags */
asm("        movq _registers+40, %rax");
asm("        pushq %rax");  /* saved cs */
asm("        movq _registers+32, %rax");
asm("        pushq %rax");  /* saved eip */
asm("        movq _registers, %rax");
/* use iret to restore pc and flags together so
   that trace flag works right.  */
asm("        iret");
#endif

#define BREAKPOINT() asm("   int $3");

/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_i386vector = -1;

/* GDB stores segment registers in 32-bit words (that's just the way
   m-i386v.h is written).  So zero the appropriate areas in registers.  */
#define SAVE_REGISTERS1()
#define SAVE_ERRCODE()
#define SAVE_REGISTERS2()

/* See if mem_fault_routine is set, if so just IRET to that address.  */
#define CHECK_FAULT()

#define CALL_HOOK()

/* This function is called when a i386 exception occurs.  It saves
 * all the cpu regs in the _registers array, munges the stack a bit,
 * and invokes an exception handler (remcom_handler).
 *
 * stack on entry:                       stack on exit:
 *   old eflags                          vector number
 *   old cs (zero-filled to 32 bits)
 *   old eip
 *
 */
extern void _catchException3();
asm(".text");
asm(".globl __catchException3");
asm("__catchException3:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $3");
CALL_HOOK();

/* Same thing for exception 1.  */
extern void _catchException1();
asm(".text");
asm(".globl __catchException1");
asm("__catchException1:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $1");
CALL_HOOK();

/* Same thing for exception 0.  */
extern void _catchException0();
asm(".text");
asm(".globl __catchException0");
asm("__catchException0:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $0");
CALL_HOOK();

/* Same thing for exception 4.  */
extern void _catchException4();
asm(".text");
asm(".globl __catchException4");
asm("__catchException4:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $4");
CALL_HOOK();

/* Same thing for exception 5.  */
extern void _catchException5();
asm(".text");
asm(".globl __catchException5");
asm("__catchException5:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $5");
CALL_HOOK();

/* Same thing for exception 6.  */
extern void _catchException6();
asm(".text");
asm(".globl __catchException6");
asm("__catchException6:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $6");
CALL_HOOK();

/* Same thing for exception 7.  */
extern void _catchException7();
asm(".text");
asm(".globl __catchException7");
asm("__catchException7:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $7");
CALL_HOOK();

/* Same thing for exception 8.  */
extern void _catchException8();
asm(".text");
asm(".globl __catchException8");
asm("__catchException8:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $8");
CALL_HOOK();

/* Same thing for exception 9.  */
extern void _catchException9();
asm(".text");
asm(".globl __catchException9");
asm("__catchException9:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $9");
CALL_HOOK();

/* Same thing for exception 10.  */
extern void _catchException10();
asm(".text");
asm(".globl __catchException10");
asm("__catchException10:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $10");
CALL_HOOK();

/* Same thing for exception 12.  */
extern void _catchException12();
asm(".text");
asm(".globl __catchException12");
asm("__catchException12:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $12");
CALL_HOOK();

/* Same thing for exception 16.  */
extern void _catchException16();
asm(".text");
asm(".globl __catchException16");
asm("__catchException16:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushq $16");
CALL_HOOK();

/* For 13, 11, and 14 we have to deal with the CHECK_FAULT stuff.  */

/* Same thing for exception 13.  */
extern void _catchException13 ();
asm (".text");
asm (".globl __catchException13");
asm ("__catchException13:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $13");
CALL_HOOK();

/* Same thing for exception 11.  */
extern void _catchException11 ();
asm (".text");
asm (".globl __catchException11");
asm ("__catchException11:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $11");
CALL_HOOK();

/* Same thing for exception 14.  */
extern void _catchException14 ();
asm (".text");
asm (".globl __catchException14");
asm ("__catchException14:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushq $14");
CALL_HOOK();


void
_returnFromException ()
{
  //return_to_prog ();
}

int
hex (ch)
     char ch;
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

unsigned char *
getpacket (void)
{
  unsigned char *buffer = &remcomInBuffer[0];
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  char ch;

  while (1)
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar ()) != '$')
	;

    retry:
      checksum = 0;
      xmitcsum = -1;
      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX - 1)
	{
	  ch = getDebugChar ();
	  if (ch == '$')
	    goto retry;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  ch = getDebugChar ();
	  xmitcsum = hex (ch) << 4;
	  ch = getDebugChar ();
	  xmitcsum += hex (ch);

	  if (checksum != xmitcsum)
	    {
	      if (remote_debug)
		{
		  fprintf (stderr,
			   "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
			   checksum, xmitcsum, buffer);
		}
	      putDebugChar ('-');	/* failed checksum */
	    }
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */

	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);

		  return &buffer[3];
		}

	      return &buffer[0];
	    }
	}
    }
}

/* send the packet in buffer.  */

void
putpacket (unsigned char *buffer)
{
  unsigned char checksum;
  int count;
  char ch;

  /*  $<packet info>#<checksum>.  */
  do
    {
      putDebugChar ('$');
      checksum = 0;
      count = 0;

      while (ch = buffer[count])
	{
	  putDebugChar (ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar ('#');
      putDebugChar (hexchars[checksum >> 4]);
      putDebugChar (hexchars[checksum % 16]);

    }
  while (getDebugChar () != '+');
}

void
debug_error (format, parm)
     char *format;
     char *parm;
{
  if (remote_debug)
    fprintf (stderr, format, parm);
}

/* Address of a routine to RTE to if we get a memory fault.  */
static void (*volatile mem_fault_routine) () = NULL;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

void
set_mem_err (void)
{
  mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
int
get_char (char *addr)
{
  return *addr;
}

void
set_char (char *addr, int val)
{
  *addr = val;
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */

char* mem2hex2(const char* mem, char* buf, int count)
{
  int i;
  for (i = 0; i<count; i++)
  {
    char ch = *mem++;
    *buf++ = hexchars[ch >> 4];
    *buf++ = hexchars[ch % 16];
  }
  *buf = 0;
  return(buf);
}

char *
mem2hex (mem, buf, count, may_fault)
     char *mem;
     char *buf;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i = 0; i < count; i++)
    {
      ch = get_char (mem++);
      if (may_fault && mem_err)
	return (buf);
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch % 16];
    }
  *buf = 0;
  if (may_fault)
    mem_fault_routine = NULL;
  return (buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char *
hex2mem (buf, mem, count, may_fault)
     char *buf;
     char *mem;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i = 0; i < count; i++)
    {
      ch = hex (*buf++) << 4;
      ch = ch + hex (*buf++);
      set_char (mem++, ch);
      if (may_fault && mem_err)
	return (mem);
    }
  if (may_fault)
    mem_fault_routine = NULL;
  return (mem);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
int
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector)
    {
    case 0:
      sigval = 8;
      break;			/* divide by zero */
    case 1:
      sigval = 5;
      break;			/* debug exception */
    case 3:
      sigval = 5;
      break;			/* breakpoint */
    case 4:
      sigval = 16;
      break;			/* into instruction (overflow) */
    case 5:
      sigval = 16;
      break;			/* bound instruction */
    case 6:
      sigval = 4;
      break;			/* Invalid opcode */
    case 7:
      sigval = 8;
      break;			/* coprocessor not available */
    case 8:
      sigval = 7;
      break;			/* double fault */
    case 9:
      sigval = 11;
      break;			/* coprocessor segment overrun */
    case 10:
      sigval = 11;
      break;			/* Invalid TSS */
    case 11:
      sigval = 11;
      break;			/* Segment not present */
    case 12:
      sigval = 11;
      break;			/* stack exception */
    case 13:
      sigval = 11;
      break;			/* general protection */
    case 14:
      sigval = 11;
      break;			/* page fault */
    case 16:
      sigval = 7;
      break;			/* coprocessor error */
    default:
      sigval = 7;		/* "software generated" */
    }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
int
hexToInt (char **ptr, int *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex (**ptr);
      if (hexValue >= 0)
	{
	  *intValue = (*intValue << 4) | hexValue;
	  numChars++;
	}
      else
	break;

      (*ptr)++;
    }

  return (numChars);
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
		numChars ++;

		(*ptr)++;
	}

	return numChars;
}



static void put_reply(const char* buffer)
{
  unsigned char csum;
  int i;

  do {
    putDebugChar('$');

    csum = 0;

    i = 0;
    while (buffer[i] != 0)
    {
      putDebugChar(buffer[i]);
      csum = csum + buffer[i];
      i++;
    }

    putDebugChar('#');
    putDebugChar(hexchars[csum >> 4]);
    putDebugChar(hexchars[csum % 16]);
    flush_debug_buffer();
  } while (getDebugChar() != '+');
  printf("put_reply '%s'\n", buffer);
}

static void get_command(char* buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  char ch;
  unsigned int count;
  unsigned int i;

  do {
    while ((ch = getDebugChar()) != '$');

    checksum = 0;
    xmitcsum = 0;
    count = 0;

    while (1)
    {
      ch = getDebugChar();
      if (ch == '#') break;
      checksum = checksum + ch;
      buffer[count] = ch;
      count++;
    }
    buffer[count] = 0;

    if (ch == '#')
    {
      xmitcsum = hex(getDebugChar()) << 4;
      xmitcsum += hex(getDebugChar());
      if (checksum != xmitcsum)
      {
        perror("Bad checksum");
      }
    }

    if (checksum != xmitcsum)
    {
      putDebugChar('-');
      flush_debug_buffer();
    }
    else
    {
      putDebugChar('+');
      if (buffer[2] == ':')
      {
        putDebugChar(buffer[0]);
        putDebugChar(buffer[1]);
        count = strlen(buffer);
        for (i = 3; i <= count; i++)
        {
          buffer[i - 3] = buffer[i];
        }
      }
      flush_debug_buffer();
    }
  } while (checksum != xmitcsum);
}


void debug_loop(int vcpufd, int sig)
{
  //char *buffer;
  char buffer[1024];
  char obuf[4096];
  int ne = 0;
  int stepping;
  //char mem[255];

  if (sig != 0) {
      snprintf(obuf, sizeof(obuf), "S%02x", 5);
      put_reply(obuf);
  }

  while (ne == 0)
  {
    get_command(buffer);
   // buffer = getpacket();

      printf("command: %s\n", buffer);
    switch (buffer[0])
    {

	case 's':
	  stepping = 1;
	case 'c':
	  /* try to read optional parameter, pc unchanged if no parm */
	 // if (hexToLong (&ptr, &addr))
	   // registers[PC] = addr;

	 // newPC = registers[PC];

	if (stepping) {
            struct kvm_guest_debug debug = {
                    .control        = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
            };

            if (ioctl(vcpufd, KVM_SET_GUEST_DEBUG, &debug) < 0)
                    err("KVM_SET_GUEST_DEBUG failed");
        }

        goto continue_to_program;

      case 'M':
      {
        put_reply("OK");
        break;
      }

      case 'm':
      {
        long addr;
        int len;
        char* ebuf;

        addr = strtoull(&buffer[1], &ebuf, 16);
        len = strtoul(ebuf + 1, NULL, 16);
        printf("addr %Lx len %x\n", addr, len);

        if ((addr + len) >= 0x20000000)
            memset(obuf, '0', len);
        else
            mem2hex(mem + addr, obuf, len);
        put_reply(obuf);
        break;
      }

      case 'P':
      {
        put_reply("OK");
        break;
      }

      case 'g':
      {
        struct kvm_regs regs;
        struct kvm_sregs sregs;
        int i, ret;

        ret = ioctl(vcpufd, KVM_GET_REGS, &regs);
        if (ret == -1)
            err(1, "KVM_GET_REGS");

        registers[RAX] = regs.rax;
        registers[RBX] = regs.rbx;
        registers[RCX] = regs.rcx;
        registers[RDX] = regs.rdx;

        registers[RSI] = regs.rsi;
        registers[RDI] = regs.rdi;
        registers[RBP] = regs.rbp;
        registers[RSP] = regs.rsp;

        registers[R8] = regs.r8;
        registers[R9] = regs.r9;
        registers[R10] = regs.r10;
        registers[R11] = regs.r11;
        registers[R12] = regs.r12;
        registers[R13] = regs.r13;
        registers[R14] = regs.r14;
        registers[R15] = regs.r15;

        registers[RIP] = regs.rip;
        registers[EFLAGS] = regs.rflags;

        //ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
        //if (ret == -1)
        //     err(1, "KVM_GET_SREGS");

        //registers[CS] = sregs.cs;
        //registers[SS] = sregs.ss;

        mem2hex ((char *) registers, obuf, NUMREGBYTES, 0);

        put_reply(obuf);
        break;
      }

      case '?':
        sprintf(obuf, "S%02x", SIGTRAP);
        put_reply(obuf);
        break;

      case 'H':
        put_reply("OK");
        break;

      case 'q':
        {
          put_reply(""); /* not supported */
        }
        break;

      case 'Z':
        //do_breakpoint(1, buffer+1);
        break;
      case 'z':
        //do_breakpoint(0, buffer+1);
        break;
      case 'k':
        //BX_PANIC(("Debugger asked us to quit"));
        break;
      case 'D':
        printf("Debugger detached\n");
        put_reply("OK");
        return;
        break;

      default:
        put_reply("");
        break;
    }
  }

continue_to_program:
  return;
}


/*
 * This function does all command procesing for interfacing to gdb.
 */
#if 0
void
handle_exception (int exceptionVector)
{
  int sigval, stepping;
  long addr;
  int length;
  char *ptr;
  int newPC;

  gdb_i386vector = exceptionVector;

  if (remote_debug)
    {
      printf ("vector=%d, sr=0x%x, pc=0x%x\n",
	      exceptionVector, registers[PS], registers[PC]);
    }

  /* reply to host that an exception has occurred */
  sigval = computeSignal (exceptionVector);

  ptr = remcomOutBuffer;

  *ptr++ = 'T';			/* notify gdb with signo, PC, FP and SP */
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[ESP]; 
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[ESP], ptr, 4, 0);	/* SP */
  *ptr++ = ';';

  *ptr++ = hexchars[EBP]; 
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[EBP], ptr, 4, 0); 	/* FP */
  *ptr++ = ';';

  *ptr++ = hexchars[PC]; 
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[PC], ptr, 4, 0); 	/* PC */
  *ptr++ = ';';

  *ptr = '\0';

  putpacket (remcomOutBuffer);

  stepping = 0;

  while (1 == 1)
    {
      remcomOutBuffer[0] = 0;
      ptr = getpacket ();

      switch (*ptr++)
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval % 16];
	  remcomOutBuffer[3] = 0;
	  break;
	case 'd':
	  remote_debug = !(remote_debug);	/* toggle debug flag */
	  break;
	case 'g':		/* return the value of the CPU registers */
	  mem2hex ((char *) registers, remcomOutBuffer, NUMREGBYTES, 0);
	  break;
	case 'G':		/* set the value of the CPU registers - return OK */
	  hex2mem (ptr, (char *) registers, NUMREGBYTES, 0);
	  strcpy (remcomOutBuffer, "OK");
	  break;
	case 'P':		/* set the value of a single CPU register - return OK */
	  {
	    int regno;

	    if (hexToInt (&ptr, &regno) && *ptr++ == '=')
	      if (regno >= 0 && regno < NUMREGS)
		{
		  hex2mem (ptr, (char *) &registers[regno], 4, 0);
		  strcpy (remcomOutBuffer, "OK");
		  break;
		}

	    strcpy (remcomOutBuffer, "E01");
	    break;
	  }

	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	case 'm':
	  /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	  if (hexToLong (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		{
		  ptr = 0;
		  mem_err = 0;
		  mem2hex ((char *) addr, remcomOutBuffer, length, 1);
		  if (mem_err)
		    {
		      strcpy (remcomOutBuffer, "E03");
		      debug_error ("memory fault");
		    }
		}

	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E01");
	    }
	  break;

	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	case 'M':
	  /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	  if (hexToLong (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		if (*(ptr++) == ':')
		  {
		    mem_err = 0;
		    hex2mem (ptr, (char *) addr, length, 1);

		    if (mem_err)
		      {
			strcpy (remcomOutBuffer, "E03");
			debug_error ("memory fault");
		      }
		    else
		      {
			strcpy (remcomOutBuffer, "OK");
		      }

		    ptr = 0;
		  }
	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E02");
	    }
	  break;

	  /* cAA..AA    Continue at address AA..AA(optional) */
	  /* sAA..AA   Step one instruction from AA..AA(optional) */
	case 's':
	  stepping = 1;
	case 'c':
	  /* try to read optional parameter, pc unchanged if no parm */
	  if (hexToLong (&ptr, &addr))
	    registers[PC] = addr;

	  newPC = registers[PC];

	  /* clear the trace bit */
	  registers[PS] &= 0xfffffeff;

	  /* set the trace bit if we're stepping */
	  if (stepping)
	    registers[PS] |= 0x100;

	  //_returnFromException ();	/* this is a jump */
	  break;

	  /* kill the program */
	case 'k':		/* do nothing */
#if 0
	  /* Huh? This doesn't look like "nothing".
	     m68k-stub.c and sparc-stub.c don't have it.  */
	  BREAKPOINT ();
#endif
	  break;
	}			/* switch */

      /* reply to the request */
      putpacket (remcomOutBuffer);
    }
}
#endif

/* this function is used to set up exception handlers for tracing and
   breakpoints */
void
set_debug_traps (void)
{
  stackPtr = &remcomStack[STACKSIZE / sizeof (int) - 1];

/*
  exceptionHandler (0, _catchException0);
  exceptionHandler (1, _catchException1);
  exceptionHandler (3, _catchException3);
  exceptionHandler (4, _catchException4);
  exceptionHandler (5, _catchException5);
  exceptionHandler (6, _catchException6);
  exceptionHandler (7, _catchException7);
  exceptionHandler (8, _catchException8);
  exceptionHandler (9, _catchException9);
  exceptionHandler (10, _catchException10);
  exceptionHandler (11, _catchException11);
  exceptionHandler (12, _catchException12);
  exceptionHandler (13, _catchException13);
  exceptionHandler (14, _catchException14);
  exceptionHandler (16, _catchException16);
*/
  initialized = 1;
}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger.  */

void
breakpoint (void)
{
  if (initialized)
    BREAKPOINT ();
}
