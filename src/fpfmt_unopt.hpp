// Derived from Russ Cox's fpfmt (BSD-3-Clause). See LICENSE.md.

#ifndef FPFMT_UNOPT_HPP
#define FPFMT_UNOPT_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
// includes
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>
#include "pow10.hpp"

namespace unopt {

	using uint128_t = __uint128_t;

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// Go helper function equivalents
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// math.Float64bits
	uint64_t float64bits(double f);

	// math.Float64frombits
	double float64frombits(uint64_t b);

	// bits.Len64
	int bits_len64(uint64_t x);

	// converts boolean to an integer
	template<typename T>
	T bool2(bool b) { return b ? T{1} : T(0); }

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// structs
	///////////////////////////////////////////////////////////////////////////////////////////////////
	struct FloatRep {
		uint64_t m;
		int e;
	};

	struct Scalers {
		uint64_t pmHi;
		uint64_t pmLo;
		int s;
	};

	struct Unrounded {
		// member
		uint64_t val = 0;

		// constructor
		explicit Unrounded(uint64_t v): val(v) {};

		// operators
		Unrounded operator|(Unrounded o)  const { return Unrounded(val | o.val); }
		Unrounded operator&(Unrounded o)  const { return Unrounded(val & o.val); }
		Unrounded operator+(Unrounded o)  const { return Unrounded(val + o.val); }
		Unrounded operator-(Unrounded o)  const { return Unrounded(val - o.val); }
		Unrounded operator>>(int s)       const { return Unrounded(val >> s); }
		bool      operator>=(Unrounded o) const { return val >= o.val; } 

		// methods
		uint64_t  floor()         const { return (val + 0) >> 2; }
		uint64_t  roundHalfDown() const { return (val + 1) >> 2; }
		uint64_t  round()         const { return (val + 1 + ((val >> 2) & 1)) >> 2; }
		uint64_t  roundHalfUp()   const { return (val + 2) >> 2; }
		uint64_t  ceil()          const { return (val + 3) >> 2; }
		Unrounded div(uint64_t d) const { return Unrounded(val / d) | (*this & Unrounded(1)) | bool2<Unrounded>(val % d != 0); } // integer div | existing sticky bit | remainder
		Unrounded rsh(int s)      const { return (*this >> s) | (*this & Unrounded(1)) | bool2<Unrounded>((val & ((1ULL << s) - 1)) != 0); } // integer shift | existing sticky bit | any bits shifted out were nonzero
		Unrounded nudge(int d)    const { return Unrounded(val + uint64_t(d)); }
	};

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
	Scalers prescale(int e, int p, int lp);
	Unrounded uscale(uint64_t x, Scalers c);

	// returns minimum Unrounded value that rounds to x
	Unrounded unmin(uint64_t x);

	// computes the skewed footprint of m * 2^e
	int skewed(int e);

	// removes trailing zeros from x * 10^p, assuming x ends in at most 16 zeros
	FloatRep trimZeros(FloatRep fr);

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// fpfmt main methods
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// returns n-digit decimal form of f as d * 10^p where n <= 18
	FloatRep fixedWidth(double f, int n);

	// rounds d * 10^p to the nearest float64 where d can have at most 19 digits
	double parse(uint64_t d, int p);

	// parses text -- optional sign, "nan"/"inf"/"infinity" (case-insensitive), "0",
	// or finite nonzero digits with an optional '.' and exponent -- into a double.
	// Mirrors fpfmt::parseText().
	double parseText(const char* start, const char* end);

	// computes shortest formatting of f that will still roundtrip back to original double
	FloatRep shortWidth(double f);

	// formats any double -- sign, zero, infinity, NaN, or finite nonzero -- into s.
	// n <= 0 (default) uses shortWidth's shortest digit count; n > 0 (<= 18) uses
	// fixedWidth's exact n-digit form instead. Returns bytes written, not
	// nul-terminated, same convention as fmt(). Mirrors fpfmt::format().
	int format(char* s, double f, int n = 0);

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// fpfmt formatting
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// digit to char lookup table
	constexpr char i2a[] =
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

}

#endif