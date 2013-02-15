#include "nic.h"
#include <intrman.h>
#include <thbase.h>
#include <stdio.h>
#include <sysclib.h>
#include "eth.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "icmp.h"
#include "svr.h"


#define NO_COPY

static void
net_loop (void *arg)
{
#if defined (NO_COPY)
  unsigned char *buf = NULL;
#else
  unsigned char buf[1536] __attribute__((aligned (64)));
#endif
  do
    {
      unsigned char *frame;
      int frame_len;
      u32 old_interrupts;

      /* fetch next packet */
      CpuSuspendIntr ((void *) &old_interrupts);
      frame_len = nic_receive_wait (&frame);
#if defined (NO_COPY)
      /* NOTE: skipping memcpy here gives about 300-350kB/sec speed; 
       *       drawback is, that we risk nic.c to corrupt/overwrite currrent
       *       frame buffer before the following code is able to process it;
       *       this can be "fixed" with buffers queue, but currently
       *       "fix" is increased buffer space */
      buf = frame;
#else
      if (frame_len > 0)
	memcpy (buf, frame, frame_len);
#endif
      CpuResumeIntr (old_interrupts);

      if (frame_len > 0)
	{
	  if (udp_dispatch (buf, frame_len) == 0)
	    ;
	  else if (arp_dispatch (buf, frame_len) == 0)
	    ;
	  else if (icmp_dispatch (buf, frame_len) == 0)
	    ;
	  /* else don't handle */
	}
    }
  while (1);
}


int
_start (int argc, char *argv[])
{
  int retv;
  iop_thread_t thr;
  nt_ip_t my_ip = { 192, 168, 0, 10 };

  if (sizeof (eth_hdr_t) != 14)
    return (-1001);
  if (sizeof (arp_hdr_t) != 28)
    return (-1002);
  if (sizeof (ip_hdr_t) != 20)
    return (-1003);
  if (sizeof (icmp_echo_request_t) != 9) /* because of an extra char[1] */
    return (-1004);
  if (sizeof (arp_frame_t) != 42)
    return (-1005);
  if (sizeof (ping_frame_t) != 43) /* because of icmp_echo_request extra byte */
    return (-1006);
  if (sizeof (udp_hdr_t) != 9) /* extra char[1] */
    return (-1007);
  if (sizeof (udp_frame_t) != 43) /* extra char[1] in udp_hdr */
    return (-1008);

  if (argc > 1)
    { /* parse IP address */
      char *endp = NULL;
      nt_ip_t ip;
      long n;
      n = strtol (argv[1], &endp, 10);
      if (n > 0 && n < 256 && endp != NULL && *endp == '.')
	{
	  ip[0] = (unsigned char) n;
	  n = strtol (endp + 1, &endp, 10);
	  if (n >= 0 && n < 256 && endp != NULL && *endp == '.')
	    {
	      ip[1] = (unsigned char) n;
	      n = strtol (endp + 1, &endp, 10);
	      if (n >= 0 && n < 256 && endp != NULL && *endp == '.')
		{
		  ip[2] = (unsigned char) n;
		  n = strtol (endp + 1, &endp, 10);
		  if (n >= 0 && n < 256)
		    {
		      ip[3] = (unsigned char) n;
		      memcpy (my_ip, ip, sizeof (nt_ip_t));
		    }
		}
	    }
	}
    }

  retv = nic_init ();
  if (retv != 1)
    {
      printf ("nic_init failed with %d.\n", retv);
      return (-1);
    }

  printf ("pktdrv: IP is %d.%d.%d.%d\n",
	  (int) my_ip[0], (int) my_ip[1], (int) my_ip[2], (int) my_ip[3]);
  eth_setup (nic_get_ethaddr ());
  ip_setup (my_ip);

  retv = svr_startup ();
  if (retv != 0)
    {
      printf ("svr_startup failed with %d.\n", retv);
      return (-2);
    }

  memset (&thr, '\0', sizeof (thr));
  thr.attr = TH_C;
  thr.thread = &net_loop;
  thr.option = 0;
  thr.priority = 20; /* 40; */
  thr.stacksize = 4096 * 4;
  retv = CreateThread (&thr);
  if (retv < 0)
    {
      printf ("CreateThread failed with %d.\n", retv);
      return (-3);
    }
  StartThread (retv, NULL);

  return (0);
}
