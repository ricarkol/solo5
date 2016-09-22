/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
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
#include "virtio_ring.h"
#include "virtio_pci.h"

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0     /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1 /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC (1 << 5) /* Host has given MAC address. */

#define PKT_BUFFER_LEN 1526

struct pkt_buffer {
    uint8_t data[PKT_BUFFER_LEN];
    uint32_t len;
};

struct pkt_buffers_queue {
    uint32_t num;
    struct pkt_buffer *bufs;
    uint32_t idx;
};

#define VIRTQ_MAX_DESCRIPTORS_PER_TX_CHAIN 2

/*
 * There is no official max queue size. But we've seen 4096, so let's use the
 * double of that.
 */
#define VIRTQ_NET_MAX_QUEUE_SIZE 8192

static struct pkt_buffer *xmit_bufs;
static struct pkt_buffer *recv_bufs;

static uint8_t *recv_data;
static uint8_t *xmit_data;

static struct virtq recvq;
static struct virtq xmitq;

#define VIRTQ_RECV 0
#define VIRTQ_XMIT 1

/* This header comes first in the scatter-gather list.
 * If VIRTIO_F_ANY_LAYOUT is not negotiated, it must
 * be the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct __attribute__((__packed__)) virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1   /* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID	2	/* Csum is valid */
    uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4	1	/* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP		3	/* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6	4	/* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN		0x80	/* TCP has ECN set */
    uint8_t gso_type;
    uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
    uint16_t csum_start;	/* Position to start checksumming from */
    uint16_t csum_offset;	/* Offset after that to place checksum */
};

static uint16_t virtio_net_pci_base; /* base in PCI config space */

static uint8_t virtio_net_mac[6];
static char virtio_net_mac_str[18];

static int net_configured;

static int handle_virtio_net_interrupt(void *);

/* WARNING: called in interrupt context */
static void virtq_handle_interrupt(struct virtq *vq)
{
    volatile struct virtq_used_elem *e;

    for (;;) {
        uint16_t desc_idx;

        if ((vq->used->idx % vq->num) == vq->last_used)
            break;

        e = &(vq->used->ring[vq->last_used % vq->num]);
        desc_idx = e->id;

	/* This will be non-zero for the receive case, and will be a no-op in
         * the transmit case. */
        ((struct pkt_buffer *)vq->desc[desc_idx].addr)->len = e->len;
        assert(e->len <= PKT_BUFFER_LEN);

        vq->num_avail++;
        while (vq->desc[desc_idx].flags & VIRTQ_DESC_F_NEXT) {
            vq->num_avail++;
            desc_idx = vq->desc[desc_idx].next;
        }

        vq->last_used = (vq->last_used + 1) % vq->num;
    }
}

/* WARNING: called in interrupt context */
int handle_virtio_net_interrupt(void *arg __attribute__((unused)))
{
    uint8_t isr_status;

    if (net_configured) {
        isr_status = inb(virtio_net_pci_base + VIRTIO_PCI_ISR);
        if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
            virtq_handle_interrupt(&xmitq);
            virtq_handle_interrupt(&recvq);
            return 1;
        }
    }
    return 0;
}

static void recv_setup(void)
{
    struct virtq_desc *desc;
    do {
        desc = &(recvq.desc[recvq.next_avail]);
        desc->addr = (uint64_t) recv_bufs[recvq.next_avail].data;
        desc->len = PKT_BUFFER_LEN;
        desc->flags = VIRTQ_DESC_F_WRITE;

        /* Memory barriers should be unnecessary with one processor */
        recvq.avail->ring[recvq.avail->idx % recvq.num] = recvq.next_avail;
        /* avail->idx always increments, and wraps naturally at 65536 */
        recvq.avail->idx++;
        recvq.next_avail = (recvq.next_avail + 1) % recvq.num;
    } while (recvq.next_avail != 0);

    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_RECV);
}

/* performance note: we perform a copy into the xmit buffer */
int virtio_net_xmit_packet(void *data, int len)
{
    struct virtq_desc *desc;
    uint16_t head, next;
    uint32_t used_descs = 2;
    int dbg = 1;

    if (xmitq.num_avail < used_descs) {
        printf("xmit buffer full! next_avail:%d last_used:%d\n",
               xmitq.next_avail, xmitq.last_used);
            return -1;
    }

    head = xmitq.next_avail;

    desc = &(xmitq.desc[head]);
    desc->addr = (uint64_t) xmit_bufs[head].data;
    desc->len = sizeof(struct virtio_net_hdr);
    memset(xmit_bufs[head].data, 0, desc->len);
    next = (head + 1) % xmitq.num;
    desc->next = next;
    desc->flags = VIRTQ_DESC_F_NEXT;

    assert(len <= PKT_BUFFER_LEN);
    desc = &(xmitq.desc[next]);
    desc->addr = (uint64_t) xmit_bufs[next].data;
    desc->len = len;
    memcpy(xmit_bufs[next].data, data, len);
    desc->flags = 0;
    desc->next = 0;

    if (dbg)
        atomic_printf("XMIT: 0x%p next_avail %d last_used %d\n",
                      desc->addr, xmitq.next_avail, (xmitq.last_used*2) % xmitq.num);

    xmitq.num_avail -= used_descs;
    /* Memory barriers should be unnecessary with one processor */
    xmitq.avail->ring[xmitq.avail->idx % xmitq.num] = head;
    /* avail->idx always increments (once per chain), and wraps naturally at
     * 65536 */
    xmitq.avail->idx++;
    xmitq.next_avail = (xmitq.next_avail + used_descs) % xmitq.num;
    outw(virtio_net_pci_base + VIRTIO_PCI_QUEUE_NOTIFY, VIRTQ_XMIT);

    return 0;
}


void virtio_config_network(uint16_t base, unsigned irq)
{
    uint8_t ready_for_init = VIRTIO_PCI_STATUS_ACK | VIRTIO_PCI_STATUS_DRIVER;
    uint32_t host_features, guest_features;
    int i;
    int dbg = 0;

    outb(base + VIRTIO_PCI_STATUS, ready_for_init);

    host_features = inl(base + VIRTIO_PCI_HOST_FEATURES);

    if (dbg) {
        uint32_t hf = host_features;

        printf("host features: %x: ", hf);
        for (i = 0; i < 32; i++) {
            if (hf & 0x1)
                printf("%d ", i);
            hf = hf >> 1;
        }
        printf("\n");
    }

    assert(host_features & VIRTIO_NET_F_MAC);

    /* only negotiate that the mac was set for now */
    guest_features = VIRTIO_NET_F_MAC;
    outl(base + VIRTIO_PCI_GUEST_FEATURES, guest_features);

    printf("Found virtio network device with MAC: ");
    for (i = 0; i < 6; i++) {
        virtio_net_mac[i] = inb(base + VIRTIO_PCI_CONFIG_OFF + i);
        printf("%02x ", virtio_net_mac[i]);
    }
    printf("\n");
    snprintf(virtio_net_mac_str,
             sizeof(virtio_net_mac_str),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             virtio_net_mac[0],
             virtio_net_mac[1],
             virtio_net_mac[2],
             virtio_net_mac[3],
             virtio_net_mac[4],
             virtio_net_mac[5]);

    /* get the size of the virt queues */
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_RECV);
    recvq.num = recvq.num_avail = inw(base + VIRTIO_PCI_QUEUE_SIZE);
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_XMIT);
    xmitq.num = xmitq.num_avail = inw(base + VIRTIO_PCI_QUEUE_SIZE);
    assert(recvq.num <= VIRTQ_NET_MAX_QUEUE_SIZE);
    assert(xmitq.num <= VIRTQ_NET_MAX_QUEUE_SIZE);
    printf("net queue size is %d/%d\n", recvq.num, xmitq.num);

    recv_data = memalign(4096, VIRTQ_SIZE(recvq.num));
    assert(recv_data);
    memset(recv_data, 0, VIRTQ_SIZE(recvq.num));
    recv_bufs = calloc(recvq.num, sizeof (struct pkt_buffer));
    assert(recv_bufs);

    recvq.desc = (struct virtq_desc *)(recv_data + VIRTQ_OFF_DESC(recvq.num));
    recvq.avail = (struct virtq_avail *)(recv_data + VIRTQ_OFF_AVAIL(recvq.num));
    recvq.used = (struct virtq_used *)(recv_data + VIRTQ_OFF_USED(recvq.num));

    xmit_data = memalign(4096, VIRTQ_SIZE(xmitq.num));
    assert(xmit_data);
    memset(xmit_data, 0, VIRTQ_SIZE(xmitq.num));
    xmit_bufs = calloc(xmitq.num, sizeof (struct pkt_buffer));
    assert(xmit_bufs);

    xmitq.desc = (struct virtq_desc *)(xmit_data + VIRTQ_OFF_DESC(xmitq.num));
    xmitq.avail = (struct virtq_avail *)(xmit_data + VIRTQ_OFF_AVAIL(xmitq.num));
    xmitq.used = (struct virtq_used *)(xmit_data + VIRTQ_OFF_USED(xmitq.num));

    virtio_net_pci_base = base;
    net_configured = 1;
    intr_register_irq(irq, handle_virtio_net_interrupt, NULL);
    outb(base + VIRTIO_PCI_STATUS, VIRTIO_PCI_STATUS_DRIVER_OK);

    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_RECV);
    outl(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) recv_data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
    outw(base + VIRTIO_PCI_QUEUE_SEL, VIRTQ_XMIT);
    outl(base + VIRTIO_PCI_QUEUE_PFN, (uint64_t) xmit_data
         >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

    recv_setup();
}

int virtio_net_pkt_poll(void)
{
    if (!net_configured)
        return 0;

    if (recvq.next_avail == (recvq.last_used % recvq.num))
        return 0;
    else
        return 1;
}

uint8_t *virtio_net_pkt_get(int *size)
{
    struct pkt_buffer *buf;

    if (recvq.next_avail == (recvq.last_used % recvq.num))
        return NULL;

    buf = &recv_bufs[recvq.next_avail];

    /* Remove the virtio_net_hdr */
    *size = buf->len - sizeof(struct virtio_net_hdr);
    return buf->data + sizeof(struct virtio_net_hdr);
}

static void recv_load_desc(void)
{
    struct virtq_desc *desc;

    if (recvq.num_avail < 1) {
        printf("recv buffer full! next_avail:%d last_used:%d\n",
               recvq.next_avail, recvq.last_used);
            return;
    }

    desc = &(recvq.desc[recvq.next_avail]);
    desc->addr = (uint64_t) recv_bufs[recvq.next_avail].data;
    desc->len = PKT_BUFFER_LEN;
    desc->flags = VIRTQ_DESC_F_WRITE;
    /* Memory barriers should be unnecessary with one processor */
    recvq.avail->ring[recvq.avail->idx % recvq.num] = recvq.next_avail;
    /* avail->idx always increments, and wraps naturally at 65536 */
    recvq.avail->idx++;
    recvq.next_avail = (recvq.next_avail + 1) % recvq.num;
}

void virtio_net_pkt_put(void)
{
    recv_load_desc();
}

int solo5_net_write_sync(uint8_t *data, int n)
{
    assert(net_configured);

    return virtio_net_xmit_packet(data, n);
}

int solo5_net_read_sync(uint8_t *data, int *n)
{
    uint8_t *pkt;
    int len = *n;

    assert(net_configured);

    pkt = virtio_net_pkt_get(&len);
    if (!pkt)
        return -1;

    assert(len <= *n);
    assert(len <= PKT_BUFFER_LEN);
    *n = len;

    /* also, it's clearly not zero copy */
    memcpy(data, pkt, len);

    virtio_net_pkt_put();

    return 0;
}

char *solo5_net_mac_str(void)
{
    assert(net_configured);

    return virtio_net_mac_str;
}
