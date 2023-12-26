/**
 * @file bitmap.h
 * @brief Defines bitmap used for managing port numbers.
 */

#pragma once

#include <sys/types.h>

/**
 * Element type.
 * 
 * This must be an unsigned integer type at least as wide as int.
 * 
 * Each bit represents one bit in the bitmap.
 * If bit 0 in an element represents bit K in the bitmap,
 * then bit 1 in the element represents bit K+1 in the bitmap,
 * and so on.
 */
typedef unsigned long elem_type;

/** Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)
#define BITMAP_ERROR (-1)

/** 
 * From the outside, a bitmap is an array of bits.  From the
 * inside, it's an array of elem_type (defined above) that
 * simulates an array of bits. 
 */
class BitMap 
{
private:
    size_t bit_cnt;     /**< Number of bits. */
    elem_type *bits;    /**< Elements that represent bits. */
public:
    BitMap(size_t bit_cnt);
    ~BitMap();

    size_t bitmap_size();
    void bitmap_set(size_t idx, bool value);
    void bitmap_mark(size_t bit_idx);
    void bitmap_reset(size_t bit_idx);
    void bitmap_flip(size_t bit_idx);
    bool bitmap_test(size_t idx);
    void bitmap_set_all(bool value);
    void bitmap_set_multiple(size_t start, size_t cnt, bool value);
    bool bitmap_contains(size_t start, size_t cnt, bool value);
    bool bitmap_any(size_t start, size_t cnt);
    bool bitmap_none (size_t start, size_t cnt);
    bool bitmap_all(size_t start, size_t cnt);
    size_t bitmap_scan(size_t start, size_t cnt, bool value);
    size_t bitmap_scan_and_flip(size_t start, size_t cnt, bool value);
};