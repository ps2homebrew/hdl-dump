#include "svr.h"
#include <atad.h>
#include <dev9.h>
#include <sysclib.h>
#include <thbase.h>
#include "eth.h"
#include "ip.h"
#include "nic.h"


/* total HDD size in sectors */
static unsigned long tot_sectors = 0;

/* write buffer */
static unsigned long wr_sector = (unsigned long) -1;
static unsigned char wr_buf[WR_BUF_SIZE] __attribute__ ((aligned (64)));
static const unsigned long wr_buf_size = WR_BUF_SIZE / 512;
static unsigned char wr_dirty[WR_BUF_SIZE / 512] = { 0 };
static int wr_dirty_count = 0;

/* read buffer */
static unsigned long rd_sector = (unsigned long) -1;
static unsigned char rd_buf[RD_BUF_SIZE] __attribute__ ((aligned (64)));
static const unsigned long rd_buf_size = RD_BUF_SIZE / 512;

/* keep next in sync with hio_udpnet2.c */
const nt_dword_t svr_magic = { 'A', 'o', 'E', '\0' };

static int svr_sync_buffer (void);


/* send a return code to the client */
static int
svr_return (const udp_frame_t *uf,
	    const svr_packet *req,
	    unsigned long result)
{
  const unsigned long payload_len = sizeof (svr_packet) - 1;
  const unsigned long tot_len = sizeof (udp_frame_t) - 1 + payload_len;
  unsigned char tmp[tot_len] __attribute__ ((aligned (4)));
  udp_frame_t *reply = (udp_frame_t*) tmp;

  eth_header (&reply->eth, &uf->eth, 0x0800);
  ip_header (&reply->ip, &uf->ip, 17 /* UDP */, tot_len - sizeof (eth_hdr_t));

  /* UDP */
  COPY_NT_WORD (reply->udp.src_port, uf->udp.dst_port);
  COPY_NT_WORD (reply->udp.dst_port, uf->udp.src_port);
  SET_NT_WORD (reply->udp.len, sizeof (udp_hdr_t) - 1 + payload_len);
  SET_NT_WORD (reply->udp.checksum, 0);
  memcpy (reply->udp.data, req, payload_len);
  SET_NT_DWORD (((svr_packet*) reply->udp.data)->result, result);

  nic_send_wait (reply, tot_len);
  return (0);
}


/* send data (the result of read operation) to the client */
static int
svr_send (const udp_frame_t *uf,
	  const svr_packet *req,
	  const void *payload,
	  unsigned long len)
{
  const unsigned long payload_len = sizeof (svr_packet) - 1 + len;
  const unsigned long tot_len = sizeof (udp_frame_t) - 1 + payload_len;
  unsigned char tmp[1536] __attribute__ ((aligned (4)));
  udp_frame_t *reply = (udp_frame_t*) tmp;

  eth_header (&reply->eth, &uf->eth, 0x0800);
  ip_header (&reply->ip, &uf->ip, 17 /* UDP */, tot_len - sizeof (eth_hdr_t));

  /* UDP */
  COPY_NT_WORD (reply->udp.src_port, uf->udp.dst_port);
  COPY_NT_WORD (reply->udp.dst_port, uf->udp.src_port);
  SET_NT_WORD (reply->udp.len, sizeof (udp_hdr_t) - 1 + payload_len);
  SET_NT_WORD (reply->udp.checksum, 0);

  /* UDP payload: */
  /* svr header: */
  memcpy (reply->udp.data, req, sizeof (svr_packet) - 1);
  SET_NT_DWORD (((svr_packet*) reply->udp.data)->result, 0);

  /* svr payload: */
  struct schain chain[2];
  chain[0].next = &chain[1];
  chain[0].data = tmp;
  chain[0].data_len = sizeof (udp_frame_t) - 1 + sizeof (svr_packet) - 1; // 56
  chain[1].next = NULL;
  chain[1].data = payload;
  chain[1].data_len = len;

  /* sizes for both chain entries are aligned to 4 bytes */
  nic_send_chain (chain);
  return (0);
}


static int
svr_read (const udp_frame_t *uf,
	  const svr_packet *req,
	  unsigned long start,
	  unsigned char count)
{
  if ((rd_sector % rd_buf_size) == 0 && /* buffer contains any data */
      start >= rd_sector &&
      start + count <= rd_sector + rd_buf_size)
    ; /* requested data is already inside read buffer */
  else
    { /* read requested data */
      if (wr_dirty_count)
	{ /* flush write buffer before read operation */
	  int result = svr_sync_buffer ();
	  if (result != 0)
	    return (svr_return (uf, req, result));
	}

      /* align starting sector to buffer size (round-down) */
      rd_sector = start - (start % rd_buf_size);
      if (start + count > rd_sector + rd_buf_size)
	rd_sector = start;
      int result = ata_device_dma_transfer (0, (char*) rd_buf,
					    rd_sector, rd_buf_size,
					    ATA_DIR_READ);
      if (result != 0)
	/* failed */
	return (svr_return (uf, req, result));
    }
  return (svr_send (uf, req, rd_buf + (start - rd_sector) * 512, count * 512));
}


/* flush write buffer; returns 0 on success */
static int
svr_sync_buffer (void)
{
  const char *start = NULL;
  int start_sect = 0, num_sect = 0, i;
  for (i = 0; i < wr_buf_size; ++i)
    if (wr_dirty[i])
      {
	if (start == NULL)
	  { /* begin run */
	    start = wr_buf + i * 512;
	    start_sect = wr_sector + i;
	    num_sect = 1;
	  }
	else
	  /* follow run */
	  ++num_sect;
      }
    else
      { /* end run? */
	if (start != NULL)
	  {
	    int result = ata_device_dma_transfer (0, (char*) start,
						  start_sect, num_sect,
						  ATA_DIR_WRITE);
	    if (result != 0)
	      return (result); /* failed */
	    start = NULL;
	  }
      }

  if (start != NULL)
    { /* last run */
      int result = ata_device_dma_transfer (0, (char*) start,
					    start_sect, num_sect,
					    ATA_DIR_WRITE);
      if (result != 0)
	return (result); /* failed */
    }

  memset (wr_dirty, 0, sizeof (wr_dirty));
  wr_dirty_count = 0;
  return (0);
}


static int
svr_write (const udp_frame_t *uf,
	   const svr_packet *req,
	   unsigned long start,
	   unsigned char count,
	   const void *buf)
{
  if ((start >= rd_sector &&
       start <= rd_sector + rd_buf_size) ||
      (start + count >= rd_sector &&
       start + count <= rd_sector + rd_buf_size))
    rd_sector = (unsigned long) -1; /* invalidate read cache */

  if ((wr_sector % wr_buf_size) == 0 && /* buffer contains any data */
      start >= wr_sector &&
      start + count <= wr_sector + wr_buf_size)
    ; /* data sent fits inside write buffer */
  else
    {
      if (wr_dirty_count > 0)
	{ /* sync write buffer */
	  int result = svr_sync_buffer ();
	  if (result != 0)
	    return (svr_return (uf, req, result));
	}

      /* re-align buffer */
      wr_sector = start - (start % wr_buf_size);
      if (start + count > wr_sector + wr_buf_size)
	wr_sector = start;
    }

  if (wr_dirty[start - wr_sector] == 0)
    { /* new chunk has been received */
      /* NOTE: the following memcpy can be skipped if nic.c is modified
       *       to put incoming data in the proper place in the buffer;
       *       the benefit is about 300-350kB/sec and it doesn't
       *       worth the trouble */
      memcpy (wr_buf + (start - wr_sector) * 512, buf, count * 512);
      wr_dirty[start - wr_sector] = 1;
      if (count == 2)
	wr_dirty[start - wr_sector + 1] = 1;
      wr_dirty_count += count;

      if (wr_dirty_count == wr_buf_size)
	/* buffer completely full: time to sync */
	return (svr_return (uf, req, svr_sync_buffer ()));
    }

  return (svr_return (uf, req, 0));
}


static int
svr_sync (const udp_frame_t *uf,
	  const svr_packet *req)
{
  return (svr_return (uf, req, svr_sync_buffer ()));
}


int
svr_request (const udp_frame_t *uf)
{
  /* BUG: write would not invalidate read buffer */
  const svr_packet *p = (const svr_packet*) uf->udp.data;
  switch (p->command)
    {
    case cmd_stat:
      return (svr_return (uf, p, tot_sectors));

    case cmd_read:
      return (svr_read (uf, p, GET_NT_DWORD (p->start), p->count));

    case cmd_write:
      return (svr_write (uf, p, GET_NT_DWORD (p->start), p->count, p->data));

    case cmd_sync:
      return (svr_sync (uf, p));

    case cmd_shutdown:
      {	/* dev9 shutdown; borrowed from ps2link */
	dev9IntrDisable (-1);
	dev9Shutdown ();
	*((unsigned char *) 0xbf402017) = 0x00;
	*((unsigned char *) 0xbf402016) = 0x0f;
      }
    }

  return (svr_return (uf, p, -1));
}


/* returns 0 on success */
int
svr_startup (void)
{
  ata_devinfo_t *dev_info = ata_get_devinfo (0);
  if (dev_info != NULL && dev_info->exists)
    {
      tot_sectors = dev_info->total_sectors;
      return (0);
    }
  return (1);
}
