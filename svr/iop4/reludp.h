#if !defined (_RELUDP_H)
#  define _RELUDP_H

#  include "nettypes.h"
#  if !defined (_IOP)
#    include <stddef.h>
#  else
#    include <sysclib.h>
typedef u32 uint32_t;
#  endif

/* UDP payload is 1466 bytes, our header is 12 bytes */
#define RAW_PACKET_PAYLOAD (1466 - 12)

/* max message size of about 128+1KB, multiple to raw packet payload */
#define MAX_MESSAGE_CHUNKS \
  ((((128 + 1) * 1024) + RAW_PACKET_PAYLOAD - 1) / RAW_PACKET_PAYLOAD)
#define MAX_MESSAGE_SIZE \
  (MAX_MESSAGE_CHUNKS * RAW_PACKET_PAYLOAD)

#define _VEC_BASE_BITS2(type) (sizeof (type) * 8)
#define _VEC_BASE_BITS(v) (sizeof ((v)[0]) * 8)
/* no of elements required to declare `type' array with at least `bits' */
#define VEC_ARR_LEN(type,bits) \
  ((bits) + _VEC_BASE_BITS2 (type) - 1) / _VEC_BASE_BITS2 (type)
/* above length in bytes (multiple to size of `type') */
#define VEC_BYTE_LEN(type,bits) \
  VEC_ARR_LEN (type, bits) * sizeof (type)
/* declare a buffer to hold at least `bits' of `type' */
#define VEC_DECL(name,type,bits) \
  type name[VEC_ARR_LEN (type, bits)]
#define VEC_CLEAR(v) \
  memset ((v), 0, sizeof (v))

/* get/set bit in a vector */
#define VEC_GET_BIT(v,n) \
  (((v)[(n) / _VEC_BASE_BITS (v)] & (1 << ((n) % _VEC_BASE_BITS (v)))) ? 1 : 0)
#define VEC_RAISE_BIT(v,n) \
  (v)[(n) / _VEC_BASE_BITS (v)] |= (1 << ((n) % _VEC_BASE_BITS (v)))
#define VEC_CLEAR_BIT(v,n) \
  (v)[(n) / _VEC_BASE_BITS (v)] &= ~(1 << ((n) % _VEC_BASE_BITS (v)))

typedef struct peer_state_type
{
  /* remote peer info */
  nt_mac_t rpeer_mac;
  nt_ip_t rpeer_ip;
  nt_port_t rpeer_port;

  /* my info */
  nt_mac_t my_mac;
  nt_ip_t my_ip;
  nt_port_t my_port;

  /* somewhat shared */
  nt_byte_t *buf;
  size_t alloc_len;

  /* current message */
  uint32_t msg_id;
  size_t tot_len, tot_chunks;
  VEC_DECL (received_chunks_bitmap, nt_byte_t, MAX_MESSAGE_CHUNKS);
  size_t received_chunks_count;
  int processed;

  /* called to send an UDP-packet */
  int (*udp_send) (const nt_mac_t rpeer_mac,
		   const nt_ip_t rpeer_ip,
		   const nt_port_t rpeer_port,
		   const void *payload,
		   size_t payload_len);

  /* client callback */
  int (*message_received) (struct peer_state_type *ps,
			   const void *message,
			   size_t message_len);

  /* send */
  int (*send) (struct peer_state_type *ps,
	       const void *message,
	       size_t message_len);
} peer_state_t;


#endif /* _RELUDP_H defined? */
