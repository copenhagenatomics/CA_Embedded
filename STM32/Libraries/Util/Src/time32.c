/*
 * time_u32.c
 *
 *  Created on: 12 Nov 2021
 *      Author: agp
 */

#include <time32.h>

uint32_t tdiff_u32(uint32_t head, uint32_t tail)
{
    if (head >= tail)
        return head - tail;

    // Overflow of head:
    // (head + 2^32) - tail = (2^32 - tail) + head = (see below)
    return (((uint32_t) -1) - tail) + 1 + head;
}

