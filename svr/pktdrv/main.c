#include <errno.h>
#include <ioman.h>
#include <irx.h>
#include <loadcore.h>
#include <netman.h>
#include <stdio.h>
#include <sysclib.h>

#include "ipconfig.h"
#include "nic.h"
#include "eth.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "icmp.h"
#include "svr.h"

IRX_ID("PKTDRV_stack", 0x00, 0x80);

static unsigned char FrameBuffer[1600];
static struct NetManPacketBuffer RxPbuf;
static unsigned int PacketAvailable = 1;

static void LinkStateUp(void)
{
}

static void LinkStateDown(void)
{
}

static struct NetManPacketBuffer *AllocRxPacket(unsigned int size)
{
    struct NetManPacketBuffer *result;

    if (PacketAvailable) {
        result = &RxPbuf;
        result->payload = FrameBuffer;
        result->length = size;
        PacketAvailable = 0;
    } else
        result = NULL;

    return result;
}

static void FreeRxPacket(struct NetManPacketBuffer *packet)
{
    PacketAvailable = 1;
}

static int EnQRxPacket(struct NetManPacketBuffer *packet)
{
    return 0;
}

static int FlushInputQueue(void)
{
    if (RxPbuf.length > 0) {
        if (icmp_probe(FrameBuffer, RxPbuf.length))
            icmp_dispatch(FrameBuffer, RxPbuf.length);
        else if (arp_probe(FrameBuffer, RxPbuf.length))
            arp_dispatch(FrameBuffer, RxPbuf.length);
        else if (udp_probe(FrameBuffer, RxPbuf.length))
            udp_dispatch(FrameBuffer, RxPbuf.length);

        RxPbuf.length = 0;
    }

    PacketAvailable = 1;

    return 0;
}

static inline int InitPktDrv(const char *ip_address, const char *subnet_mask, const char *gateway);

int _start(int argc, char *argv[])
{
    unsigned int i;
    unsigned char ip_address[4], subnet_mask[4], gateway[4];
    static struct NetManNetProtStack stack = {
        &LinkStateUp,
        &LinkStateDown,
        &AllocRxPacket,
        &FreeRxPacket,
        &EnQRxPacket,
        &FlushInputQueue};

    for (i = 1; i < argc; i++) {
        //		printf("%u: %s\n", i, argv[i]);
        if (strncmp("-ip=", argv[i], 4) == 0) {
            ParseNetAddr(&argv[i][4], ip_address);
        } else if (strncmp("-netmask=", argv[i], 9) == 0) {
            ParseNetAddr(&argv[i][9], subnet_mask);
        } else if (strncmp("-gateway=", argv[i], 9) == 0) {
            ParseNetAddr(&argv[i][9], gateway);
        } else
            break;
    }

    InitPktDrv((const char *)ip_address, (const char *)subnet_mask, (const char *)gateway);
    NetManRegisterNetworkStack(&stack);

    return MODULE_RESIDENT_END;
}

static inline int InitPktDrv(const char *ip_address, const char *subnet_mask, const char *gateway)
{
    int retv;
    nt_ip_t my_ip;

    if (sizeof(eth_hdr_t) != 14)
        return (-1001);
    if (sizeof(arp_hdr_t) != 28)
        return (-1002);
    if (sizeof(ip_hdr_t) != 20)
        return (-1003);
    if (sizeof(icmp_echo_request_t) != 9) /* because of an extra char[1] */
        return (-1004);
    if (sizeof(arp_frame_t) != 42)
        return (-1005);
    if (sizeof(ping_frame_t) != 43) /* because of icmp_echo_request extra byte */
        return (-1006);
    if (sizeof(udp_hdr_t) != 9) /* extra char[1] */
        return (-1007);
    if (sizeof(udp_frame_t) != 43) /* extra char[1] in udp_hdr */
        return (-1008);

    eth_setup(nic_get_ethaddr());

    my_ip[0] = ip_address[0];
    my_ip[1] = ip_address[1];
    my_ip[2] = ip_address[2];
    my_ip[3] = ip_address[3];

    printf("pktdrv: IP is %u.%u.%u.%u\n",
           (unsigned char)my_ip[0], (unsigned char)my_ip[1], (unsigned char)my_ip[2], (unsigned char)my_ip[3]);

    ip_setup(my_ip);

    retv = svr_startup();
    if (retv != 0) {
        printf("svr_startup failed with %d.\n", retv);
        return (-2);
    }

    return (0);
}
