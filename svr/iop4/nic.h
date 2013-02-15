#ifndef	__NIC_H__
#define	__NIC_H__

int nic_init (void);

void nic_quit (void);

int nic_receive (unsigned char **ppPacket);

int nic_receive_wait (unsigned char **ppPacket);

int nic_send (const void *pPacket, int size);

int nic_send_wait (const void *pPacket, int size);

unsigned char *nic_get_ethaddr (void);

/* same as nic_send
 * pros: can send out-of-order data (not consecutive bytes)
 * cons: each chunk must be multiple to 4 bytes */
struct schain
{
  const struct schain *next;
  const void *data;
  unsigned short data_len;
};
int nic_send_chain (const struct schain *head);

#endif /* __NIC_H__ defined? */
