#include "udp.h"
#include <sysclib.h>
#include <thbase.h>
#include "eth.h"
#include "ip.h"
#include "nic.h"
#include "svr.h"


static int
udp_packet (const udp_frame_t *uf)
{
  static const size_t MAX_UDP_DATA_LEN = 1500 - sizeof (ip_hdr_t);
  const nt_byte_t *my_ip = ip_address ();
  if (NT_IP_EQ (uf->ip.dst_ip, my_ip) &&
      GET_NT_WORD (uf->udp.len) <= MAX_UDP_DATA_LEN)
    {
      if (SVR_TEST_MAGIC (uf->udp.data))
	return (svr_request (uf));
    }
  return (1);
}


int
udp_dispatch (const void *frame, size_t frame_len)
{
  if (frame_len < sizeof (udp_frame_t))
    return (1);

  const udp_frame_t *uf = (const udp_frame_t*) frame;
  if (IS_NT_WORD (uf->eth.eth_type, 0x0800) &&
      uf->ip.version_and_hdr_len == ((4 << 4) | (5 << 0)) &&
      uf->ip.diff_serv == 0 &&
      uf->ip.proto == 17 /* UDP */ &&
      (GET_NT_WORD (uf->ip.flags_frag_offs) == 0x0000 ||
       GET_NT_WORD (uf->ip.flags_frag_offs) == 0x4000 /* don't fragment */))
    {
      return (udp_packet (uf));
    }
  return (1);
}
