#if !defined (_ETH_H)
#  define _ETH_H

#  include "nettypes.h"

void eth_setup (nt_mac_t mac);

void eth_header (eth_hdr_t *dst,
		 const eth_hdr_t *in_reply_to,
		 unsigned short eth_type);

const nt_byte_t* eth_mac (void);

#endif /* _ETH_H defined? */
