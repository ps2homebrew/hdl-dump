#define CRC16_POLYNOMIAL        0x8005
#define CRC16_INITIAL_CHECKSUM  0x0
#define CRC16_FINAL_XOR_VALUE   0x0

void InitCRC16Table(void);
unsigned short int CalculateCRC16(unsigned char *buffer, unsigned int length, unsigned short int InitialChecksum);
unsigned short int ReflectAndXORCRC16(unsigned short int crc);
