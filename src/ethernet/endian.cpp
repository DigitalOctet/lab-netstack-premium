/**
 * @file endian.cpp
 */

#include "endian.h"

/** Check whether the byte order of the host is little endian. */
const size_t i = 1;
const bool is_little_endian = (*((u_char *)&i) == 1);

inline u_short 
change_order(u_short src)
{
    if(is_little_endian){
        u_short dst;
        u_char *src_ptr = (u_char *)&src;
        u_char *dst_ptr = (u_char *)&dst;
        dst_ptr[0] = src_ptr[1];
        dst_ptr[1] = src_ptr[0];
        return dst;
    }
    else{
        return src;
    }
}