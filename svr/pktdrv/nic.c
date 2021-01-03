//PS2 built-in network interface card packet driver (under AFL license)
#include "nic.h"
//#include "storage.h"
#include "stddef.h"

#include <sysclib.h>
#include <thbase.h>
#include <netman.h>

struct s_descriptor
{
    u16 flag;
    u16 zero;
    u16 size;
    u16 addr;
};

struct s_nic
{
    struct s_descriptor *rx_descriptors; /* [64] */
    struct s_descriptor *tx_descriptors; /* [64] */
    u8 ethaddr[6];                       // keep it aligned on 4 bytes boundary
    u16 rx_next_addr;
    u16 tx_head_addr;
    u16 tx_tail_addr;
    u8 rx_next_index;

    /* when tx_head_index != tx_tail_index queued data for send exists */
    u8 tx_head_index; /* [0,63] */
    u8 tx_tail_index; /* [0,63] */
    u8 running;
};

static struct s_nic g_nic;

size_t
nic_get_state(void *buf)
{
    memcpy(buf, &g_nic, sizeof(g_nic));
    return (sizeof(g_nic));
}

int nic_send(const void *pPacket, int size)
{
    // return(SMAPSendPacket(pPacket, size)!=0?1:0);
    return (NetManNetIFSendPacket(pPacket, size) != 0 ? 1 : 0);
}

int nic_send_wait(const void *pPacket, int size)
{
    while (nic_send(pPacket, size) == 0) {
    } //SMAPWaitTxEnd();

    return 1;
}

unsigned char *
nic_get_ethaddr(void)
{
    // SMAPGetMACAddress(g_nic.ethaddr);

    NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_MAC, NULL, 0, g_nic.ethaddr, sizeof(g_nic.ethaddr));

    return g_nic.ethaddr;
}
