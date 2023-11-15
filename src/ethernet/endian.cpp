/**
 * @file endian.cpp
 */

#include <ethernet/endian.h>

u_short 
change_order(u_short src)
{
    u_short dst;
    u_char *src_ptr = (u_char *)&src;
    u_char *dst_ptr = (u_char *)&dst;
    dst_ptr[0] = src_ptr[1];
    dst_ptr[1] = src_ptr[0];
    return dst;
}

unsigned int
change_order(unsigned int src)
{
    unsigned int dst;
    u_char *src_ptr = (u_char *)&src;
    u_char *dst_ptr = (u_char *)&dst;
    dst_ptr[0] = src_ptr[3];
    dst_ptr[1] = src_ptr[2];
    dst_ptr[2] = src_ptr[1];
    dst_ptr[3] = src_ptr[0];
    return dst;
}