#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <sys/mman.h>
#include <pthread.h>

#include <zlib.h>
#include <linux/kvm.h>

#include "queue.h"
#include "libudis86/udis86.h"

#include "ukvm.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_rr.h"

#include <fcntl.h>
#include "lz4.h"


//#define RR_DO_CHECKS
#ifdef RR_DO_CHECKS
#include "ukvm_module_rr_checks.h"
#else
#define HEAVY_CHECKS_IN(f) do{}while(0)
#define HEAVY_CHECKS_OUT(f) do{}while(0)
#define CHECK(a,b,c) do{}while(0)
#define CHECKS_INIT() do{}while(0)
#endif

struct trap_t {
    ukvm_gpa_t    insn_off;        /* Instruction offset (address) */
    int           insn_mnemonic;   /* Really an enum ud_mnemonic_code */
    int           insn_len;        /* Instruction length in bytes */
    ud_operand_t  insn_opr[2];     /* Instruction operands (max 2) */

    SLIST_ENTRY(trap_t) entries;
};

SLIST_HEAD(traps_head, trap_t);
static struct traps_head traps;

int rr_mode = RR_MODE_NONE;
static int rr_fd;
int rr_pipe[2];

#define BLOCK_BYTES (1024 * 128)

#include <pthread.h>
#include <semaphore.h>
// N must be 2^i
#define N (64)

struct ring_item_t {
   char buf[BLOCK_BYTES];
   int sz;
};

struct ring_item_t *b;
uint64_t in = 0, out = 0;
sem_t countsem, spacesem;

void enqueue(char *buf, int sz)
{
    // wait if there is no space left:
    sem_wait( &spacesem );

    b[in % N].sz = sz;
    if (sz > 0)
        memcpy(b[in % N].buf, buf, sz);
    __sync_fetch_and_add(&in, 1);

    // increment the count of the number of items
    sem_post(&countsem);
}

struct ring_item_t dequeue(){
    // Wait if there are no items in the buffer
    sem_wait(&countsem);

    struct ring_item_t result = b[(out++) & (N-1)];

    // Increment the count of the number of spaces
    sem_post(&spacesem);

    return result;
}

static char staging[BLOCK_BYTES];
static int staging_pos = 0;

void rr(int l, uint8_t *x, size_t sz, const char *func, int line)
{
    int ret;
    if ((l == RR_LOC_IN) && (rr_mode == RR_MODE_REPLAY)) {
        ret = read(rr_fd, x, sz);
        if (ret == 0)
            errx(0, "Reached end of replay\n");
        assert(ret == sz);
        //printf("%s reading val=%llu sz=%zu\n", func, *((unsigned long long *)x), sz);
        printf("%s\n", func);
        fflush(stdout);
    }
    if ((l == RR_LOC_OUT) && (rr_mode == RR_MODE_RECORD)) {
        //printf("%s recording val=%llu sz=%zu\n", func, *((unsigned long long *)x), sz);

        //enqueue((char *)x, sz); return;

        assert(sz < BLOCK_BYTES);
        if (staging_pos + sz > BLOCK_BYTES) {
            enqueue(staging, staging_pos);
            staging_pos = 0;
        }

        memcpy(staging + staging_pos, x, sz);
        staging_pos += sz;
        assert(sz < BLOCK_BYTES);
    }
}

#define RR(l, x, s) do {                                        \
        rr(l, (uint8_t *)(x), s, __FUNCTION__, __LINE__);       \
    } while (0)

static int init_traps(struct ukvm_hv *hv)
{
    struct kvm_guest_debug dbg = {0};

    /* This disassemble takes ~5ms */
    ud_t ud_obj;
    ud_init(&ud_obj);
    ud_set_input_buffer(&ud_obj, hv->mem + hv->p_entry,
                        hv->p_tend - hv->p_entry);
    ud_set_mode(&ud_obj, 64);
    ud_set_pc(&ud_obj, hv->p_entry);
    ud_set_syntax(&ud_obj, UD_SYN_INTEL);

// mnemonic=528 off=100200 len=2
// mnemonic=528 off=10025c len=2
    {
            struct trap_t *trap;
            int i;
            trap = malloc(sizeof(struct trap_t));
            assert(trap);
            memset(trap, 0, sizeof(struct trap_t));
            trap->insn_off = 0x100200;
            trap->insn_mnemonic = 528;
            trap->insn_len = 2;
            /* We replace the first byte with an int3, which traps */
            uint8_t *addr = UKVM_CHECKED_GPA_P(hv, trap->insn_off,
                                               trap->insn_len);
            addr[0] = 0xcc; /* int3 */
            for (i = 1; i < trap->insn_len; i++)
                addr[i] = 0x90; /* nop */
            SLIST_INSERT_HEAD(&traps, trap, entries);
    }
    {
            struct trap_t *trap;
            int i;
            trap = malloc(sizeof(struct trap_t));
            assert(trap);
            memset(trap, 0, sizeof(struct trap_t));
            trap->insn_off = 0x10025c;
            trap->insn_mnemonic = 528;
            trap->insn_len = 2;
            /* We replace the first byte with an int3, which traps */
            uint8_t *addr = UKVM_CHECKED_GPA_P(hv, trap->insn_off,
                                               trap->insn_len);
            addr[0] = 0xcc; /* int3 */
            for (i = 1; i < trap->insn_len; i++)
                addr[i] = 0x90; /* nop */
            SLIST_INSERT_HEAD(&traps, trap, entries);
    }

    dbg.control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;
    if (ioctl(hv->b->vcpufd, KVM_SET_GUEST_DEBUG, &dbg) == -1) {
        /* The KVM_CAP_SET_GUEST_DEBUG capbility is not available. */
        err(1, "KVM_SET_GUEST_DEBUG failed");
        return -1;
    }

    return 0;
}

static void rdtsc_emulate(struct ukvm_hv *hv, struct trap_t *trap)
{
    uint64_t tscval = 0;
    uint32_t eax, edx;
    struct kvm_regs regs;
    int ret;
    
    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &regs);
    assert(ret == 0);

    RR_INPUT(hv, rdtsc, &tscval);
    
    __asm__("rdtsc" : "=a" (eax), "=d" (edx));
    tscval = ((uint64_t)edx << 32) | eax;

    RR_OUTPUT(hv, rdtsc, &tscval);
    
    regs.rax = tscval & ~0ULL;
    regs.rdx = (tscval >> 32) & ~0ULL;
    regs.rip += trap->insn_len;
    
    ret = ioctl(hv->b->vcpufd, KVM_SET_REGS, &regs);
    assert(ret == 0);
}

static void rdrand_emulate(struct ukvm_hv *hv, struct trap_t *trap)
{
    uint64_t randval = 0;
    struct kvm_regs regs;
    int ret;

/*
IF HW_RND_GEN.ready = 1
    THEN
         CASE of
              osize is 64: DEST[63:0] <- HW_RND_GEN.data;
              osize is 32: DEST[31:0] <- HW_RND_GEN.data;
              osize is 16: DEST[15:0] <- HW_RND_GEN.data;
         ESAC
         CF <- 1;
    ELSE
         CASE of
              osize is 64: DEST[63:0] <- 0;
              osize is 32: DEST[31:0] <- 0;
              osize is 16: DEST[15:0] <- 0;
         ESAC
         CF <- 0;
FI
OF, SF, ZF, AF, PF <- 0;
*/

    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &regs);
    assert(ret == 0);

    RR_INPUT(hv, rdrand, &randval);

    // XXX: this is pretty bad
    randval = ((long long)rand() << 32) | rand();
    
    RR_OUTPUT(hv, rdrand, &randval);

    assert(trap->insn_opr[0].size == 64);
    assert(trap->insn_opr[0].type == UD_OP_REG);

    switch (trap->insn_opr[0].base) {
    case UD_R_RAX:
        regs.rax = randval;
        break;

    case UD_R_RBX:
        regs.rbx = randval;
        break;

    case UD_R_RCX:
        regs.rcx = randval;
        break;

    case UD_R_RDX:
        regs.rdx = randval;
        break;

    default:
        errx(1, "unexpected rdrand register");
    }

    regs.rflags |= 1; // CF
    regs.rflags &= ~(1 << 11); // OF
    regs.rflags &= ~(1 << 7); // SF
    regs.rflags &= ~(1 << 6); // ZF
    regs.rflags &= ~(1 << 4); // AF
    regs.rflags &= ~(1 << 2); // PF
    regs.rip += trap->insn_len;

    ret = ioctl(hv->b->vcpufd, KVM_SET_REGS, &regs);
    assert(ret == 0);
}

static void cpuid_emulate(struct ukvm_hv *hv, struct trap_t *trap)
{
    struct cpuid_t cpuid;
    struct kvm_regs regs;
    int ret;
    
    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &regs);
    assert(ret == 0);

    cpuid.code = regs.rax;
    cpuid.eax = cpuid.ebx = cpuid.ecx = cpuid.edx = 0;

    RR_INPUT(hv, cpuid, &cpuid);

    __asm__ volatile("cpuid"
                     :"=a"(cpuid.eax),"=b"(cpuid.ebx),
                     "=c"(cpuid.ecx),"=d"(cpuid.edx)
                     :"a"((uint32_t)cpuid.code));

    RR_OUTPUT(hv, cpuid, &cpuid);
    
    regs.rax = cpuid.eax;
    regs.rbx = cpuid.ebx;
    regs.rcx = cpuid.ecx;
    regs.rdx = cpuid.edx;
    regs.rip += trap->insn_len;
    
    ret = ioctl(hv->b->vcpufd, KVM_SET_REGS, &regs);
    assert(ret == 0);
}

void test_compress(FILE* outFp, FILE* inpFp);
void test_decompress(FILE* outFp, FILE* inpFp);

static size_t write_int(FILE* fp, int i) {
    return fwrite(&i, sizeof(i), 1, fp);
}

static size_t write_bin(FILE* fp, const void* array, size_t arrayBytes) {
    return fwrite(array, 1, arrayBytes, fp);
}

void *rr_dump()
{
    int fd;
    char name[] = "rr_out.dat.XXXXXX";
    fd = mkstemp(name);
    FILE* outFp = fdopen(fd, "wb");

    LZ4_stream_t lz4Stream_body;
    LZ4_stream_t* lz4Stream = &lz4Stream_body;

    LZ4_resetStream(lz4Stream);

    while (1) {
        sem_wait(&countsem);
        struct ring_item_t *item = &b[out % N];
        //char *ptr = item->buf;
        __sync_fetch_and_add(&out, 1);
        //printf("%s recording val=%llu sz=%d\n", "-", *((unsigned long long *)ptr),item->sz);

        if (item->sz < 0) {
            sem_post(&spacesem);
            //printf("item->sz < 0\n");
            break;
        }
        char* inpPtr = item->buf;
        const int inpBytes = item->sz;
        {
            char cmpBuf[LZ4_COMPRESSBOUND(BLOCK_BYTES)];
            const int cmpBytes = LZ4_compress_fast_continue(
                lz4Stream, inpPtr, cmpBuf, inpBytes, sizeof(cmpBuf), 32);
            assert(cmpBytes > 0);
            if (cmpBytes <= 0) {
                printf("cmpBytes <=0\n");
                sem_post(&spacesem);
                break;
            }
            write_int(outFp, cmpBytes);
            write_bin(outFp, cmpBuf, (size_t) cmpBytes);
        }

        sem_post(&spacesem);
    }

    fflush(outFp);
    fclose(outFp);
    return NULL;
}

void *rr_read()
{
    FILE* inpFp = fopen("rr_out.dat.lz4", "rb");
    FILE* outFp = fdopen(rr_pipe[1], "wb"); // [1] is writer

    setvbuf(inpFp, NULL, _IONBF, 0);
    setvbuf(outFp, NULL, _IONBF, 0);
    test_decompress(outFp, inpFp);

    fclose(outFp);
    fclose(inpFp);

    return NULL;
}

pthread_t tid;

static void handle_ukvm_exit(void)
{
    enqueue(staging, staging_pos);
    enqueue(NULL, -1);
    close(rr_fd);
    pthread_join(tid, NULL);
}

static int rr_init(char *rr_file)
{
    CHECKS_INIT();
    int res;

    atexit(handle_ukvm_exit);

    res = pipe(rr_pipe);
    if (res < 0)
        err(1, "Failed to create pipe for rr");
   
    switch (rr_mode) {
    case RR_MODE_RECORD: {
        pthread_create(&tid, NULL, rr_dump, NULL);

        b = calloc(sizeof(struct ring_item_t), N);
        assert(b);
        sem_init(&countsem, 0, 0);
        sem_init(&spacesem, 0, N);

        break;
    }

    case RR_MODE_REPLAY: {
        pthread_create(&tid, NULL, rr_read, NULL);

        rr_fd = rr_pipe[0]; // used to read (0)
        assert(fcntl(rr_fd, F_SETPIPE_SZ, 1024 * 1024) > 0);
        if (rr_fd <= 0)
            errx(1, "couldn't open rr file %s\n", rr_file);
        break;
    }

    default:
        return -1;
    }
    return 0;
}
    
void rr_ukvm_walltime(struct ukvm_hv *hv, struct ukvm_walltime *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &o->nsecs, sizeof(o->nsecs));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_puts(struct ukvm_hv *hv, struct ukvm_puts *o, int loc)
{
    HEAVY_CHECKS_IN();
        
    CHECK(loc, &o->data, sizeof(o->data));
    CHECK(loc, UKVM_CHECKED_GPA_P(hv, o->data, o->len), o->len);
    CHECK(loc, &o->len, sizeof(o->len));

    HEAVY_CHECKS_OUT();
}
    
void rr_ukvm_poll(struct ukvm_hv *hv, struct ukvm_poll *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    CHECK(loc, &o->timeout_nsecs, sizeof(o->timeout_nsecs));
    RR(loc, &o->ret, sizeof(o->ret));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_rdtsc(struct ukvm_hv *hv, uint64_t *new_tsc, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &*new_tsc, sizeof(*new_tsc));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_netinfo(struct ukvm_hv *hv, struct ukvm_netinfo *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &o->mac_str, sizeof(o->mac_str));

    HEAVY_CHECKS_OUT();
}
void rr_ukvm_netwrite(struct ukvm_hv *hv, struct ukvm_netwrite *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    CHECK(loc, &o->data, sizeof(o->data));
    CHECK(loc, UKVM_CHECKED_GPA_P(hv, o->data, o->len), o->len);
    CHECK(loc, &o->len, sizeof(o->len));
    RR(loc, &o->ret, sizeof(o->ret));

    HEAVY_CHECKS_OUT();
}
void rr_ukvm_netread(struct ukvm_hv *hv, struct ukvm_netread *o, int loc)
{
    HEAVY_CHECKS_IN();

    CHECK(loc, &o->data, sizeof(o->data));
    RR(loc, &o->len, sizeof(o->len));
    RR(loc, UKVM_CHECKED_GPA_P(hv, o->data, o->len), o->len);
    RR(loc, &o->ret, sizeof(o->ret));

    HEAVY_CHECKS_OUT();
}
#if 0
void rr_ukvm_boot_info(struct ukvm_hv *hv, struct ukvm_boot_info *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &o->mem_size, sizeof(o->mem_size));
    RR(loc, &o->kernel_end, sizeof(o->kernel_end));
    RR(loc, &o->cmdline_len, sizeof(o->cmdline_len));
    RR(loc, UKVM_CHECKED_GPA_P(hv, o->cmdline, o->cmdline_len), o->cmdline_len);

    HEAVY_CHECKS_OUT();
}
#endif
void rr_ukvm_blkinfo(struct ukvm_hv *hv, struct ukvm_blkinfo *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &o->sector_size, sizeof(o->sector_size));
    RR(loc, &o->num_sectors, sizeof(o->num_sectors));
    RR(loc, &o->rw, sizeof(o->rw));

    HEAVY_CHECKS_OUT();
}
void rr_ukvm_blkwrite(struct ukvm_hv *hv, struct ukvm_blkwrite *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    CHECK(loc, &o->sector, sizeof(o->sector));
    CHECK(loc, &o->data, sizeof(o->data));
    CHECK(loc, UKVM_CHECKED_GPA_P(hv, o->data, o->len), o->len);
    CHECK(loc, &o->len, sizeof(o->len));
    RR(loc, &o->ret, sizeof(o->ret));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_blkread(struct ukvm_hv *hv, struct ukvm_blkread *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    CHECK(loc, &o->sector, sizeof(o->sector));
    CHECK(loc, &o->data, sizeof(o->data));
    RR(loc, &o->len, sizeof(o->len));
    RR(loc, UKVM_CHECKED_GPA_P(hv, o->data, o->len), o->len);
    RR(loc, &o->ret, sizeof(o->ret));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_cpuid(struct ukvm_hv *hv, struct cpuid_t *o, int loc)
{
    HEAVY_CHECKS_IN();
    
    CHECK(loc, &o->code, sizeof(o->code));
    RR(loc, &o->eax, sizeof(o->eax));
    RR(loc, &o->ebx, sizeof(o->ebx));
    RR(loc, &o->ecx, sizeof(o->ecx));
    RR(loc, &o->edx, sizeof(o->edx));

    HEAVY_CHECKS_OUT();
}

void rr_ukvm_rdrand(struct ukvm_hv *hv, uint64_t *randval, int loc)
{
    HEAVY_CHECKS_IN();
    
    RR(loc, &*randval, sizeof(*randval));

    HEAVY_CHECKS_OUT();
}

static int handle_vmexits(struct ukvm_hv *hv)
{
    /* if it's not an int3 for a rdtsc or rdrand, let the vcpu loop do it */
    if (hv->b->vcpurun->exit_reason != KVM_EXIT_DEBUG)
        return -1;

    struct kvm_regs regs;
    struct trap_t *trap;
    int ret;
    
    ret = ioctl(hv->b->vcpufd, KVM_GET_REGS, &regs);
    assert(ret == 0);

    SLIST_FOREACH(trap, &traps, entries) {
        if (trap->insn_off == regs.rip) {
            switch (trap->insn_mnemonic) {
            case UD_Irdtsc:
                rdtsc_emulate(hv, trap);
                break;

            case UD_Irdrand:
                rdrand_emulate(hv, trap);
                break;

            case UD_Icpuid:
                cpuid_emulate(hv, trap);
                break;

            default:
                errx(1, "Unhandled mnemonic");
            }
            return 0;
        }
    }

    /* it wasn't an rdtsc */
    return -1;
}

static int setup(struct ukvm_hv *hv)
{
    int ret;

    if (rr_mode == RR_MODE_NONE)
        return 0;

    /*
     * We force trapping on instructions like rdtsc by changing them to int3's
     * and trap. And in order to change memory, we need to change the
     * permissions.
     */
    if (mprotect(hv->mem, hv->mem_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        err(1, "RR: Cannot remove guest memory protection");

    rr_init("rr_out.dat");

    ret = init_traps(hv);
    assert(ret == 0);
    /* for rdtsc traps, we need to handle int3s */
    ret = ukvm_core_register_vmexit(handle_vmexits);
    assert(ret == 0);
    /* XXX we really want a entry exit hook for rr */
    return 0;
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strncmp("--record", cmdarg, 8)) {
        rr_mode = RR_MODE_RECORD;
        return 0;
    } else if (!strncmp("--replay", cmdarg, 8)) {
        rr_mode = RR_MODE_REPLAY;
        return 0;
    } else {
        return -1;
    }
}

static char *usage(void)
{
    return "[--record][--replay]\n";
}

struct ukvm_module ukvm_module_rr = {
    .name = "rr",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
