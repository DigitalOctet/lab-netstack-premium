/**
 * @file bitmap.cpp
 * 
 * Adapted from PintOS.
 */

#include <tcp/tcp.h>
#include <limits.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

/** 
 * Returns the index of the element that contains the bit
 * numbered BIT_IDX.
 */
static inline size_t
elem_idx (size_t bit_idx) 
{
    return bit_idx / ELEM_BITS;
}

/** 
 * Returns an elem_type where only the bit corresponding to
 * BIT_IDX is turned on. 
 */
static inline elem_type
bit_mask (size_t bit_idx) 
{
    return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/** Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
    return (bit_cnt + ELEM_BITS - 1) / ELEM_BITS;
}

/** Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
    return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/** Creation and destruction. */

/** 
 * Creates and returns a pointer to a newly allocated bitmap with room for
 * BIT_CNT (or more) bits.  Returns a null pointer if memory allocation fails.
 * The caller is responsible for freeing the bitmap, with bitmap_destroy(),
 * when it is no longer needed.
 */
BitMap::BitMap(size_t cnt): bit_cnt(cnt)
{
    bits = (elem_type *)malloc(byte_cnt (bit_cnt));
    if (bits != NULL || bit_cnt == 0) {
        bitmap_set_all(false);
    }
}

/** 
 * Destroys bitmap B, freeing its storage.
 * Not for use on bitmaps created by bitmap_create_in_buf(). 
 */
BitMap::~BitMap() 
{
    if (bits) {
        free(bits);
    }
}

/** Bitmap size. */

/** Returns the number of bits in B. */
size_t
BitMap::bitmap_size()
{
    return bit_cnt;
}

/** Setting and testing single bits. */

/** Atomically sets the bit numbered IDX in B to VALUE. */
void
BitMap::bitmap_set(size_t idx, bool value) 
{
    if (idx < bit_cnt) {
        exit(-1);
    }
    if (value)
        bitmap_mark(idx);
    else
        bitmap_reset(idx);
}

/** Atomically sets the bit numbered BIT_IDX in B to true. */
void
BitMap::bitmap_mark(size_t bit_idx) 
{
    size_t idx = elem_idx (bit_idx);
    elem_type mask = bit_mask (bit_idx);

    /* This is equivalent to `b->bits[idx] |= mask' except that it
        is guaranteed to be atomic on a uniprocessor machine.  See
        the description of the OR instruction in [IA32-v2b]. */
    asm ("orq %1, %0" : "=m" (bits[idx]) : "r" (mask) : "cc");
}

/** Atomically sets the bit numbered BIT_IDX in B to false. */
void
BitMap::bitmap_reset(size_t bit_idx) 
{
    size_t idx = elem_idx(bit_idx);
    elem_type mask = bit_mask(bit_idx);

    /* This is equivalent to `b->bits[idx] &= ~mask' except that it
        is guaranteed to be atomic on a uniprocessor machine.  See
        the description of the AND instruction in [IA32-v2a]. */
    asm ("andq %1, %0" : "=m" (bits[idx]) : "r" (~mask) : "cc");
}

/** Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
BitMap::bitmap_flip (size_t bit_idx) 
{
    size_t idx = elem_idx (bit_idx);
    elem_type mask = bit_mask (bit_idx);

    /* This is equivalent to `b->bits[idx] ^= mask' except that it
        is guaranteed to be atomic on a uniprocessor machine.  See
        the description of the XOR instruction in [IA32-v2b]. */
    asm ("xorq %1, %0" : "=m" (bits[idx]) : "r" (mask) : "cc");
}

/** Returns the value of the bit numbered IDX in B. */
bool
BitMap::bitmap_test(size_t idx) 
{
    if (idx < bit_cnt) {
        fprintf(stderr, "IDX is too big!\n");
        exit(-1);
    }
    return (bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/** Setting and testing multiple bits. */

/** Sets all bits in B to VALUE. */
void
BitMap::bitmap_set_all(bool value) 
{
    bitmap_set_multiple(0, bitmap_size(), value);
}

/** Sets the CNT bits starting at START in B to VALUE. */
void
BitMap::bitmap_set_multiple(size_t start, size_t cnt, bool value) 
{
    size_t i;
    
    if ((start > bit_cnt) || (start + cnt > bit_cnt)) {
        fprintf(stderr, "Range is invalid!\n");
        exit(-1);
    }

    for (i = 0; i < cnt; i++)
        bitmap_set (start + i, value);
}

/** Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
BitMap::bitmap_contains(size_t start, size_t cnt, bool value) 
{
    size_t i;
    
    if((start > bit_cnt) || (start + cnt > bit_cnt)) {
        exit(-1);
    }

    for (i = 0; i < cnt; i++)
        if (bitmap_test (start + i) == value)
        return true;
    return false;
    }

/** Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
BitMap::bitmap_any(size_t start, size_t cnt) 
{
    return bitmap_contains (start, cnt, true);
}

/** Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
BitMap::bitmap_none (size_t start, size_t cnt) 
{
    return !bitmap_contains (start, cnt, true);
}

/** Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
BitMap::bitmap_all(size_t start, size_t cnt) 
{
    return !bitmap_contains(start, cnt, false);
}

/** Finding set or unset bits. */

/** 
 * Finds and returns the starting index of the first group of CNT
 * consecutive bits in B at or after START that are all set to VALUE.
 * If there is no such group, returns BITMAP_ERROR. 
 */
size_t
BitMap::bitmap_scan(size_t start, size_t cnt, bool value) 
{
    if (start > bit_cnt) {
        return BITMAP_ERROR;
    }

    if (cnt <= bit_cnt) 
        {
        size_t last = bit_cnt - cnt;
        size_t i;
        for (i = start; i <= last; i++)
            if (!bitmap_contains (i, cnt, !value))
            return i; 
        }
    return BITMAP_ERROR;
}

/** 
 * Finds the first group of CNT consecutive bits in B at or after
 * START that are all set to VALUE, flips them all to !VALUE,
 * and returns the index of the first bit in the group.
 * If there is no such group, returns BITMAP_ERROR.
 * If CNT is zero, returns 0.
 * Bits are set atomically, but testing bits is not atomic with
 * setting them. 
 */
size_t
BitMap::bitmap_scan_and_flip(size_t start, size_t cnt, bool value)
{
    size_t idx = bitmap_scan (start, cnt, value);
    if (idx != BITMAP_ERROR)
        bitmap_set_multiple (idx, cnt, !value);
    return idx;
}

/**
 * @brief add an additional port
 */
void
BitMap::bitmap_add(size_t bit_idx) 
{
    size_t idx = elem_idx (bit_idx);
    elem_type mask = bit_mask (bit_idx);
    mutex.lock();
    auto it = map.find(idx);
    if(it != map.end()){
        it->second++;
    }
    else{
        map[idx] = 1;
    }
    mutex.unlock();
    return;
}

/**
 * @brief Delete an additional port. If it's the last one, reset bitmap.
 */
void
BitMap::bitmap_delete(size_t bit_idx) 
{
    size_t idx = elem_idx(bit_idx);
    elem_type mask = bit_mask(bit_idx);
    bool reset = false;
    mutex.lock();
    auto it = map.find(idx);
    if(it != map.end()){
        it->second--;
        if(it->second == -1){
            reset = true;
            map.erase(idx);
        }
    }
    else{
        reset = true;
    }

    if(reset)
        asm ("andq %1, %0" : "=m" (bits[idx]) : "r" (~mask) : "cc");
}