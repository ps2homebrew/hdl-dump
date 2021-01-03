#if !defined(_IP_H)
#define _IP_H

#include <stddef.h>
#include "nettypes.h"

void ip_setup(nt_ip_t ip);

nt_byte_t *ip_checksum(const void *p, size_t len, nt_word_t checksum);

/* set-up IP header */
void ip_header(ip_hdr_t *ip,
               const ip_hdr_t *in_reply_to,
               nt_byte_t proto,
               size_t tot_len);

const nt_byte_t *ip_address(void);

#endif /* _IP_H defined? */
