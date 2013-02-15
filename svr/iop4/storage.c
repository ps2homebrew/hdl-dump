#include "storage.h"
#include <stddef.h>


unsigned char storage[STORAGE_MAX * MAX_PACKET_SIZE] __attribute__ ((aligned (64)));
volatile unsigned int storage_len[STORAGE_MAX];
volatile int storage_next_in = 0;
volatile int storage_next_out = 0;
volatile int storage_counter = 0;

typedef struct frame_type
{
  unsigned char data[1528];
  size_t len;
  void *user;
} frame_t; /* total size 1536 bytes */


/*
 * TODO:
 *
 * containers:
 * queue/stack of free packets
 * fifo of packets to sent
 * fifo of received packets
 *
 * methods for:
 * initialization of data structures
 * allocation of free packet (and take ownership) (can fail with NULL)
 * add a free packet to any queue (2x)
 * borrow next packet from the queue (and take ownership) (2x)
 * release ownership of a packet
 */
