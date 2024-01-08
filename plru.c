/*
 * Pseudo LRU replacement policy handling for Dinero IV
 * Written by Walid Ghandour and Christophe Guillon
 *
 * Copyright (C) 2023 INRIA. All rights reserved.
 * Copyright (C) 1997 NEC Research Institute, Inc. and Mark D. Hill.
 * All rights reserved.
 * Copyright (C) 1985, 1989 Mark D. Hill.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its associated documentation for non-commercial purposes is hereby
 * granted (for commercial purposes see below), provided that the above
 * copyright notice appears in all copies, derivative works or modified
 * versions of the software and any portions thereof, and that both the
 * copyright notice and this permission notice appear in the documentation.
 * NEC Research Institute Inc. and Mark D. Hill shall be given a copy of
 * any such derivative work or modified version of the software and NEC
 * Research Institute Inc.  and any of its affiliated companies (collectively
 * referred to as NECI) and Mark D. Hill shall be granted permission to use,
 * copy, modify, and distribute the software for internal use and research.
 * The name of NEC Research Institute Inc. and its affiliated companies
 * shall not be used in advertising or publicity related to the distribution
 * of the software, without the prior written consent of NECI.  All copies,
 * derivative works, or modified versions of the software shall be exported
 * or reexported in accordance with applicable laws and regulations relating
 * to export control.  This software is experimental.  NECI and Mark D. Hill
 * make no representations regarding the suitability of this software for
 * any purpose and neither NECI nor Mark D. Hill will support the software.
 *
 * Use of this software for commercial purposes is also possible, but only
 * if, in addition to the above requirements for non-commercial use, written
 * permission for such use is obtained by the commercial user from NECI or
 * Mark D. Hill prior to the fabrication and distribution of the software.
 *
 * THE SOFTWARE IS PROVIDED AS IS.  NECI AND MARK D. HILL DO NOT MAKE
 * ANY WARRANTEES EITHER EXPRESS OR IMPLIED WITH REGARD TO THE SOFTWARE.
 * NECI AND MARK D. HILL ALSO DISCLAIM ANY WARRANTY THAT THE SOFTWARE IS
 * FREE OF INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS OF OTHERS.
 * NO OTHER LICENSE EXPRESS OR IMPLIED IS HEREBY GRANTED.  NECI AND MARK
 * D. HILL SHALL NOT BE LIABLE FOR ANY DAMAGES, INCLUDING GENERAL, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, ARISING OUT OF THE USE OR INABILITY
 * TO USE THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "d4.h"

extern void d4_rep_plru_initialize(d4cache *c);

/* Fast integer log2: credits to https://stackoverflow.com/a/24748637 */
static int fast_log2(unsigned long int n)
{
#define S(k) if (n >= ((unsigned long int)(1)) << (k)) { i += (k); n >>= (k); }
    int i = -(n == 0); S(32); S(16); S(8); S(4); S(2); S(1); return i;
#undef S
}

#ifndef NDEBUG
static int is_pow2(unsigned long int n)
{
	return (n != 0) && (n & (n - 1)) == 0;
}
#endif

static void d4_rep_plru_init_path(int assoc, unsigned long int *paths, unsigned long int *masks) {
	assert(is_pow2(assoc));
	int depth = fast_log2(assoc);
	int root_bitpos = assoc > 1 ? assoc - 2: 0;
	assert(root_bitpos < sizeof(*paths)*8);
	for (int num = 0; num < assoc; num++) {
		unsigned long int path = 0;
		unsigned long int mask = 0;
		int offset_bitpos = 0;
		int level_bitpos = 0, level_width = 1;
		for (int highbit = depth - 1; highbit >= 0; highbit--) {
			int bitpos = root_bitpos - offset_bitpos;
			int next_level_bitpos = level_bitpos + level_width;
			int next_level_width = level_width << 1;
			int next_node_bitpos = ((offset_bitpos - level_bitpos) << 1) + next_level_bitpos;
			mask |= 1 << bitpos;
			if ((num & (1 << highbit)) == 0) {
				path |= 0 << bitpos;
				offset_bitpos = next_node_bitpos;
			} else {
				path |= 1 << bitpos;
				offset_bitpos = next_node_bitpos + 1;
			}
			level_bitpos = next_level_bitpos;
			level_width = next_level_width;
		}
		paths[num] = path;
		masks[num] = mask;
	}
}

void d4_rep_plru_initialize(d4cache *c) {
	int assoc = c->assoc;
	c->plru_values = malloc(sizeof(unsigned long int)*assoc);
	c->plru_masks = malloc(sizeof(unsigned long int)*assoc);
	d4_rep_plru_init_path(assoc, c->plru_values, c->plru_masks);
}

#ifdef TEST_PLRU
#include <stdio.h>
#include <string.h>

static void print_base2(FILE *f, unsigned long int n, int width) {
	assert((unsigned long int)1 << width > n);
	for (int bit = width - 1; bit >= 0; bit--) {
		if (n & ((unsigned long int)1 << bit)) { fputc('1', f); }
		else { fputc('0', f); }
	}
}

static void print_base2_array(FILE *f, unsigned long int *a, int size, int width) {
	for (int i = 0; i < size; i++) {
		fprintf(f, " 0b");
		print_base2(f, a[i], width);
	}
}

static int check_result_array(FILE *f, unsigned long int *ref, unsigned long int *tst, int size, int width) {
	if (memcmp(ref, tst, sizeof(*ref)*size) != 0) {
		fprintf(stderr, "Arrays mismatch (size %d):\n", size);
		fprintf(stderr, " REF:");
		print_base2_array(f, ref, size, width);
		fprintf(stderr, "\n TST:");
		print_base2_array(f, tst, size, width);
		fprintf(stderr, "\n");
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	unsigned long int paths[128];
	unsigned long int masks[128];
	unsigned long int paths_1[] = {0b0};
	unsigned long int masks_1[] = {0b0};
	unsigned long int paths_2[] = {0b0, 0b1};
	unsigned long int masks_2[] = {0b1, 0b1};
	unsigned long int paths_4[] = {0b000, 0b010, 0b100, 0b101};
	unsigned long int masks_4[] = {0b110, 0b110, 0b101, 0b101};
	unsigned long int paths_8[] = {0b0000000, 0b0001000, 0b0100000, 0b0100100, 0b1000000, 0b1000010, 0b1010000, 0b1010001};
	unsigned long int masks_8[] = {0b1101000, 0b1101000, 0b1100100, 0b1100100, 0b1010010, 0b1010010, 0b1010001, 0b1010001};
	int errors = 0;
	d4_rep_plru_init_path(1, paths, masks);
	errors += check_result_array(stderr, paths, paths_1, 1, 1);
	errors += check_result_array(stderr, masks, masks_1, 1, 1);
	d4_rep_plru_init_path(2, paths, masks);
	errors += check_result_array(stderr, paths, paths_2, 2, 2-1);
	errors += check_result_array(stderr, masks, masks_2, 2, 2-1);
	d4_rep_plru_init_path(4, paths, masks);
	errors += check_result_array(stderr, paths, paths_4, 4, 4-1);
	errors += check_result_array(stderr, masks, masks_4, 4, 4-1);
	d4_rep_plru_init_path(8, paths, masks);
	errors += check_result_array(stderr, paths, paths_8, 8, 8-1);
	errors += check_result_array(stderr, masks, masks_8, 8, 8-1);
	d4_rep_plru_init_path(16, paths, masks);
	d4_rep_plru_init_path(32, paths, masks);
	d4_rep_plru_init_path(64, paths, masks);
	// TODO: should we support associativity > 64?
	//d4_rep_plru_init_path(128, paths, masks);
	if (errors) {
		fprintf(stdout, "FAILURE: %d errors\n", errors);
	} else {
		fprintf(stdout, "SUCCESS\n");
	}
	return errors != 0;
}
#endif
