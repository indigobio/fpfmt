#ifndef TEST_C_H
#define TEST_C_H

///////////////////////////////////////////////////////////////////////////////////////////////////
// includes
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "../src/fpfmt_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// test helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static inline double safeUnpackPack(double f) {
	// caller handles special cases per paper
	if (f == 0.0) return f;
	if (isinf(f)) return f;
	if (isnan(f)) return f;

	FloatRep fr = unpack64(f);
	return pack64(fr.m >> 11, fr.e + 11); // caller provides 53-bit mantissa per paper
}

static inline void unrounded_string(Unrounded u, char* buf, size_t bufsize) {
	uint64_t n = u.val >> 2;
	uint64_t h = ((u.val >> 1) & 1) ? 5 : 0;
	snprintf(buf, bufsize, "\xe2\x9f\xa8%llu.%llu%s\xe2\x9f\xa9", (unsigned long long)n, (unsigned long long)h, (u.val & 1) ? "+" : "");
}

typedef struct {
	int n;
	double f;
	uint64_t d;
	int p;
} IvyTest;

static bool parse_ivy_line(const char* line, IvyTest* out) {
	if (line[0] != '(') return false;

	int n, p;
	unsigned long long d;
	char fbuf[128];
	if (sscanf(line, "(%d ftoa %127[^)]) is %llu %d", &n, fbuf, &d, &p) != 4) return false;

	out->n = n;
	out->f = strtod(fbuf, NULL);
	out->d = (uint64_t)d;
	out->p = p;
	return true;
}

// caller owns the returned buffer and must free() it
static inline IvyTest* load_ivy_tests(const char* path, size_t* out_count) {
	*out_count = 0;

	FILE* file = fopen(path, "r");
	if (!file) {
		printf("ERROR: could not open %s\n", path);
		return NULL;
	}

	size_t cap = 1024, count = 0;
	IvyTest* tests = malloc(cap * sizeof(IvyTest));
	char line[256];
	while (fgets(line, sizeof(line), file)) {
		IvyTest t;
		if (!parse_ivy_line(line, &t)) continue;
		if (count == cap) {
			cap *= 2;
			tests = realloc(tests, cap * sizeof(IvyTest));
		}
		tests[count++] = t;
	}
	fclose(file);

	*out_count = count;
	return tests;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// small open-addressing set of doubles (by bit pattern), used to dedup test inputs
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	uint64_t* keys;
	bool* occupied;
	size_t cap;
} DoubleSet;

static inline void doubleset_init(DoubleSet* s, size_t cap) {
	s->cap = cap;
	s->keys = calloc(cap, sizeof(uint64_t));
	s->occupied = calloc(cap, sizeof(bool));
}

static inline void doubleset_free(DoubleSet* s) {
	free(s->keys);
	free(s->occupied);
}

static inline uint64_t doubleset_hash(uint64_t x) {
	x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 33;
	return x;
}

// returns true if already present; inserts f otherwise
static inline bool doubleset_seen(DoubleSet* s, double f) {
	uint64_t key = float64bits(f);
	size_t idx = (size_t)(doubleset_hash(key) % s->cap);
	while (s->occupied[idx]) {
		if (s->keys[idx] == key) return true;
		idx = (idx + 1) % s->cap;
	}
	s->keys[idx] = key;
	s->occupied[idx] = true;
	return false;
}

#endif
