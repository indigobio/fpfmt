// Derived from Russ Cox's fpfmt (BSD-3-Clause). See LICENSE.md.

#ifndef FPFMT_H
#define FPFMT_H

///////////////////////////////////////////////////////////////////////////////////////////////////
// includes
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "pow10.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Go helper function equivalents
///////////////////////////////////////////////////////////////////////////////////////////////////
// math.Float64bits
uint64_t float64bits(double f);

// math.Float64frombits
double float64frombits(uint64_t b);

// bits.Len64 (portable replacement for __builtin_clzll -- see fpfmt.c)
int bits_len64(uint64_t x);

uint64_t rotateRight64(uint64_t x, int s);

///////////////////////////////////////////////////////////////////////////////////////////////////
// structs
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	uint64_t m;
	int e;
} FloatRep;

typedef struct {
	uint64_t hi;
	uint64_t lo;
} PmHiLo;

typedef struct {
	PmHiLo pm;
	int s;
} Scaler;

typedef struct {
	uint64_t val;
} Unrounded;

// utilities
static inline Unrounded unrounded_make(uint64_t v) { Unrounded u; u.val = v; return u; }
static inline Unrounded unrounded_from_bool(bool b) { return unrounded_make(b ? 1u : 0u); }

// operators
static inline Unrounded unrounded_or(Unrounded a, Unrounded b)  { return unrounded_make(a.val | b.val); }
static inline Unrounded unrounded_and(Unrounded a, Unrounded b) { return unrounded_make(a.val & b.val); }
static inline Unrounded unrounded_add(Unrounded a, Unrounded b) { return unrounded_make(a.val + b.val); }
static inline Unrounded unrounded_sub(Unrounded a, Unrounded b) { return unrounded_make(a.val - b.val); }
static inline Unrounded unrounded_shr(Unrounded a, int s)       { return unrounded_make(a.val >> s); }
static inline bool      unrounded_ge(Unrounded a, Unrounded b)  { return a.val >= b.val; }

// methods
static inline uint64_t unrounded_floor(Unrounded u)           { return (u.val + 0) >> 2; }
static inline uint64_t unrounded_round_half_down(Unrounded u) { return (u.val + 1) >> 2; }
static inline uint64_t unrounded_round(Unrounded u)           { return (u.val + 1 + ((u.val >> 2) & 1)) >> 2; }
static inline uint64_t unrounded_round_half_up(Unrounded u)   { return (u.val + 2) >> 2; }
static inline uint64_t unrounded_ceil(Unrounded u)            { return (u.val + 3) >> 2; }

// integer div | existing sticky bit | remainder
static inline Unrounded unrounded_div(Unrounded u, uint64_t d) {
	Unrounded q = unrounded_make(u.val / d);
	Unrounded existing_sticky = unrounded_and(u, unrounded_make(1));
	Unrounded new_sticky = unrounded_from_bool((u.val % d) != 0);
	return unrounded_or(unrounded_or(q, existing_sticky), new_sticky);
}

// integer shift | existing sticky bit | any bits shifted out were nonzero
static inline Unrounded unrounded_rsh(Unrounded u, int s) {
	Unrounded shifted = unrounded_shr(u, s);
	Unrounded existing_sticky = unrounded_and(u, unrounded_make(1));
	Unrounded new_sticky = unrounded_from_bool((u.val & ((1ULL << s) - 1)) != 0);
	return unrounded_or(unrounded_or(shifted, existing_sticky), new_sticky);
}

static inline Unrounded unrounded_nudge(Unrounded u, int d) {
	return unrounded_make(u.val + (uint64_t)d);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt utilities
///////////////////////////////////////////////////////////////////////////////////////////////////
// expects caller to handle sign, 0, NaN, and inf before calling
FloatRep unpack64(double f);

// expects 53-bit right-justified m and e in range for m
double pack64(uint64_t m, int e);

// unrounds a float64
Unrounded unround(double x);

// base 10 log functions
int log10Pow2(int x);
int log2Pow10(int x);

// fixed width helpers
Scaler prescale(int e, int p, int lp);
Unrounded uscale(uint64_t x, Scaler c);

// returns minimum unrounded value that rounds to x
Unrounded unmin(uint64_t x);

// computes the skewed footprint of m * 2^e
int skewed(int e);

// removes trailing zeros from x * 10^p, assuming x ends in at most 16 zeros
FloatRep trimZeros(FloatRep fr);

// true if [s, end] equals literal (case insensitive). doesn't check for null termination, caller must handle range appropriately
bool ci_equals(const char* s, const char* end, const char* lit);

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt main methods
///////////////////////////////////////////////////////////////////////////////////////////////////
// returns n-digit decimal form of f as d * 10^p where n <= 18
FloatRep fixedWidth(double f, int n);

// rounds d * 10^p to the nearest float64 where d can have at most 19 digits
double parse(uint64_t d, int p);

// parses text - optional sign, "nan"/"inf"/"infinity" (case-insensitive), "0", optional '.' and exponent
double parseText(const char* start, const char* end);

// computes shortest formatting of f that will still roundtrip back to original double
FloatRep shortWidth(double f);

// formats any double - sign, zero, infinity, NaN, or finite nonzero - into s.
int format(char* s, double f, int n);
static inline int format_shortest(char* s, double f) { return format(s, f, 0); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt formatting
///////////////////////////////////////////////////////////////////////////////////////////////////
// digit to char lookup table
static const char i2a[] =
	"00010203040506070809"
	"10111213141516171819"
	"20212223242526272829"
	"30313233343536373839"
	"40414243444546474849"
	"50515253545556575859"
	"60616263646566676869"
	"70717273747576777879"
	"80818283848586878889"
	"90919293949596979899"
;

// counts decimal digits in d
int digits(uint64_t d);

// formats d into a right to left, leading zeros if needed
void formatBase10(char* a, uint64_t d, int n);

// formats d * 10^p with n digits into s in exponential notation, returns bytes written
int fmt(char* s, uint64_t d, int p, int n);

#endif