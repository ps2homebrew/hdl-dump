#include "arp.h"
#include <sysclib.h>
#include <thbase.h>
#include "eth.h"
#include "ip.h"
#include "nic.h"


static int
arp_request (const arp_frame_t *af)
{
  const nt_byte_t *my_ip = ip_address ();
  if (NT_IP_EQ (af->arp.dst_ip, my_ip))
    { /* schedule ARP reply */
      arp_frame_t tmp __attribute__ ((aligned (4)));

      arp_frame_t *reply = &tmp;
      memset (reply, 0, sizeof (arp_frame_t));
      eth_header (&reply->eth, &af->eth, 0x0806);

      /* arp */
      SET_NT_WORD (reply->arp.hardware_type, 0x0001);
      SET_NT_WORD (reply->arp.proto, 0x0800);
      reply->arp.hardware_len = 6;
      reply->arp.proto_len = 4;
      SET_NT_WORD (reply->arp.oper, 0x0002); /* ARP reply */
      COPY_NT_MAC (reply->arp.src_mac, eth_mac ());
      COPY_NT_IP (reply->arp.src_ip, my_ip);
      COPY_NT_MAC (reply->arp.dst_mac, af->arp.src_mac);
      COPY_NT_IP (reply->arp.dst_ip, af->arp.src_ip);

      while (!nic_send (reply, sizeof (arp_frame_t)))
	DelayThread (10 /* usec */);
      return (0);
    }
  return (1);
}


int
arp_dispatch (const void *frame, size_t frame_len)
{
  if (frame_len < sizeof (arp_frame_t))
    return (1);

  const arp_frame_t *af = (const arp_frame_t*) frame;
  if (IS_NT_WORD (af->eth.eth_type, 0x0806) &&
      IS_NT_WORD (af->arp.hardware_type, 0x0001) &&
      IS_NT_WORD (af->arp.proto, 0x0800) &&
      af->arp.hardware_len == 6 &&
      af->arp.proto_len == 4)
    {
      switch (GET_NT_WORD (af->arp.oper))
	{
	case 0x0001:
	  return (arp_request (af));
	}
    }
  return (1);
}
