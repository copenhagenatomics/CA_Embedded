/*
 * time_u32.h
 *
 *  Created on: 12 Nov 2021
 *      Author: agp
 */

#pragma once

#include <stdint.h>

// Finds the time difference tDiff = head - tail and assume head >= tail.
// If head < tail it is assumed that head is overflow and time difference is then
// (head + 2^32) - tail
uint32_t tdiff_u32(uint32_t head, uint32_t tail);
