/*
Copyright (C) 2010-2014 GRNET S.A.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <math.h>

#include "bench-lfsr.h"

/*
 * LFSR taps retrieved from:
 * http://home1.gte.net/res0658s/electronics/LFSRtaps.html
 *
 * The memory overhead of the following tap table should be relatively small,
 * no more than 400 bytes.
 */
static uint8_t taps[64][MAX_TAPS] = {
    {0}, {0}, {0},              //LFSRs with less that 3-bits cannot exist
    {3, 2},                     //Tap position for 3-bit LFSR
    {4, 3},                     //Tap position for 4-bit LFSR
    {5, 3},                     //Tap position for 5-bit LFSR
    {6, 5},                     //Tap position for 6-bit LFSR
    {7, 6},                     //Tap position for 7-bit LFSR
    {8, 6, 5, 4},               //Tap position for 8-bit LFSR
    {9, 5},                     //Tap position for 9-bit LFSR
    {10, 7},                    //Tap position for 10-bit LFSR
    {11, 9},                    //Tap position for 11-bit LFSR
    {12, 6, 4, 1},              //Tap position for 12-bit LFSR
    {13, 4, 3, 1},              //Tap position for 13-bit LFSR
    {14, 5, 3, 1},              //Tap position for 14-bit LFSR
    {15, 14},                   //Tap position for 15-bit LFSR
    {16, 15, 13, 4},            //Tap position for 16-bit LFSR
    {17, 14},                   //Tap position for 17-bit LFSR
    {18, 11},                   //Tap position for 18-bit LFSR
    {19, 6, 2, 1},              //Tap position for 19-bit LFSR
    {20, 17},                   //Tap position for 20-bit LFSR
    {21, 19},                   //Tap position for 21-bit LFSR
    {22, 21},                   //Tap position for 22-bit LFSR
    {23, 18},                   //Tap position for 23-bit LFSR
    {24, 23, 22, 17},           //Tap position for 24-bit LFSR
    {25, 22},                   //Tap position for 25-bit LFSR
    {26, 6, 2, 1},              //Tap position for 26-bit LFSR
    {27, 5, 2, 1},              //Tap position for 27-bit LFSR
    {28, 25},                   //Tap position for 28-bit LFSR
    {29, 27},                   //Tap position for 29-bit LFSR
    {30, 6, 4, 1},              //Tap position for 30-bit LFSR
    {31, 28},                   //Tap position for 31-bit LFSR
    {32, 31, 29, 1},            //Tap position for 32-bit LFSR
    {33, 20},                   //Tap position for 33-bit LFSR
    {34, 27, 2, 1},             //Tap position for 34-bit LFSR
    {35, 33},                   //Tap position for 35-bit LFSR
    {36, 25},                   //Tap position for 36-bit LFSR
    {37, 5, 4, 3, 2, 1},        //Tap position for 37-bit LFSR
    {38, 6, 5, 1},              //Tap position for 38-bit LFSR
    {39, 35},                   //Tap position for 39-bit LFSR
    {40, 38, 21, 19},           //Tap position for 40-bit LFSR
    {41, 38},                   //Tap position for 41-bit LFSR
    {42, 41, 20, 19},           //Tap position for 42-bit LFSR
    {43, 42, 38, 37},           //Tap position for 43-bit LFSR
    {44, 43, 18, 17},           //Tap position for 44-bit LFSR
    {45, 44, 42, 41},           //Tap position for 45-bit LFSR
    {46, 45, 26, 25},           //Tap position for 46-bit LFSR
    {47, 42},                   //Tap position for 47-bit LFSR
    {48, 47, 21, 20},           //Tap position for 48-bit LFSR
    {49, 40},                   //Tap position for 49-bit LFSR
    {50, 49, 24, 23},           //Tap position for 50-bit LFSR
    {51, 50, 36, 35},           //Tap position for 51-bit LFSR
    {52, 49},                   //Tap position for 52-bit LFSR
    {53, 52, 38, 37},           //Tap position for 53-bit LFSR
    {54, 53, 18, 17},           //Tap position for 54-bit LFSR
    {55, 31},                   //Tap position for 55-bit LFSR
    {56, 55, 35, 34},           //Tap position for 56-bit LFSR
    {57, 50},                   //Tap position for 57-bit LFSR
    {58, 39},                   //Tap position for 58-bit LFSR
    {59, 58, 38, 37},           //Tap position for 59-bit LFSR
    {60, 59},                   //Tap position for 60-bit LFSR
    {61, 60, 46, 45},           //Tap position for 61-bit LFSR
    {62, 61, 6, 5},             //Tap position for 62-bit LFSR
    {63, 62},                   //Tap position for 63-bit LFSR
};

#define __LFSR_NEXT(__lfsr, __v)						\
	__v = ((__v >> 1) | __lfsr->cached_bit) ^			\
			(((__v & 1UL) - 1UL) & __lfsr->xormask);

static inline void __lfsr_next(struct bench_lfsr *lfsr, unsigned int spin)
{
    /*
     * This should be O(1) since most compilers will create a jump table for
     * this switch.
     */
    switch (spin) {
    case 16:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 15:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 14:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 13:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 12:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 11:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 10:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 9:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 8:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 7:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 6:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 5:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 4:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 3:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 2:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 1:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    case 0:
        __LFSR_NEXT(lfsr, lfsr->last_val);
    default:
        break;
    }
}

/*
 * lfsr_next does the following:
 *
 * a. Return if the number of max values has been exceeded.
 * b. Check if we have a spin value that produces a repeating subsequence.
 *    This is previously calculated in `prepare_spin` and cycle_length should
 *    be > 0. If we do have such a spin:
 *
 *    i. Decrement the calculated cycle.
 *    ii. If it reaches zero, add "+1" to the spin and reset the cycle_length
 *        (we have it cached in the struct fio_lfsr)
 *
 *    In either case, continue with the calculation of the next value.
 * c. Check if the calculated value exceeds the desirable range. In this case,
 *    go back to b, else return.
 */
uint64_t lfsr_next(struct bench_lfsr *lfsr)
{
    lfsr->num_vals++;

    do {
        if (lfsr->cycle_length) {
            lfsr->cycle_length--;
            if (!lfsr->cycle_length) {
                __lfsr_next(lfsr, lfsr->spin + 1);
                lfsr->cycle_length = lfsr->cached_cycle_length;
                goto check;
            }
        }
        __lfsr_next(lfsr, lfsr->spin);
check:;
    } while (lfsr->last_val > lfsr->max_val);

    return lfsr->last_val;
}

static uint64_t lfsr_create_xormask(uint8_t * taps)
{
    int i;
    uint64_t xormask = 0;

    for (i = 0; i < MAX_TAPS && taps[i] != 0; i++) {
        xormask |= 1UL << (taps[i] - 1);
    }

    return xormask;
}

static uint8_t *find_lfsr(uint64_t size)
{
    int i;

    for (i = 3; i < 64; i++) {
        if ((1UL << i) > size) {        /* TODO: Explain why. */
            return taps[i];
        }
    }

    return NULL;
}

/*
 * It is well-known that all maximal n-bit LFSRs will start repeating
 * themselves after their 2^n iteration. The introduction of spins however, is
 * possible to create a repetition of a sub-sequence before we hit that mark.
 * This happens if:
 *
 * [1]: ((2^n - 1) * i) % (spin + 1) == 0,
 * where "n" is LFSR's bits and "i" any number within the range [1,spin]
 *
 * It is important to know beforehand if a spin can cause a repetition of a
 * sub-sequence (cycle) and its length. However, calculating (2^n - 1) * i may
 * produce a buffer overflow for "n" close to 64, so we expand the above to:
 *
 * [2]: (2^n - 1) -> (x * (spin + 1) + y), where x >= 0 and 0 <= y <= spin
 *
 * Thus, [1] is equivalent to (y * i) % (spin + 1) == 0;
 * Also, the cycle's length will be (x * i) + (y * i) / (spin + 1)
 */
int prepare_spin(struct bench_lfsr *lfsr, unsigned int spin)
{
    uint64_t max = (lfsr->cached_bit << 1) - 1;
    uint64_t x, y;
    int i;

    if (spin > 15) {
        return 1;
    }

    x = max / (spin + 1);
    y = max % (spin + 1);
    lfsr->cycle_length = max;   /* This is the expected cycle */
    lfsr->spin = spin;

    for (i = 1; i <= spin; i++) {
        if ((y * i) % (spin + 1) == 0) {
            lfsr->cycle_length = (x * i) + (y * i) / (spin + 1);
            break;
        }
    }
    lfsr->cached_cycle_length = lfsr->cycle_length;

    return 0;
}

int lfsr_reset(struct bench_lfsr *lfsr, unsigned long seed)
{
    uint64_t bitmask = (lfsr->cached_bit << 1) - 1;

    lfsr->num_vals = 0;
    lfsr->last_val = seed & bitmask;

    /* All-ones state is illegal for XNOR LFSRs */
    if (lfsr->last_val == bitmask) {
        return 1;
    }

    return 0;
}

int lfsr_init(struct bench_lfsr *lfsr, uint64_t nums, unsigned long seed,
              unsigned int spin)
{
    uint8_t *lfsr_taps;

    lfsr_taps = find_lfsr(nums);
    if (!lfsr_taps) {
        return 1;
    }

    lfsr->max_val = nums - 1;
    lfsr->xormask = lfsr_create_xormask(lfsr_taps);
    lfsr->cached_bit = 1UL << (lfsr_taps[0] - 1);

    if (prepare_spin(lfsr, spin)) {
        return 1;
    }

    if (lfsr_reset(lfsr, seed)) {
        return 1;
    }

    return 0;
}
