//PS2 built-in network interface card packet driver (under AFL license)
#include "irx_imports.h"
#include "nic.h"
#include "storage.h"
#include "stddef.h"

#define BASE 0xB0000000

//"volatile" will prevent bad optimizations from compiler
//(it may assume it's useless to read same memory address in a loop!)
#define	REGB(Offset)	(*(u8 volatile*)(BASE+(Offset)))
#define	REGW(Offset)	(*(u16 volatile*)(BASE+(Offset)))
#define	REG(Offset)	(*(u32 volatile*)(BASE+(Offset)))

struct s_descriptor
{
  u16 flag;
  u16 zero;
  u16 size;
  u16 addr;
};

struct s_nic
{
  struct s_descriptor *rx_descriptors; /* [64] */
  struct s_descriptor *tx_descriptors; /* [64] */
  u8 ethaddr[6];		// keep it aligned on 4 bytes boundary
  u16 rx_next_addr;
  u16 tx_head_addr;
  u16 tx_tail_addr;
  u8 rx_next_index;

  /* when tx_head_index != tx_tail_index queued data for send exists */
  u8 tx_head_index; /* [0,63] */
  u8 tx_tail_index; /* [0,63] */
  u8 running;
};

static struct s_nic g_nic;
static int got_data_sema = 0;
static int can_send_sema = 0;

size_t
nic_get_state (void *buf)
{
  memcpy (buf, &g_nic, sizeof (g_nic));
  return (sizeof (g_nic));
}

/* executed in interrupt handler context only */
static void
nic_tx_interrupt (void)
{
  while (g_nic.tx_head_index != g_nic.tx_tail_index)
    {
      volatile struct s_descriptor *d =
	&g_nic.tx_descriptors[g_nic.tx_head_index];
      if (d->flag & 0x8000)
	return;
      g_nic.tx_head_addr = (g_nic.tx_head_addr + ((d->size + 3) & ~3)) % 4096;
      g_nic.tx_head_index = (g_nic.tx_head_index + 1) % 64;
      d->size = 0;
      d->addr = 0;
      d->flag = 0;
    }
  g_nic.tx_head_addr = g_nic.tx_tail_addr;
  (void) iSignalSema (can_send_sema);
}

/* executed in interrupt handler context only */
static void
nic_rx_interrupt (void)
{
  /* this is where data is dumped when all buffers are full */
  static unsigned char dump_bin[MAX_PACKET_SIZE]
    __attribute__ ((aligned (64)));

  while (1)
    {
      volatile struct s_descriptor *d =
	&g_nic.rx_descriptors[g_nic.rx_next_index];
      if (d->flag & 0x8000)
	return;

      // Did any errors occur and is the packet-size in the valid range?
      if (((d->flag & 0x03FF) == 0) && (d->size <= 1518) && (d->size >= 14))
	{
	  int i;
	  u32 *p;
	  int aligned_size = (d->size + 3) & ~3;
	  g_nic.rx_next_addr = ((d->addr - 0x4000) % 16384) & ~3;
	  if (storage_counter < STORAGE_MAX)
	    { /* store data in the buffer */
	      /* TODO: if incoming data is UDP frame, 1038 bytes payload
	       *       put it where it belongs to skip memcpy, later;
	       *       2 buffers are neccesary, as sync cannot be done
	       *       from within interrupt handler */
	      storage_counter++;
	      p = (u32 *) &storage[storage_next_in * MAX_PACKET_SIZE];
	      REGW (0x1034) = g_nic.rx_next_addr;
	      for (i = 0; i < aligned_size; i += 4)
		*(p++) = REG (0x1200);
	      storage_len[storage_next_in] = d->size;
	      storage_next_in = (storage_next_in + 1) % STORAGE_MAX;
	      (void) iSignalSema (got_data_sema);
	    }
	  else
	    { /* no available buffers found; drop data on the floor */
	      p = (u32 *) dump_bin;
	      REGW (0x1034) = g_nic.rx_next_addr;
	      for (i = 0; i < aligned_size; i += 4)
		*(p++) = REG (0x1200);
	    }
	}
      REGB (0x1040) = 0x01;
      d->flag = 0x8000;
      g_nic.rx_next_index = (g_nic.rx_next_index + 1) % 64;
    }
}


static void
nic_start (void)
{
  if (g_nic.running)
    return;
  REGW (0x128) = 0x7C;
  REGW (0x2014) = 0x03FF;
  REGW (0x2016) = 0x03FB;
  REGW (0x2000) = 0x1800;
  dev9IntrEnable (0x7C);
  g_nic.running = 1;
}


static void
nic_stop (void)
{
  REGW (0x2000) = REGW (0x2000) & 0xE7FF;
  dev9IntrDisable (0x7C);
  REGW (0x128) = 0x7C;
  REGW (0x2014) = 0x03FF;
  REGW (0x2016) = 0x03FB;
  g_nic.running = 0;
}


/* executed in the caller context */
int
nic_receive (unsigned char **ppPacket)
{
  int n;

  //return nbr of bytes (if packet received)
  n = 0;
  if (storage_counter)
    {
      n = storage_len[storage_next_out];
      if (ppPacket)
	{
	  storage_counter--;
	  *ppPacket = &storage[storage_next_out * MAX_PACKET_SIZE];
	  storage_next_out = (storage_next_out + 1) % STORAGE_MAX;
	}
    }
  return n;
}


/* executed in the caller context */
int
nic_receive_wait (unsigned char **ppPacket)
{
  int n = 0;

  /* return nbr of bytes */
  do
    {
      n = nic_receive (ppPacket);
      if (n == 0)
	WaitSema (got_data_sema);
    }
  while (n == 0);
  return (n);
}


static int
nic_can_send_bytes (int size)
{
  int available_space;
  int aligned_size = (size + 3) & ~3;
  int i;

  if (g_nic.running == 0)
    return 0;
  if (size > 1514)
    return 0;
  if (g_nic.tx_head_addr > g_nic.tx_tail_addr)
    available_space = 4096 + (g_nic.tx_tail_addr - g_nic.tx_head_addr);
  else
    available_space = 4096 - (g_nic.tx_tail_addr - g_nic.tx_head_addr);
  if (aligned_size > available_space)
    return 0;
  for (i = 0; i < 256; i++)
    if ((REGW (0x2008) & 0x8000) == 0)
      break;
  if (i == 256)
    return 0;
  return (1);
}


int
nic_send (const void *pPacket, int size)
{
  int aligned_size = (size + 3) & ~3;
  if (nic_can_send_bytes (aligned_size))
    {
      volatile struct s_descriptor *d =
	&g_nic.tx_descriptors[g_nic.tx_tail_index];
      int i;
      const u32 *p = (u32 *) pPacket;

      REGW (0x1004) = g_nic.tx_tail_addr;
      for (i = 0; i < aligned_size; i += 4)
	REG (0x1100) = *(p++);
      d->size = size;
      d->addr = 0x1000 + g_nic.tx_tail_addr;
      REGB (0x1010) = 0x01;
      d->flag = 0x8300;
      REG (0x2008) = 0x8000;	//Yes! REG, not REGW!
      g_nic.tx_tail_addr = (g_nic.tx_tail_addr + aligned_size) % 4096;
      g_nic.tx_tail_index = (g_nic.tx_tail_index + 1) % 64;
      return (1);
    }
  else
    return (0);
}


int
nic_send_wait (const void *pPacket, int size)
{
  int n = 0;

  do
    {
      n = nic_send (pPacket, size);
      if (n == 0)
	WaitSema (can_send_sema);
    }
  while (n == 0);
  return (n);
}


int
nic_send_chain (const struct schain *head)
{
  /* calc total size */
  const struct schain *e;
  size_t tot_size = 0;
  for (e = head; e; e = e->next)
    if ((e->data_len % 4) == 0)
      tot_size += e->data_len;
    else
      return (-1);

  while (!nic_can_send_bytes (tot_size))
    WaitSema (can_send_sema);

  volatile struct s_descriptor *d =
    &g_nic.tx_descriptors[g_nic.tx_tail_index];
  int i;
  const u32 *p;

  REGW (0x1004) = g_nic.tx_tail_addr;
  for (e = head; e; e = e->next)
    for (i = 0, p = e->data; i < e->data_len; i += 4)
      REG (0x1100) = *(p++);
  d->size = tot_size;
  d->addr = 0x1000 + g_nic.tx_tail_addr;
  REGB (0x1010) = 0x01;
  d->flag = 0x8300;
  REG (0x2008) = 0x8000;	//Yes! REG, not REGW!
  g_nic.tx_tail_addr = (g_nic.tx_tail_addr + tot_size) % 4096;
  g_nic.tx_tail_index = (g_nic.tx_tail_index + 1) % 64;
  return (1);
}

unsigned char *
nic_get_ethaddr (void)
{
  return g_nic.ethaddr;
}


/* executed in interrupt handler context only */
static int
nic_interrupt (int f)
{
  int irq;
  irq = REGW (0x28) & 0x7C;
  if (irq & 0x14)
    {
      REGW (0x128) = irq & 0x14;
      nic_tx_interrupt ();
    }
  irq = REGW (0x28) & 0x7C;
  if (irq & 0x28)
    {
      REGW (0x128) = irq & 0x28;
      nic_rx_interrupt ();
    }
  if (REGW (0x28) & 0x40)
    {
      REGW (0x2014) = REGW (0x2014);
      REGW (0x2016) = REGW (0x2016);
      REGW (0x128) = 0x40;
    }
  return 1;
}


/* returns 1 if everything is ok */
int
nic_init (void)
{
  u16 i, j;
  u32 old_interrupts;

  iop_sema_t sema;
  sema.attr = 0;
  sema.option = 0;
  sema.initial = 0;
  sema.max = 1;
  got_data_sema = CreateSema (&sema);
  can_send_sema = CreateSema (&sema);

  dev9IntrDisable (0x7C);
  EnableIntr (IOP_IRQ_DEV9);
  CpuEnableIntr ();
  REGW (0xF801464) = 3;
  memset (&g_nic, 0, sizeof (struct s_nic));
  g_nic.rx_descriptors = (struct s_descriptor *) (BASE + 0x3200);
  g_nic.tx_descriptors = (struct s_descriptor *) (BASE + 0x3000);

  //obtain ethaddr from EEPROM
  CpuSuspendIntr ((void *) &old_interrupts);
  REGB (0x2C) = 0xE0;		//sel clk din 00000
  REGB (0x2E) = 0x00;
  DelayThread (1);
  REGB (0x2E) = 0x80;
  DelayThread (1);
  for (i = 0; i < 2; i++)
    {
      REGB (0x2E) = 0xA0;
      DelayThread (1);
      REGB (0x2E) = 0xE0;
      DelayThread (1);
      REGB (0x2E) = 0xA0;
      DelayThread (1);
    }
  for (i = 0; i < 7; i++)
    {
      REGB (0x2E) = 0x80;
      DelayThread (1);
      REGB (0x2E) = 0xC0;
      DelayThread (1);
      REGB (0x2E) = 0x80;
      DelayThread (1);
    }
  for (j = 0; j < 6; j++)
    for (i = 0; i < 8; i++)
      {
	g_nic.ethaddr[j ^ 1] <<= 1;
	REGB (0x2E) = 0x80;
	DelayThread (1);
	REGB (0x2E) = 0xC0;
	DelayThread (1);
	g_nic.ethaddr[j ^ 1] |= ((REGB (0x2E) >> 4) & 1);
	REGB (0x2E) = 0x80;
	DelayThread (1);
      }
  REGB (0x2E) = 0x00;
  DelayThread (2);
  CpuResumeIntr (old_interrupts);

  //network interface card initialization
  dev9IntrDisable (0x7C);
  REGW (0x128) = 0x7C;
  REGW (0x2014) = 0x03FF;
  REGW (0x2016) = 0x03FB;
  REGB (0x102) = 0;
  REGB (0x1000) = 1;
  REGB (0x1030) = 1;
  for (i = 0; i < 256; i++)
    if (((REGB (0x1000) | REGB (0x1030)) & 1) == 0)
      break;
  REGW (0x2000) = 0x2000;
  for (i = 0; i < 256; i++)
    if ((REGW (0x2000) & 0x2000) == 0)
      break;
  REGW (0x2004) = 0x8164;
  REGW (0x2006) = 0x0000;
  for (i = 0; i < 256; i++)
    if ((REGW (0x205E) & 0x8000) == 0x8000)
      break;
  REGW (0x205C) = 0xB100;
  REGW (0x205E) = 0x2020;
  for (i = 0; i < 256; i++)
    if ((REGW (0x205E) & 0x8000) == 0x8000)
      break;
  DelayThread (300);
  for (j = 0; j < 256; j++)
    {
      REGW (0x205E) = 0x1020;
      for (i = 0; i < 10000; i++)
	if ((REGW (0x205E) & 0x8000) == 0x8000)
	  break;
      if ((REGW (0x205C) & 0x8000) == 0)
	break;
      DelayThread (300);
    }
  for (j = 0; j < 256; j++)
    {
      REGW (0x205E) = 0x1021;
      for (i = 0; i < 10000; i++)
	if ((REGW (0x205E) & 0x8000) == 0x8000)
	  break;
      if ((REGW (0x205C) & 0x0020) == 0x0020)
	break;
      DelayThread (1000);
    }
  if (REGW (2) <= 18)		//untested, related to old ps2 versions
    {
      for (j = 0; j < 256; j++)
	{
	  REGW (0x205E) = 0x1030;
	  for (i = 0; i < 256; i++)
	    if ((REGW (0x205E) & 0x8000) == 0x8000)
	      break;
	  i = REGW (0x205C);
	  if ((i & 0x0011) == 0x0011)
	    break;
	  DelayThread (1000);
	}
      j = REGW (0x2004) & 0xFF3F;
      if (i & 0x0004)
	j |= 0x9800;

      else if ((i & 0x0002) == 0x0002)
	j &= 0x66FF;

      else
	j &= 0x67FF;
      if ((i & 0x0002) == 0x0000)
	j |= 0x0040;
      REGW (0x2004) = j;
    }
  dev9IntrDisable (0x7C);
  REGW (0x128) = 0x7C;
  REGW (0x2014) = 0x03FF;
  REGW (0x2016) = 0x03FB;
  REGW (0x201C) = 0;
  REGW (0x201E) = (g_nic.ethaddr[0] << 8) | g_nic.ethaddr[1];
  REGW (0x2020) = (g_nic.ethaddr[2] << 8) | g_nic.ethaddr[3];
  REGW (0x2022) = (g_nic.ethaddr[4] << 8) | g_nic.ethaddr[5];
  REGW (0x205A) = 0x0004;
  REGW (0x2010) = 0xC050;
  REGW (0x200C) = 0x380F;
  REGW (0x2060) = 0x6000;
  REGW (0x2064) = 0x0800;
  REGW (0x2066) = 0x4000;
  REGW (0x2018) = 0x03FF;
  REGW (0x201A) = 0x03FB;
  REGW (0x2000) = REGW (0x2000) & 0xE7FF;
  REGB (0x1000) = 1;
  REGB (0x1030) = 1;
  for (i = 0; i < 256; i++)
    if (((REGB (0x1000) | REGB (0x1030)) & 1) == 0)
      break;

  //descriptors initialization
  g_nic.rx_next_addr = 0;
  g_nic.rx_next_index = 0;
  g_nic.tx_head_addr = 0;
  g_nic.tx_head_index = 0;
  g_nic.tx_tail_addr = 0;
  g_nic.tx_tail_index = 0;
  for (i = 0; i < 64; i++)
    {
      g_nic.rx_descriptors[i].flag = 0x8000;
      g_nic.rx_descriptors[i].zero = 0;
      g_nic.rx_descriptors[i].size = 0;
      g_nic.rx_descriptors[i].addr = 0;
      g_nic.tx_descriptors[i].flag = 0;
      g_nic.tx_descriptors[i].zero = 0;
      g_nic.tx_descriptors[i].size = 0;
      g_nic.tx_descriptors[i].addr = 0;
    }
  for (i = 2; i < 7; i++)
    dev9RegisterIntrCb (i, nic_interrupt);
  nic_start ();
  return 1;
}


void
nic_quit (void)
{
  int i;
  nic_stop ();
  for (i = 2; i < 7; ++i)
    dev9RegisterIntrCb (i, NULL);
}
