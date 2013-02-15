#include "eth.h"
#include <sysclib.h>


static nt_mac_t my_mac = { 0, 0, 0, 0, 0, 0 };


void
eth_setup (nt_mac_t mac)
{
  memcpy (my_mac, mac, sizeof (nt_mac_t));
}


void
eth_header (eth_hdr_t *dst,
	    const eth_hdr_t *in_reply_to,
	    unsigned short eth_type)
{
  COPY_NT_MAC (dst->dst_mac, in_reply_to->src_mac);
  COPY_NT_MAC (dst->src_mac, my_mac);
  SET_NT_WORD (dst->eth_type, eth_type);
}


const nt_byte_t*
eth_mac (void)
{
  return (my_mac);
}
