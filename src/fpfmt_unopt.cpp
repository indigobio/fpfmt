// Derived from Russ Cox's fpfmt (BSD-3-Clause). See LICENSE.md.

#include "fpfmt_unopt.hpp"

namespace unopt {
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

	int bits_len64(uint64_t x) {
		if (x == 0) return 0;
		return 64 - __builtin_clzll(x);
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
		int e = int((b >> 52) & ((1ULL << shift) - 1));

		// subnormal handling
		if (e == 0) {
			// clear implicit leading 1
			m &= ~(1ULL << 63);

			// set e to left-justified form
			e = minExp;

			// renormalizes by shifting m until it's left-justified, updating e to compensate
			int s = 64 - bits_len64(m);
			return { m << s, e - s };
		}

		// normal path, unbias and compensate e for shift in m
		return { m, (e - 1) + minExp };
	}

	double pack64(uint64_t m, int e) {
		// implicit leading 1 not present, already packed
		if ((m & (1ULL << 52)) == 0) return float64frombits(m);

		// implicit leading 1 present, must clear it and construct biased e
		return float64frombits((m & ~(1ULL << 52)) | (uint64_t(1075 + e) << 52));
	}

	Unrounded unround(double x) {
		// shift h and s to integer range
		double f = 4.0 * x;

		// extract integer, low 2 bits encode half bit
		double fr = floor(f);

		// set sticky if there's a remainder
		return Unrounded(uint64_t(fr)) | bool2<Unrounded>(fr != f);
	}

	int log10Pow2(int x) { return (x * 78913)  >> 18; } // approximate of floor(x*log_10(2))
	int log2Pow10(int x) { return (x * 108853) >> 15; } // approximate of floor(x*log_2(10))

	Scalers prescale(int e, int p, int lp) {
		// make sure p is within range of precomputed table
		if (p < pow10Min || p > pow10Max) std::abort(); 

		// shift count, negated to shift right
		int s = -(e + lp + 3); // e + log2(10^p) + 3 for Unrounded representation

		// bias p for table indexing
		p -= pow10Min;

		return { pow10Tab[p][0], pow10Tab[p][1], s }; // pmHi, pmLo, s
	}

	Unrounded uscale(uint64_t x, Scalers c) {
		// multiply for high 128 bits
		uint128_t full = (uint128_t)x * c.pmHi;
		uint64_t hi = full >> 64; // high 64 bits
		uint64_t mid = full; // mid 64 bits for sticky correction

		// assign mid 64 bits and default sticky
		uint64_t sticky = 1;

		// if low bits of hi are zero, hi alone can't determine sticky, use pmLo to correct
		if ((hi & ((1ULL << c.s) - 1)) == 0) {
			// multiply for low 64 bits
			uint64_t low = ((uint128_t)x * c.pmLo) >> 64;

			// nonzero bits means result is inexact
			sticky = (mid - low > 1);

			// if low > mid underflows, borrow from hi
			hi -= mid < low;
		}

		// shift hi to result position, include sticky bit
		return Unrounded((hi >> c.s) | sticky);
	}

	Unrounded unmin(uint64_t x) { return Unrounded((x << 2) - 2); }

	int skewed(int e) { return ((e * 631305) - 261663) >> 21; } // approximate of floor(e*log_10(2) - (log_10(4/3))

	// naive divmod version of fpfmt::trimZeros's reciprocal-multiply trick
	FloatRep trimZeros(FloatRep fr) {
		uint64_t x = fr.m;
		int p = fr.e;

		// try to cut 1 zero, or return
		if (x % 10 != 0) {
			return { x, p };
		}
		x /= 10;
		p += 1;

		// try to cut 8 zeros, then 4, then 2, then 1
		if (x % 100000000 == 0) {
			x /= 100000000;
			p += 8;
		}
		if (x % 10000 == 0) {
			x /= 10000;
			p += 4;
		}
		if (x % 100 == 0) {
			x /= 100;
			p += 2;
		}
		if (x % 10 == 0) {
			x /= 10;
			p += 1;
		}

		return { x, p };
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////
	// fpfmt main methods
	///////////////////////////////////////////////////////////////////////////////////////////////////
	FloatRep fixedWidth(double f, int n) {
		// checks for reasonable number of decimal digits requested
		if (n > 18) std::abort();

		// unpack mantissa and exponent from double
		FloatRep fr = unpack64(f);
		uint64_t m = fr.m;
		int e = fr.e;

		// find the power of 10 that scales f so d = f * 10^p is an n-digit integer
		int p = n - 1 - log10Pow2(e + 63);
		
		// compute m * 2^e * 10^p to scale base 2 to base 10
		Unrounded u = uscale(m, prescale(e, p, log2Pow10(p)));

		// round scaled value to nearest integer
		uint64_t d = u.round();

		// if p is off by one because of approximation or rollover, divide by 10 and compensate p
		if (d >= uint64pow10[n]) {
			d = u.div(10).round();
			p--;
		}
		return { d, -p };
	}

	double parse(uint64_t d, int p) {
		// checks if d fits in 19 digit limit
		if (d > uint64pow10[19]) std::abort();

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
		int adj = bool2<int>(u >= unmin(1ULL << 53));
		u = (u >> adj) | (u & Unrounded(1));
		e -= adj;

		// round scaled value to nearest integer
		uint64_t m = u.round();

		return pack64(m, -e);
	}

	// true if [s, end] equals literal (case insensitive). doesn't check for null termination, caller must handle range appropriately
	static bool ci_equals(const char* s, const char* end, const char* lit) {
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

		if (ci_equals(start, end, "nan")) return neg ? -std::nan("") : std::nan("");
		if (ci_equals(start, end, "inf") || ci_equals(start, end, "infinity")) return neg ? -HUGE_VAL : HUGE_VAL;

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
				if (ndigits > 0 || digit != 0) d = d * 10 + (uint64_t)digit; ndigits++;
			} else extraIntDigits++;
		}

		if (s < end && *s == '.') {
			s++;
			for (; s < end && *s >= '0' && *s <= '9'; s++) {
				int digit = *s - '0';
				if (ndigits < 19) {
					if (ndigits > 0 || digit != 0) d = d * 10 + (uint64_t)digit; ndigits++;
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
		Scalers pre = prescale(e, p, log2Pow10(p));
		uint64_t dmin = uscale(min, pre).nudge(+odd).ceil();
		uint64_t dmax = uscale(max, pre).nudge(-odd).floor();

		// prefer one fewer digit if truncating dmax still lands in [dmin, dmax]
		uint64_t d = dmax / 10;
		if ((d * 10) >= dmin) return trimZeros({ d, -(p - 1) });

		// otherwise fall back to dmin, or the value nearest m if more than one candidate remains
		d = dmin;
		if (d < dmax) d = uscale(m, pre).round();
		return { d, -p };
	}

	int format(char* s, double f, int n) {
		// handle NaN
		if (std::isnan(f)) {
			memcpy(s, "NaN", 3);
			return 3;
		}

		// handle sign
		int offset = 0;
		if (std::signbit(f)) {
			s[offset++] = '-';
			f = -f;
		}

		// handle infinities
		if (std::isinf(f)) {
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
				memset(s + offset + 2, '0', n - 1);
				i = n + 1;
			}
			memcpy(s + offset + i, "e+00", 4);
			return offset + i + 4;
		}

		if (n > 0) {
			auto [d, p] = fixedWidth(f, n);
			return offset + fmt(s + offset, d, p, n);
		}

		auto [d, p] = shortWidth(f);
		return offset + fmt(s + offset, d, p, digits(d));
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
			uint32_t x = d % 100000000;
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
		uint32_t h = d;
		while(h >= 10000) {
			uint32_t x = h % 10000;
			h /= 10000;
			uint32_t x1 = (x / 100) * 2;
			uint32_t x0 = (x % 100) * 2;
			memcpy(a + n - 4, i2a + x1, 2);
			memcpy(a + n - 2, i2a + x0, 2);
			n -= 4;
		}
		if(h >= 100) {
			uint32_t x = h % 100;
			h /= 100;
			memcpy(a + n - 2, i2a + 2 * x, 2);
			n -= 2;
		}
		if(h >= 10) {
			memcpy(a + n - 2, i2a + 2 * h, 2);
			return;
		}
		a[n - 1] = '0' + h;
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

		// append exponent: sign, then up to 3 digits via i2a
		s[i] = 'e';
		if (p < 0){
			s[i + 1] = '-';
			p = -p;
		} else s[i + 1] = '+';

		if (p < 100) {
			s[i + 2] = i2a[p * 2];
			s[i + 3] = i2a[p * 2 + 1];
			return i + 4;
		}
		s[i + 2] = '0' + p / 100;
		s[i + 3] = i2a[(p % 100) * 2];
		s[i + 4] = i2a[(p % 100) * 2 + 1];
		return i + 5;
	}

}