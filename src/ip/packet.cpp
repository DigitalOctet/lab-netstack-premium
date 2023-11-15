/**
 * @file packet.cpp
 */

#include <ip/packet.h>

u_short 
calculate_checksum(const u_short *header, int len)
{
    // There are at most 30 2-byte numbers, so unsigned int is big enough.
    unsigned int sum = 0;
    unsigned int num = 0;
    for(int i = 0; i < len; i++){
        num = ((header[i] & 0xff) << 8) | ((header[i] & 0xff00) >> 8);
        sum += num;
    }
    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }
    
    return ~sum;
}