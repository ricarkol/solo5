#ifndef __RR_RDTSC_HELPER__
#define __RR_RDTSC_HELPER__
static uint64_t rdtsc_locs[] = { 
    0x100200,
    0x10025c,
};
#define NUM_RDTSC_LOCS (sizeof(rdtsc_locs)/sizeof(uint64_t))
#endif
