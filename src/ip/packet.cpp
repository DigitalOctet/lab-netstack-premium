/**
 * @file packet.cpp
 */

#include "packet.h"

u_short 
calculate_checksum(const u_short *header, int len)
{
    unsigned int sum = 0;
    unsigned int num = 0;
    for(int i = 0; i < len; i++){
        num = ((header[i] & 0xff) << 8) | ((header[i] & 0xff00) >> 8);
        sum += num;
    }
    sum = (sum & 0xffff) + ((sum & 0xffff0000) >> 16);
    return ~sum;
}