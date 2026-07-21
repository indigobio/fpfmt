#!/usr/bin/env python3
import argparse
import math
import random
import struct
import sys
from decimal import Decimal, Context, ROUND_HALF_EVEN
from fractions import Fraction as F

def fixed_width_decimal(f, n):
	if not (math.isfinite(f) and f > 0):
		raise ValueError(f"f must be finite and positive, got {f!r}")

	ctx = Context(prec=n, rounding=ROUND_HALF_EVEN)
	rounded = ctx.create_decimal(Decimal(f))  # rounds DOWN to n digits if there are more
	sign, digits, exponent = rounded.as_tuple()
	assert sign == 0

	if len(digits) < n:
		pad = n - len(digits)
		digits = digits + (0,) * pad
		exponent -= pad

	assert len(digits) == n
	d = int("".join(map(str, digits)))
	p = exponent
	return d, p

def fixed_width_fraction(f, n):
	if not (math.isfinite(f) and f > 0):
		raise ValueError(f"f must be finite and positive, got {f!r}")

	exact = F(f)
	p = math.floor(math.log10(f)) - (n - 1)
	while F(10) ** (p + n - 1) > exact:
		p -= 1
	while F(10) ** (p + n) <= exact:
		p += 1

	scale = F(10) ** (-p)
	scaled = exact * scale
	q, r = divmod(scaled.numerator, scaled.denominator)
	twice_r = 2 * r
	if twice_r > scaled.denominator or (twice_r == scaled.denominator and q % 2 == 1):
		q += 1
	if q >= 10 ** n:
		q //= 10
		p += 1
	return q, p

def selftest(trials=20000, seed=0):
	rng = random.Random(seed)
	checked = 0
	for _ in range(trials):
		bits = rng.getrandbits(64)
		f = struct.unpack("<d", struct.pack("<Q", bits))[0]
		if not (math.isfinite(f) and f > 0):
			continue
		n = rng.randint(1, 19)
		d1, p1 = fixed_width_decimal(f, n)
		d2, p2 = fixed_width_fraction(f, n)
		if (d1, p1) != (d2, p2):
			print(f"MISMATCH f={f!r} n={n}: decimal=({d1},{p1}) fraction=({d2},{p2})", file=sys.stderr)
			return False
		checked += 1
	print(f"selftest OK: {checked} (f, n) pairs, Decimal and Fraction agree")
	return True

def parse_float_arg(s):
	s = s.strip()
	if s.lower().startswith("0x") or s.lower().startswith("-0x"):
		return float.fromhex(s)
	return float(s)

def bits_to_float(bits):
	return struct.unpack("<d", struct.pack("<Q", bits & 0xFFFFFFFFFFFFFFFF))[0]

def random_floats(count, seed, min_exp, max_exp):
	rng = random.Random(seed)
	out = []
	while len(out) < count:
		exp_field = rng.randint(min_exp, max_exp)
		mantissa = rng.getrandbits(52)
		bits = (exp_field << 52) | mantissa
		f = bits_to_float(bits)
		if math.isfinite(f) and f > 0:
			out.append(f)
	return out

def gen_lines(floats, n_min, n_max):
	for f in floats:
		hexfloat = float.hex(f)
		for n in range(n_min, n_max + 1):
			d, p = fixed_width_decimal(f, n)
			yield f"({n} ftoa {hexfloat}) is {d} {p}"

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument("out_file", nargs="?", help="path to append/write generated lines to")
	ap.add_argument("--float", nargs="+", default=[], metavar="F", help="explicit float literal(s) (decimal or 0x hex-float)")
	ap.add_argument("--hex", nargs="+", default=[], metavar="H", help="explicit hex-float literal(s), e.g. 0x1.921fp+1")
	ap.add_argument("--bits", nargs="+", default=[], metavar="B", help="explicit IEEE-754 bit pattern(s) (decimal or 0x)")
	ap.add_argument("--random", type=int, default=0, metavar="N", help="generate N random positive finite doubles")
	ap.add_argument("--seed", type=int, default=0, help="RNG seed for --random (default: 0, for reproducibility)")
	ap.add_argument("--min-exp", type=int, default=1, help="min IEEE biased exponent field for --random (default: 1, smallest normal)")
	ap.add_argument("--max-exp", type=int, default=2046, help="max IEEE biased exponent field for --random (default: 2046, largest normal)")
	ap.add_argument("--comment", type=str, default=None, help="optional '; ...' header comment written before the batch")
	ap.add_argument("--write", action="store_true", help="overwrite out_file instead of appending")
	ap.add_argument("--selftest", action="store_true", help="cross-check fixed_width_decimal against fixed_width_fraction and exit")
	args = ap.parse_args()

	if args.selftest:
		ok = selftest()
		sys.exit(0 if ok else 1)

	if not args.out_file:
		ap.error("out_file is required unless --selftest is given")

	floats = []
	floats += [parse_float_arg(s) for s in args.float]
	floats += [parse_float_arg(s) for s in args.hex]
	floats += [bits_to_float(int(s, 0)) for s in args.bits]
	if args.random:
		floats += random_floats(args.random, args.seed, args.min_exp, args.max_exp)

	if not floats:
		ap.error("no input floats given -- use --float/--hex/--bits/--random")

	for f in floats:
		if not (math.isfinite(f) and f > 0):
			ap.error(f"all inputs must be finite and positive; got {f!r}")

	lines = list(gen_lines(floats, 1, 19))

	mode = "w" if args.write else "a"
	with open(args.out_file, mode) as out:
		if args.comment:
			out.write(f"; --- {args.comment} ---\n")
		for line in lines:
			out.write(line + "\n")

	verb = "wrote" if args.write else "appended"
	print(f"{verb} {len(lines)} lines ({len(floats)} floats x digits 1..19) to {args.out_file}")

if __name__ == "__main__":
	main()
