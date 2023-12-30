/**
 * @brief segment.cpp
 */

#include <ethernet/endian.h>
#include <ip/packet.h>
#include <tcp/segment.h>

RetransElem::RetransElem(u_char *seg, unsigned int s, int l): 
    segment(seg), seq(s), len(l), time(0) {}

RetransElem::~RetransElem()
{
    delete[] segment;
}

u_short 
calculate_checksum(const u_char *segment, int len)
{
    size_t sum = 0;
    size_t num = 0;
    const u_short *seg_short = (const u_short *)segment;
    for(int i = 0; i < len / 2; i++){
        num = ((seg_short[i] & 0xff) << 8) | ((seg_short[i] & 0xff00) >> 8);
        sum += num;
    }
    if(len % 2){
        sum += (size_t)segment[len - 1] << 8;
    }
    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return change_order((u_short)~sum);
}