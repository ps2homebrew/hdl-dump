#ifndef	__STORAGE_H__
#define	__STORAGE_H__

#define STORAGE_MAX	128
#define MAX_PACKET_SIZE	1536
extern unsigned char storage[STORAGE_MAX * MAX_PACKET_SIZE];	//align(64) multiple(32*64)
extern volatile unsigned int storage_len[STORAGE_MAX];
extern volatile int storage_next_in;
extern volatile int storage_next_out;
extern volatile int storage_counter;

#endif /*  */
