#if !defined(_NET_TYPES_H)
#define _NET_TYPES_H

#if defined(_IOP) || defined(_EE)
#include <tamtypes.h>
#else
#if defined(_WIN32)
typedef unsigned long u32;
typedef unsigned short u16;
#else
#include <stddef.h>
typedef uint32_t u32;
typedef uint16_t u16;
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned char nt_byte_t;
typedef unsigned char nt_word_t[2];
typedef unsigned char nt_dword_t[4];
typedef unsigned char nt_ip_t[4];
typedef unsigned char nt_port_t[2];
typedef unsigned char nt_mac_t[6];

#define IS_NT_WORD(src, val)                \
    (((src)[0] == (((val) >> 8) & 0xff)) && \
     ((src)[1] == (((val) >> 0) & 0xff)))

#define NT_WORD_EQ(w1, w2) \
    ((w1)[0] == (w2)[0] && \
     (w1)[1] == (w2)[1])

#define GET_NT_WORD(src)      \
    ((((u16)(src)[0]) << 8) | \
     (((u16)(src)[1]) << 0))

#define SET_NT_WORD(dst, val)            \
    (((dst)[0] = (((val) >> 8) & 0xff)), \
     ((dst)[1] = (((val) >> 0) & 0xff)))

#define COPY_NT_WORD(dst, src) \
    (((dst)[0] = (src)[0]),    \
     ((dst)[1] = (src)[1]))


#define IS_NT_DWORD(src, val)                \
    (((src)[0] == (((val) >> 24) & 0xff)) && \
     ((src)[1] == (((val) >> 16) & 0xff)) && \
     ((src)[2] == (((val) >> 8) & 0xff)) &&  \
     ((src)[3] == (((val) >> 0) & 0xff)))

#define NT_DWORD_EQ(dw1, dw2) \
    ((dw1)[0] == (dw2)[0] &&  \
     (dw1)[1] == (dw2)[1] &&  \
     (dw1)[2] == (dw2)[2] &&  \
     (dw1)[3] == (dw2)[3])

#define GET_NT_DWORD(src)      \
    ((((u32)(src)[0]) << 24) | \
     (((u32)(src)[1]) << 16) | \
     (((u32)(src)[2]) << 8) |  \
     (((u32)(src)[3]) << 0))

#define SET_NT_DWORD(dst, val)            \
    (((dst)[0] = (((val) >> 24) & 0xff)), \
     ((dst)[1] = (((val) >> 16) & 0xff)), \
     ((dst)[2] = (((val) >> 8) & 0xff)),  \
     ((dst)[3] = (((val) >> 0) & 0xff)))

#define COPY_NT_DWORD(dst, src) \
    (((dst)[0] = (src)[0]),     \
     ((dst)[1] = (src)[1]),     \
     ((dst)[2] = (src)[2]),     \
     ((dst)[3] = (src)[3]))


#define COPY_NT_MAC(dst, src)    \
    do {                         \
        size_t i;                \
        for (i = 0; i < 6; ++i)  \
            (dst)[i] = (src)[i]; \
    } while (0)

#define NT_IP_EQ(ip1, ip2) NT_DWORD_EQ(ip1, ip2)
#define COPY_NT_IP(dst, src) COPY_NT_DWORD(dst, src)

typedef struct eth_hdr_type
{
    nt_mac_t dst_mac;
    nt_mac_t src_mac;
    nt_word_t eth_type;              /* 0x08, 0x06 == ARP, 0x08, 0x00 == IP */
} __attribute__((packed)) eth_hdr_t; /* 14 bytes */

typedef struct arp_hdr_type
{
    nt_word_t hardware_type; /* 0x00, 0x01 for Ethernet */
    nt_word_t proto;         /* 0x08, 0x00 */
    nt_byte_t hardware_len;  /* == 6 */
    nt_byte_t proto_len;     /* == 4 */
    nt_word_t oper;
    nt_mac_t src_mac;
    nt_ip_t src_ip;
    nt_mac_t dst_mac;
    nt_ip_t dst_ip;
} __attribute__((packed)) arp_hdr_t; /* 28 bytes */

typedef struct ip_hdr_type
{
    /* header length is in 32-bit words; min 5 (20 bytes), max 15 (60 bytes) */
    nt_byte_t version_and_hdr_len; /* (version << 4) | (len << 0) */
    nt_byte_t diff_serv;
    nt_word_t tot_len; /* including header */
    nt_word_t id;
    nt_word_t flags_frag_offs; /* flags: 3 bits; frag offs: 13 bits */
    nt_byte_t ttl;
    nt_byte_t proto; /* 1: ICMP; 6: TCP; 17: UDP */
    nt_word_t hdr_checksum;
    nt_ip_t src_ip;
    nt_ip_t dst_ip;
    /* optional options, depending on hdr_len */
} __attribute__((packed)) ip_hdr_t; /* 20 bytes */

typedef struct icmp_echo_rr_type
{
    nt_byte_t type; /* == 8 for request, 0 for reply */
    nt_byte_t code; /* == 0 */
    nt_word_t hdr_checksum;
    nt_word_t id;
    nt_word_t seq_no;
    nt_byte_t data[1];                                            /* usually more than 1 byte */
} __attribute__((packed)) icmp_echo_request_t, icmp_echo_reply_t; /* 8 bytes+ */

typedef struct udp_hdr_type
{
    nt_word_t src_port;
    nt_word_t dst_port;
    nt_word_t len;
    nt_word_t checksum;
    nt_byte_t data[1];               /* usually more than 1 byte */
} __attribute__((packed)) udp_hdr_t; /* 8 bytes+ */

typedef struct arp_frame_type
{
    eth_hdr_t eth;
    arp_hdr_t arp;
} __attribute__((packed)) arp_frame_t;

typedef struct ping_frame_type
{
    eth_hdr_t eth;
    ip_hdr_t ip;
    icmp_echo_request_t echo;
} __attribute__((packed)) ping_frame_t, pong_frame_t;

typedef struct udp_frame_type
{
    eth_hdr_t eth;                     /* 14 bytes */
    ip_hdr_t ip;                       /* 20 bytes */
    udp_hdr_t udp;                     /* 8+1 bytes */
} __attribute__((packed)) udp_frame_t; /* 42 bytes */

#if defined(__cplusplus)
}
#endif

#endif /* _NET_TYPES_H defined? */
