#include "reludp.h"
#if !defined (_IOP)
#  include <string.h>
#else
#  include <sysmem.h>
#  include <sysclib.h>

#  define malloc(n) AllocSysMemory (ALLOC_FIRST, n, NULL)
#  define free(p) FreeSysMemory (p)
#endif



typedef struct reludp_packet_type
{
  nt_dword_t magic;
  nt_word_t msg_id;
  nt_word_t chunk_no;
  nt_dword_t tot_len;
  /* unsigned char payload[] */
} reludp_packet_t;

typedef struct reludp_ack_type
{
  nt_dword_t msg_id;
  nt_dword_t tot_len;
  nt_word_t tot_chunks, have_chunks;
  VEC_DECL (bitmap, nt_byte_t, MAX_MESSAGE_CHUNKS);
} reludp_ack_t;




void
ps_init (peer_state_t *ps)
{
  memset (ps, 0, sizeof (peer_state_t));
  ps->processed = 1;
}


int
ps_process (peer_state_t *ps,
	    const nt_mac_t rpeer_mac,
	    const nt_ip_t rpeer_ip,
	    const nt_port_t rpeer_port,
	    const nt_byte_t *packet, /* udp payload, not raw */
	    size_t packet_len)
{
  const reludp_packet_t *up = (const reludp_packet_t*) packet;
  const uint32_t magic = GET_NT_DWORD (up->magic);
  const uint32_t msg_id = GET_NT_WORD (up->msg_id);
  const size_t chunk_no = GET_NT_WORD (up->chunk_no);
  const size_t tot_len = GET_NT_DWORD (up->tot_len);

  if (magic != 0x12345678)
    return (-1); /* not for us */

  if (tot_len > MAX_MESSAGE_SIZE)
    return (-1); /* message size too big */

  if (GET_NT_DWORD (ps->rpeer_ip) == 0 &&
      GET_NT_WORD (ps->rpeer_port) == 0)
    { /* new remote peer, first packet */
      COPY_NT_MAC (ps->rpeer_mac, rpeer_mac);
      COPY_NT_IP (ps->rpeer_ip, rpeer_ip);
      COPY_NT_WORD (ps->rpeer_port, rpeer_port);
    }
  else
    { /* handle connections from current remote peer only */
      if (!NT_IP_EQ (ps->rpeer_ip, rpeer_ip) ||
	  !NT_WORD_EQ (ps->rpeer_port, rpeer_port))
	return (-1); /* got peer already */
    }

  if (ps->alloc_len < tot_len)
    { /* (re)alloc buffer */
      if (ps->buf != NULL)
	{
	  free (ps->buf);
	  ps->buf = NULL;
	  ps->alloc_len = 0;
	}
      ps->buf = (unsigned char*) malloc (tot_len);
      if (ps->buf != NULL)
	ps->alloc_len = tot_len;
      else
	return (-1); /* out of memory */
    }

  if (ps->msg_id != msg_id)
    { /* new message */
      ps->msg_id = msg_id;
      ps->tot_len = tot_len;
      ps->tot_chunks = (tot_len + RAW_PACKET_PAYLOAD - 1) / RAW_PACKET_PAYLOAD;

      memset (ps->received_chunks_bitmap, 0,
	      sizeof (ps->received_chunks_bitmap));
      ps->received_chunks_count = 0;
      ps->processed = 0;
    }

  if (chunk_no > ps->tot_chunks)
    return (-1); /* invalid chunk */

  if (!VEC_GET_BIT (ps->received_chunks_bitmap, chunk_no))
    { /* new chunk received */
      size_t chunk_size = RAW_PACKET_PAYLOAD;
      if (chunk_no == ps->tot_chunks - 1)
	/* last packet could be smaller */
	chunk_size = ((ps->tot_len - 1) % RAW_PACKET_PAYLOAD) + 1;
      memcpy (ps->buf + RAW_PACKET_PAYLOAD * chunk_no,
	      packet + sizeof (reludp_packet_t), chunk_size);
      VEC_RAISE_BIT (ps->received_chunks_bitmap, chunk_no);
      ++ps->received_chunks_count;
    }

  /* prepare ACKnowledge */
  reludp_ack_t ack;
  SET_NT_DWORD (ack.msg_id, 0x80000000 | ps->msg_id);
  SET_NT_DWORD (ack.tot_len, ps->tot_len);
  SET_NT_WORD (ack.tot_chunks, ps->tot_chunks);
  SET_NT_WORD (ack.have_chunks, ps->received_chunks_count);
  memcpy (ack.bitmap, ps->received_chunks_bitmap,
	  sizeof (ps->received_chunks_bitmap));
  /* TODO: schedule ACK packet */
}


/* would not return until whole message is sent or a timeout expires */
int
ps_send_message_loop (peer_state_t *ps)
{
  do
    { /* send all un-acknowledged chunks */
      size_t i;
      for (i = 0; i < ps->tot_chunks; ++i)
	if (!VEC_GET_BIT (ps->received_chunks_bitmap, i))
	  {
	  }
    }
  while (1);
}
