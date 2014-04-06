#include "icmp.h"
#include <sysclib.h>
#include "eth.h"
#include "ip.h"
#include "nic.h"


static int
icmp_echo_request (const ping_frame_t *pf)
{
  const nt_byte_t *my_ip = ip_address ();
  if (NT_IP_EQ (pf->ip.dst_ip, my_ip))
    {
      const size_t ip_tot_len = GET_NT_WORD (pf->ip.tot_len);
      const size_t payload_len = ip_tot_len - 20 - 8; /* ip & icmp headers */
      unsigned char tmp[1536] __attribute__ ((aligned (64)));
      nt_word_t cs;

      pong_frame_t *pong = (pong_frame_t*) tmp;
      memset (pong, 0, sizeof (pong_frame_t));
      eth_header (&pong->eth, &pf->eth, 0x0800);

      /* IPv4 */
      ip_header (&pong->ip, &pf->ip, 0x01 /* ICMP */,
		 GET_NT_WORD (pf->ip.tot_len));

      /* ICMP */
      pong->echo.type = 0x00; /* echo reply */
      pong->echo.code = 0x00;
      SET_NT_WORD (pong->echo.hdr_checksum, 0); /* below */
      COPY_NT_WORD (pong->echo.id, pf->echo.id);
      COPY_NT_WORD (pong->echo.seq_no, pf->echo.seq_no);
      memcpy (pong->echo.data, pf->echo.data, payload_len);
      SET_NT_WORD (cs, 0);
      COPY_NT_WORD (pong->echo.hdr_checksum,
		    ip_checksum (&pong->echo, 8 + payload_len, cs));

      nic_send_wait(pong, sizeof (eth_hdr_t) + ip_tot_len);

      return (0);
    }
  return (1);
}

int icmp_probe(const void *frame, unsigned int frame_len){
	int result;

	result=0;
	if(frame_len>=sizeof (ping_frame_t)){
		const ping_frame_t *pf = (const ping_frame_t*) frame;
		if (IS_NT_WORD (pf->eth.eth_type, 0x0800) &&
		pf->ip.version_and_hdr_len == ((4 << 4) | (5 << 0)) &&
		pf->ip.diff_serv == 0 &&
		pf->ip.proto == 0x01 /* ICMP */)
		{
			switch (pf->echo.type){
				case 0x08:
					result=1;
			}
		}
	}

	return result;
}

int icmp_dispatch(const void *frame, size_t frame_len)
{
	int result;

	if(icmp_probe(frame, frame_len)){
		result=icmp_echo_request((const ping_frame_t*)frame);
	}
	else result=1;

	return result;
}
