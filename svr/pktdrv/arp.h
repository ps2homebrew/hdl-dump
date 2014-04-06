#if !defined (_ARP_H)
#  define _ARP_H

#  include <stddef.h>
#  include "nettypes.h"

int arp_probe(const void *frame, unsigned int frame_len);
int arp_dispatch (const void *frame,
		  size_t frame_len);

#endif /* _ARP_H defined? */
