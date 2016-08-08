#include "kernel.h"

/* ukvm net interface */
int solo5_net_write_sync(__attribute__((__unused__)) solo5_device *dev, __attribute__((__unused__)) uint64_t off, uint8_t *data, int n)
{
    volatile struct ukvm_netwrite wr;

    wr.data = data;
    wr.len = n;
    wr.ret = 0;

    // TODO: get the port associated with device *dev

    outl(UKVM_PORT_NETWRITE, ukvm_ptr(&wr));
    cc_barrier();

    return wr.ret;
}

int solo5_net_read_sync(__attribute__((__unused__)) solo5_device *dev, __attribute__((__unused__)) uint64_t off, uint8_t *data, int *n)
{
    volatile struct ukvm_netread rd;

    rd.data = data;
    rd.len = *n;
    rd.ret = 0;

    outl(UKVM_PORT_NETREAD, ukvm_ptr(&rd));
    cc_barrier();

    *n = rd.len;

    return rd.ret;
}

static char mac_str[18];
char *solo5_net_mac_str(__attribute__((__unused__)) solo5_device *dev)
{
    volatile struct ukvm_netinfo info;

    outl(UKVM_PORT_NETINFO, ukvm_ptr(&info));
    cc_barrier();

    memcpy(mac_str, (void *)&info, 18);
    return mac_str;
}

/* ukvm block interface */
solo5_request solo5_blk_write_async(__attribute__((__unused__)) solo5_device *dev, uint64_t sec, uint8_t *data, int n)
{
    solo5_request solo5_req;
    solo5_req._req = malloc(sizeof(struct ukvm_blkread));
    struct ukvm_blkwrite *wr = (struct ukvm_blkwrite *) solo5_req._req;

    wr->sector = sec;
    wr->data = data;
    wr->len = n;
    wr->ret = 0;

    // TODO: get the port associated with device *dev

    outl(UKVM_PORT_BLKWRITE, ukvm_ptr(wr));
    cc_barrier();

    return solo5_req;
}
int solo5_blk_write_sync(solo5_device *dev, uint64_t sec, uint8_t *data, int n)
{
    solo5_request solo5_req;
    short events[SOLO5_NUM_DEVICES];

    solo5_req = solo5_blk_write_async(dev, sec, data, n);

    memset(events, 0, SOLO5_NUM_DEVICES * sizeof(events));
    events[dev->poll_event_idx] = SOLO5_POLL_IO_READY;
    solo5_poll(solo5_clock_monotonic() + 1e9, events, NULL);

    // FIXME: check for errors or sometihng
    solo5_req = solo5_req;

    return 0;
}


solo5_request solo5_blk_read_async_submit(__attribute__((__unused__)) solo5_device *dev, uint64_t sec, int *n)
{
    solo5_request solo5_req;
    solo5_req._req = malloc(sizeof(struct ukvm_blkread));
    struct ukvm_blkread *rd = (struct ukvm_blkread *) solo5_req._req;

    rd->sector = sec;
    rd->data = malloc(*n);
    rd->len = *n;
    rd->ret = 0;

    // TODO: get the port associated with device *dev

    outl(UKVM_PORT_BLKREAD, ukvm_ptr(rd));
    cc_barrier();

    return solo5_req;
}

int solo5_blk_read_async_complete(__attribute__((__unused__)) solo5_device *dev, solo5_request solo5_req, uint8_t *data, int *n)
{
    struct ukvm_blkread *req = (struct ukvm_blkread *) solo5_req._req;
    int ret;

    *n = req->len;
    memcpy(data, req->data, *n);

    ret = req->ret;
    free(req);
    return ret;
}

int solo5_blk_write_async_complete(__attribute__((__unused__)) solo5_device *dev, solo5_request solo5_req, int *n)
{
    struct ukvm_blkread *req = (struct ukvm_blkread *) solo5_req._req;
    int ret;

    *n = req->len;

    ret = req->ret;
    free(req);
    return ret;
}


int solo5_blk_read_sync(solo5_device *dev, uint64_t sec, uint8_t *data, int *n)
{
    solo5_request solo5_req;

    short events[SOLO5_NUM_DEVICES];
    short revents[SOLO5_NUM_DEVICES];

    solo5_req = solo5_blk_read_async_submit(dev, sec, n);

    memset(events, 0, SOLO5_NUM_DEVICES * sizeof(events));
    events[dev->poll_event_idx] = SOLO5_POLL_IO_READY;
    solo5_poll(solo5_clock_monotonic() + 1e9, events, revents);

    // FIXME: remove the printf
    printf("revents: %d %d\n", events[0], events[1]);

    assert(events[dev->poll_event_idx] == SOLO5_POLL_IO_READY);
    return solo5_blk_read_async_complete(dev, solo5_req, data, n);
}

int solo5_blk_sector_size(__attribute__((__unused__)) solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    // TODO: get the port associated with device *dev

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.sector_size;
}

uint64_t solo5_blk_sectors(__attribute__((__unused__)) solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    // TODO: get the port associated with device *dev

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.num_sectors;
}

int solo5_blk_rw(__attribute__((__unused__)) solo5_device *dev)
{
    volatile struct ukvm_blkinfo info;

    outl(UKVM_PORT_BLKINFO, ukvm_ptr(&info));
    cc_barrier();

    return info.rw;
}
