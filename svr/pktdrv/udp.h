#if !defined (_UDP_H)
#  define _UDP_H

#  include <stddef.h>
#  include "nettypes.h"

int udp_probe(const void *frame, unsigned int frame_len);
int udp_dispatch (const void *frame, size_t frame_len);

#endif /* _UDP_H defined? */
