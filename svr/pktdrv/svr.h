#if !defined (_SVR_H)
#  define _SVR_H

#  include "nettypes.h"

/* sizes for write- and read buffers */
#  define WR_BUF_SIZE 131072
#  define RD_BUF_SIZE 32768

#  if ((WR_BUF_SIZE % 512) != 0) || ((RD_BUF_SIZE % 512) != 0)
#    error Buffers should be multiple to HDD sector size (512 bytes).
#  endif
#  if WR_BUF_SIZE < 1024 || RD_BUF_SIZE < 1024
#    error Buffer sizes should be at least 1KB.
#  endif

/* however, only the first 3 bytes are used; 4th is seq_no */
const nt_dword_t svr_magic;

/* this one is 14 bytes so it will complement ethernet + ip + udp headers
 * to be multiple of 4 bytes */
typedef struct svr_packet
{
  nt_byte_t magic[3];
  nt_byte_t seq_no;
  nt_dword_t start;
  nt_dword_t result;
  nt_byte_t command;
  nt_byte_t count;
  unsigned char data[1];
} svr_packet; /* 14+ bytes */

#define SVR_TEST_MAGIC(p) (p[0] == svr_magic[0] && \
			   p[1] == svr_magic[1] && \
			   p[2] == svr_magic[2])

enum commands
  {
    cmd_stat = 1,
    cmd_read = 2,
    cmd_write = 3,
    cmd_sync = 4,
    cmd_shutdown = 5
  };

int svr_startup (void);
int svr_request (const udp_frame_t *uf);

#endif /* _SVR_H defined? */
