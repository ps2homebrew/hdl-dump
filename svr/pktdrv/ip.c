#include "ip.h"

static nt_ip_t my_ip = {0, 0, 0, 0};

void ip_setup(nt_ip_t ip)
{
    COPY_NT_IP(my_ip, ip);
}


nt_byte_t *
ip_checksum(const void *p, size_t len, nt_word_t checksum)
{
    unsigned long tot = 0;
    while (len >= 2) {
        tot += GET_NT_WORD((unsigned char *)p);
        p = (unsigned short *)p + 1;
        len -= 2;
    }
    if (len)
        tot += *(unsigned char *)p;
    while (tot > 0xffff)
        tot = (tot & 0xffff) + (tot >> 16);
    tot = ~tot;
    SET_NT_WORD(checksum, tot);
    return (checksum);
}


void ip_header(ip_hdr_t *ip,
               const ip_hdr_t *in_reply_to,
               nt_byte_t proto,
               size_t tot_len)
{
    nt_word_t cs;
    ip->version_and_hdr_len = (4 << 4) | (5 << 0);
    ip->diff_serv = 0;
    SET_NT_WORD(ip->tot_len, tot_len);
    SET_NT_WORD(ip->id, 0);
    SET_NT_WORD(ip->flags_frag_offs, 0);
    ip->ttl = in_reply_to->ttl;
    ip->proto = proto;
    SET_NT_WORD(ip->hdr_checksum, 0); /* below */
    COPY_NT_IP(ip->src_ip, in_reply_to->dst_ip);
    COPY_NT_IP(ip->dst_ip, in_reply_to->src_ip);
    SET_NT_WORD(cs, 0);
    COPY_NT_WORD(ip->hdr_checksum,
                 ip_checksum(ip, sizeof(ip_hdr_t), cs));
}


const nt_byte_t *
ip_address(void)
{
    return (my_ip);
}
