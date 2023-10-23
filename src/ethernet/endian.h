/**
 * @file endian.h
 * @brief Determine byte order of the machine and provide a function that 
 * changes byte order if it's not big endian.
 */

#ifndef ETHERNET_ENDIAN
#define ETHERNET_ENDIAN

#include <sys/types.h>

/**
 * @brief Changes the byte order of an unsigned short.
 * 
 * @param src Source number which possibly need reversing.
 * @return Unsigned short in the correct order.
 */
inline u_short change_order(u_short src);

#endif /**< ETHERNET_ENDIAN */