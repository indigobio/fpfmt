// Derived from Russ Cox's fpfmt (BSD-3-Clause). See LICENSE.md.

#include "fpfmt_c.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Go helper function equivalents
///////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t float64bits(double f) {
	uint64_t b;
	memcpy(&b, &f, 8);
	return b;
}

double float64frombits(uint64_t b) {
	double f;
	memcpy(&f, &b, 8);
	return f;
}

// bit length of x (0 for x == 0); intrinsic where available, else portable de Bruijn fallback
#if defined(_M_X64) && defined(_MSC_VER)
#include <intrin.h>
int bits_len64(uint64_t x) {
	unsigned long index;
	if (!_BitScanReverse64(&index, x)) return 0;
	return (int)index + 1;
}
#elif defined(__GNUC__) || defined(__clang__)
int bits_len64(uint64_t x) {
	if (x == 0) return 0;
	return 64 - __builtin_clzll(x);
}
#else
static const int debruijn64[64] = {
   0, 47,  1, 56, 48, 27,  2, 60, 57, 49, 41, 37, 28, 16,  3, 61,
  54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11,  4, 62,
  46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
  25, 39, 14, 33, 19, 30,  9, 24, 13, 18,  8, 12,  7,  6,  5, 63
};
int bits_len64(uint64_t x) {
  if (x == 0) return 0;
  x |= x>>1; x |= x>>2; x |= x>>4; x |= x>>8; x |= x>>16; x |= x>>32;
  return debruijn64[(x * 0x03f79d71b4cb0a89ULL) >> 58] + 1;
}
#endif

uint64_t rotateRight64(uint64_t x, int s) { return ((x >> s) | (x << (64 - s))); }

// 64x64->128 split multiply, high 64 bits; intrinsic/__int128 where available, else portable fallback
#if defined(_M_X64) && defined(_MSC_VER)
#include <intrin.h>
static uint64_t mulhi64(uint64_t a, uint64_t b) {
	uint64_t hi;
	_umul128(a, b, &hi);
	return hi;
}
#elif defined(__SIZEOF_INT128__) && ( \
	(defined(__clang__) && !defined(_MSC_VER)) || \
	(defined(__GNUC__) && !defined(__clang__) && !defined(__CUDACC__)))
static uint64_t mulhi64(uint64_t a, uint64_t b) {
	return (uint64_t)(((unsigned __int128)a * b) >> 64);
}
#else
static uint64_t mulhi64(uint64_t a, uint64_t b) {
	uint64_t a_lo = (uint32_t)a, a_hi = a >> 32;
	uint64_t b_lo = (uint32_t)b, b_hi = b >> 32;

	uint64_t lo_lo = a_lo * b_lo;
	uint64_t hi_lo = a_hi * b_lo;
	uint64_t lo_hi = a_lo * b_hi;
	uint64_t hi_hi = a_hi * b_hi;

	uint64_t cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFFu) + (lo_hi & 0xFFFFFFFFu);
	return hi_hi + (hi_lo >> 32) + (lo_hi >> 32) + (cross >> 32);
}
#endif

// arithmetic right shift on int32_t, portable since shifting a negative int is impl-defined; valid for 1 <= n <= 31
static int32_t ashr32(int32_t x, int n) {
	uint32_t ux = (uint32_t)x;
	uint32_t shifted = ux >> n;
	if (x < 0) shifted |= (uint32_t)(~0u << (32 - n));
	return (int32_t)shifted;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt utilities
///////////////////////////////////////////////////////////////////////////////////////////////////
FloatRep unpack64(double f) {
	const int shift = 64 - 53; // = 11
	const int minExp = -(1074 + shift); // = -1085, used for compensating e for shift in m

	// extract bits from incoming
	uint64_t b = float64bits(f);

	// isolate stored 52-bit m, left-justify it to 64-bits, restore implicit leading 1
	uint64_t m = (1ULL << 63) | ((b & ((1ULL << 52) - 1)) << shift);

	// get raw stored exponent
	int e = (int)((b >> 52) & ((1ULL << shift) - 1));

	// subnormal handling
	if (e == 0) {
		// clear implicit leading 1
		m &= ~(1ULL << 63);

		// set e to left-justified form
		e = minExp;

		// renormalizes by shifting m until it's left-justified, updating e to compensate
		int s = 64 - bits_len64(m);
		return (FloatRep){ m << s, e - s };
	}

	// normal path, unbias and compensate e for shift in m
	return (FloatRep){ m, (e - 1) + minExp };
}

double pack64(uint64_t m, int e) {
	// implicit leading 1 not present, already packed
	if ((m & (1ULL << 52)) == 0) return float64frombits(m);

	// implicit leading 1 present, must clear it and construct biased e
	return float64frombits((m & ~(1ULL << 52)) | ((uint64_t)(1075 + e) << 52));
}

Unrounded unround(double x) {
	// shift h and s to integer range
	double f = 4.0 * x;

	// extract integer, low 2 bits encode half bit
	double fr = floor(f);

	// set sticky if there's a remainder
	return unrounded_or(unrounded_make((uint64_t)fr), unrounded_from_bool(fr != f));
}

int log10Pow2(int x) { return ashr32((int32_t)((int64_t)x * 78913), 18); }
int log2Pow10(int x) { return ashr32((int32_t)((int64_t)x * 108853), 15); }

Scaler prescale(int e, int p, int lp) {
	// make sure p is within range of precomputed table
	if (p < pow10Min || p > pow10Max) abort();

	uint64_t* res = pow10Tab[p - pow10Min];
	return (Scaler){ { res[0], res[1] }, -(e + lp + 3) };
}

Unrounded uscale(uint64_t x, Scaler c) {
	// multiply for high 128 bits (split into hi/lo 64-bit halves)
	uint64_t hi = mulhi64(x, c.pm.hi); // high 64 bits
	uint64_t mid = x * c.pm.hi;        // low 64 bits, for sticky correction (wraps mod 2^64 -- well defined)

	// default sticky
	uint64_t sticky = 1;

	// if low bits of hi are zero, hi alone can't determine sticky, use pmLo to correct
	if ((hi & ((1ULL << c.s) - 1)) == 0) {
		// multiply for low 64 bits
		uint64_t low = mulhi64(x, c.pm.lo);

		// nonzero bits means result is inexact
		sticky = (mid - low > 1);

		// if low > mid underflows, borrow from hi
		hi -= (mid < low);
	}

	// shift hi to result position, include sticky bit
	return unrounded_make((hi >> c.s) | sticky);
}

Unrounded unmin(uint64_t x) { return unrounded_make((x << 2) - 2); }

int skewed(int e) { return ashr32((int32_t)((int64_t)e * 631305 - 261663), 21); } // approximate of floor(e*log_10(2) - (log_10(4/3))

FloatRep trimZeros(FloatRep fr) {
	// precomputed divisions
	const uint64_t maxUint64 = ~0ULL;
	const uint64_t div1e8m = 0xc767074b22e90e21ULL;
	const uint64_t div1e4m = 0xd288ce703afb7e91ULL;
	const uint64_t div1e2m = 0x8f5c28f5c28f5c29ULL;
	const uint64_t div1e1m = 0xcccccccccccccccdULL;
	const uint64_t div1e8le = maxUint64 / 100000000;
	const uint64_t div1e4le = maxUint64 / 10000;
	const uint64_t div1e2le = maxUint64 / 100;
	const uint64_t div1e1le = maxUint64 / 10;

	uint64_t x = fr.m;
	int p = fr.e;

	uint64_t d;

	// try to cut 1 zero, or return.
	if ((d = rotateRight64(x * div1e1m, 1)) <= div1e1le) {
		x = d;
		p++;
	} else return (FloatRep){ x, p };

	// try to cut 8 zeros, then 4, then 2, then 1
	if ((d = rotateRight64(x * div1e8m, 8)) <= div1e8le) {
		x = d;
		p += 8;
	}
	if ((d = rotateRight64(x * div1e4m, 4)) <= div1e4le) {
		x = d;
		p += 4;
	}
	if ((d = rotateRight64(x * div1e2m, 2)) <= div1e2le) {
		x = d;
		p += 2;
	}
	if ((d = rotateRight64(x * div1e1m, 1)) <= div1e1le) {
		x = d;
		p += 1;
	}

	return (FloatRep){ x, p };
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt formatting
///////////////////////////////////////////////////////////////////////////////////////////////////
int digits(uint64_t d) {
	// approximate digit count from bit length, then correct for undershoot
	int n = log10Pow2(bits_len64(d));
	return n + (d >= uint64pow10[n]);
}

void formatBase10(char* a, uint64_t d, int n) {
	// peel off 8 digits at a time via the i2a lookup table, right to left
	while ((d >> 32) != 0) {
		uint32_t x = (uint32_t)(d % 100000000);
		d /= 100000000;
		uint32_t y = x % 10000;
		x /= 10000;
		uint32_t x1 = (x / 100) * 2;
		uint32_t x0 = (x % 100) * 2;
		uint32_t y1 = (y / 100) * 2;
		uint32_t y0 = (y % 100) * 2;
		memcpy(a + n - 8, i2a + x1, 2);
		memcpy(a + n - 6, i2a + x0, 2);
		memcpy(a + n - 4, i2a + y1, 2);
		memcpy(a + n - 2, i2a + y0, 2);
		n -= 8;
	}

	// remaining digits fit in 32 bits; peel off 4, then 2, then the last 1-2 by hand
	uint32_t h = (uint32_t)d;
	while (h >= 10000) {
		uint32_t x = h % 10000;
		h /= 10000;
		uint32_t x1 = (x / 100) * 2;
		uint32_t x0 = (x % 100) * 2;
		memcpy(a + n - 4, i2a + x1, 2);
		memcpy(a + n - 2, i2a + x0, 2);
		n -= 4;
	}
	if (h >= 100) {
		uint32_t x = h % 100;
		h /= 100;
		memcpy(a + n - 2, i2a + 2 * x, 2);
		n -= 2;
	}
	if (h >= 10) {
		memcpy(a + n - 2, i2a + 2 * h, 2);
		return;
	}
	a[n - 1] = (char)('0' + h);
}

int fmt(char* s, uint64_t d, int p, int n) {
	// write digits into s[1..n], then move the leading digit into s[0] for the decimal point
	formatBase10(s + 1, d, n);
	p += n - 1;

	s[0] = s[1];
	int i = n;
	if (i > 1) {
		s[1] = '.';
		i++;
	}

	s[i] = 'e';
	if (p < 0) {
		s[i + 1] = '-';
		p = -p;
	} else s[i + 1] = '+';

	if (p < 100) {
		s[i + 2] = i2a[p * 2];
		s[i + 3] = i2a[p * 2 + 1];
		return i + 4;
	}
	s[i + 2] = (char)('0' + p / 100);
	s[i + 3] = i2a[(p % 100) * 2];
	s[i + 4] = i2a[(p % 100) * 2 + 1];
	return i + 5;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt main methods
///////////////////////////////////////////////////////////////////////////////////////////////////
// TO-DO: check if n = 0 is a problem
FloatRep fixedWidth(double f, int n) {
	// checks for reasonable number of decimal digits requested
	if (n > 18) abort();

	// unpack mantissa and exponent from double
	FloatRep fr = unpack64(f);
	uint64_t m = fr.m;
	int e = fr.e;

	// find the power of 10 that scales f so d = f * 10^p is an n-digit integer
	int p = n - 1 - log10Pow2(e + 63);

	// compute m * 2^e * 10^p to scale base 2 to base 10
	Unrounded u = uscale(m, prescale(e, p, log2Pow10(p)));

	// round scaled value to nearest integer
	uint64_t d = unrounded_round(u);

	// if p is off by one because of approximation or rollover, divide by 10 and compensate p
	if (d >= uint64pow10[n]) {
		d = unrounded_round(unrounded_div(u, 10));
		p--;
	}
	return (FloatRep){ d, -p };
}

double parse(uint64_t d, int p) {
	// checks if d fits in 19 digit limit
	if (d > uint64pow10[19]) abort();

	// get bit length of d
	int b = bits_len64(d);

	// cache for speed
	int lp = log2Pow10(p);

	// approximate exponent to scale d * 10^p into 53-bit integer range. cap at 1074 to handle subnormals
	int e = 53 - b - lp;
	if (e > 1074) e = 1074;

	// compute m * 2^e * 10^p after left-justifying d and compensating e respectively
	Unrounded u = uscale(d << (64 - b), prescale(e - (64 - b), p, lp));

	// adjust u and p if approximation was off by one
	int adj = unrounded_ge(u, unmin(1ULL << 53)) ? 1 : 0;
	u = unrounded_or(unrounded_shr(u, adj), unrounded_and(u, unrounded_make(1)));
	e -= adj;

	// round scaled value to nearest integer
	return pack64(unrounded_round(u), -e);
}

bool ci_equals(const char* s, const char* end, const char* lit) {
	for (; *lit; lit++, s++) {
		if (s == end) return false;
		char c = *s;
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		if (c != *lit) return false;
	}
	return s == end;
}

double parseText(const char* start, const char* end) {
	// optional leading sign
	bool neg = false;
	if (start < end && (*start == '-' || *start == '+')) {
		neg = (*start == '-');
		start++;
	}

	if (ci_equals(start, end, "nan")) {
		const uint64_t qnan_bits = 0x7FF8000000000000ULL;
		const uint64_t sign_bit  = 1ULL << 63;
		return float64frombits(neg ? (qnan_bits | sign_bit) : qnan_bits);
	}
	if (ci_equals(start, end, "inf") || ci_equals(start, end, "infinity")) {
		const uint64_t inf_bits = 0x7FF0000000000000ULL;
		const uint64_t sign_bit = 1ULL << 63;
		return float64frombits(neg ? (inf_bits | sign_bit) : inf_bits);
	}

	// accumulate digits into d; frac counts fractional digits to fold into p below.
	// caps at 19 sig digits to avoid overflowing d; extra int digits widen the
	// exponent instead (extraIntDigits). leading zeros are free.
	uint64_t d = 0;
	int ndigits = 0;
	int extraIntDigits = 0;
	int frac = 0;
	const char* s = start;
	for (; s < end && *s >= '0' && *s <= '9'; s++) {
		int digit = *s - '0';
		if (ndigits < 19) {
			if (ndigits > 0 || digit != 0) { d = d * 10 + (uint64_t)digit; ndigits++; }
		} else extraIntDigits++;
	}

	if (s < end && *s == '.') {
		s++;
		for (; s < end && *s >= '0' && *s <= '9'; s++) {
			int digit = *s - '0';
			if (ndigits < 19) {
				if (ndigits > 0 || digit != 0) { d = d * 10 + (uint64_t)digit; ndigits++; }
				frac++;
			}
		}
	}

	// optional exponent
	int p = 0;
	if (s < end && (*s == 'e' || *s == 'E')) {
		s++;
		int esign = 1;
		if (s < end && (*s == '-' || *s == '+')) {
			esign = (*s == '-') ? -1 : 1;
			s++;
		}
		for (; s < end && *s >= '0' && *s <= '9'; s++) p = p * 10 + (*s - '0');
		p *= esign;
	}

	if (d == 0) return neg ? -0.0 : 0.0;

	// subtract frac, add extraIntDigits back into the exponent
	double m = parse(d, p - frac + extraIntDigits);
	return neg ? -m : m;
}

FloatRep shortWidth(double f) {
	const int minExp = -1085;

	FloatRep fr = unpack64(f);
	uint64_t m = fr.m;
	int e = fr.e;

	// find [min, max), the interval that all rounds back to f; z=11 is unpack64's shift, so half a ULP is 1<<(z-1)
	uint64_t min;
	int p;
	int z = 11;
	if ((m == (1ULL << 63)) && (e > minExp)) {
		// smallest normalized mantissa: interval below m is only a quarter-ULP, use skewed() for the asymmetry
		p = -skewed(e + z);
		min = m - (1ULL << (z - 2));
	} else {
		if (e < minExp) z = 11 + (minExp - e); // e clamped for subnormals, widen z so min doesn't underflow
		p = -log10Pow2(e + z);
		min = m - (1ULL << (z - 1));
	}

	uint64_t max = m + (1ULL << (z - 1));
	int odd = (int)(m >> z) & 1; // nudges boundary inward on ties, per round-to-even

	// scale interval to base 10, rounding inward so [dmin, dmax] still rounds back to f
	Scaler pre = prescale(e, p, log2Pow10(p));
	uint64_t dmin = unrounded_ceil(unrounded_nudge(uscale(min, pre), +odd));
	uint64_t dmax = unrounded_floor(unrounded_nudge(uscale(max, pre), -odd));

	// prefer one fewer digit if truncating dmax still lands in [dmin, dmax]
	uint64_t d = dmax / 10;
	if ((d * 10) >= dmin) return trimZeros((FloatRep){ d, -(p - 1) });

	// otherwise fall back to dmin, or the value nearest m if more than one candidate remains
	d = dmin;
	if (d < dmax) d = unrounded_round(uscale(m, pre));
	return (FloatRep){ d, -p };
}

int format(char* s, double f, int n) {
	// handle NaN
	if (isnan(f)) {
		memcpy(s, "NaN", 3);
		return 3;
	}

	// handle sign
	int offset = 0;
	if (signbit(f)) {
		s[offset++] = '-';
		f = -f;
	}

	// handle infinities
	if (isinf(f)) {
		memcpy(s + offset, "inf", 3);
		return offset + 3;
	}

	// handle zero -- fmt()/formatBase10 assume a nonzero d, so build the digit
	// string by hand instead of routing 0 through fixedWidth/shortWidth
	if (f == 0.0) {
		if (n <= 0) {
			memcpy(s + offset, "0.0", 3);
			return offset + 3;
		}
		int i = 1;
		s[offset] = '0';
		if (n > 1) {
			s[offset + 1] = '.';
			memset(s + offset + 2, '0', (size_t)(n - 1));
			i = n + 1;
		}
		memcpy(s + offset + i, "e+00", 4);
		return offset + i + 4;
	}

	if (n > 0) {
		FloatRep fr = fixedWidth(f, n);
		return offset + fmt(s + offset, fr.m, fr.e, n);
	}

	FloatRep fr = shortWidth(f);
	return offset + fmt(s + offset, fr.m, fr.e, digits(fr.m));
}