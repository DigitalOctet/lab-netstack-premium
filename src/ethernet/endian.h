/**
 * @file endian.h
 * @brief My implementation only support little-endian machines. These 
 * overloaded functions are used for changing the order of a number.
 * 
 * Currently supported types:
 *     1. u_short(unsigned short)
 *     2. unsigned int
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
u_short change_order(u_short src);

/**
 * @brief Changes the byte order of an unsigned int.
 * 
 * @param src Source number which possibly need reversing.
 * @return Unsigned int in the correct order.
 */
unsigned int change_order(unsigned int src);

#endif /**< ETHERNET_ENDIAN */