#!/usr/bin/env python3
import csv
import math
import os
import struct
from collections import defaultdict
from decimal import Context, Decimal, ROUND_HALF_EVEN
from fractions import Fraction

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FLOATS_CSV = os.path.join(SCRIPT_DIR, "..", "data", "bench_floats.csv")
VALIDATE_CSV = os.path.join(SCRIPT_DIR, "..", "bench", "results", "validate.csv")
MAX_EXAMPLES = 5

ORACLE_DISAGREEMENTS = []

def load_floats(path):
	floats = []
	with open(path) as f:
		next(f) # skip header
		for line in f:
			bits = int(line.strip())
			floats.append(struct.unpack("<d", struct.pack("<Q", bits))[0])
	return floats

def load_validate(path):
	rows = defaultdict(lambda: defaultdict(dict))
	with open(path) as f:
		for row in csv.DictReader(f):
			rows[row["op"]][row["impl"]][int(row["index"])] = row["value"]
	return rows

def round_half_even(x: Fraction) -> int:
	q, r = divmod(x.numerator, x.denominator)
	twice_r = 2 * r
	if twice_r < x.denominator:
		return q
	if twice_r > x.denominator:
		return q + 1
	return q if q % 2 == 0 else q + 1

def correctly_rounded_digits_fraction(f: float, n: int):
	frac = Fraction(f)
	p = n - 1 - math.floor(math.log10(f))  # approximate, corrected exactly below
	ten_n, ten_n1 = Fraction(10) ** n, Fraction(10) ** (n - 1)
	while frac * (Fraction(10) ** p) >= ten_n:
		p -= 1
	while frac * (Fraction(10) ** p) < ten_n1:
		p += 1
	d = round_half_even(frac * (Fraction(10) ** p))
	if d >= 10 ** n:
		d //= 10
		p -= 1
	return d, -p

def correctly_rounded_digits_decimal(f: float, n: int):
	ctx = Context(prec=n, rounding=ROUND_HALF_EVEN)
	_, digits, exponent = ctx.plus(Decimal(f)).as_tuple()
	d = int("".join(map(str, digits)))
	pad = n - len(str(d))
	if pad > 0:
		d *= 10 ** pad
		exponent -= pad
	return d, exponent

def correctly_rounded_digits(f: float, n: int):
	via_fraction = correctly_rounded_digits_fraction(f, n)
	via_decimal = correctly_rounded_digits_decimal(f, n)
	if via_fraction != via_decimal:
		ORACLE_DISAGREEMENTS.append(("correctly_rounded_digits", f, n, via_fraction, via_decimal))
	return via_fraction

def is_even_mantissa(x: float) -> bool:
	return (struct.unpack("<Q", struct.pack("<d", x))[0] & 1) == 0

def nearest_double_fraction(d: int, p: int) -> float:
	target = Fraction(d) * (Fraction(10) ** p)
	candidate = float(Decimal(d).scaleb(p))
	lo, hi = math.nextafter(candidate, -math.inf), math.nextafter(candidate, math.inf)
	mid_lo = (Fraction(lo) + Fraction(candidate)) / 2
	mid_hi = (Fraction(candidate) + Fraction(hi)) / 2
	if target < mid_lo:
		return lo
	if target > mid_hi:
		return hi
	if target == mid_lo:
		return lo if is_even_mantissa(lo) else candidate
	if target == mid_hi:
		return hi if is_even_mantissa(hi) else candidate
	return candidate

def nearest_double_decimal(d: int, p: int) -> float:
	"""same contract as nearest_double_fraction, using Decimal's own correctly-
	rounded Decimal-to-float conversion as the independent cross-check."""
	return float(Decimal(d).scaleb(p))

def nearest_double(d: int, p: int) -> float:
	"""cross-checked oracle: agree or don't trust either."""
	via_fraction = nearest_double_fraction(d, p)
	via_decimal = nearest_double_decimal(d, p)
	if via_fraction != via_decimal:
		ORACLE_DISAGREEMENTS.append(("nearest_double", d, p, via_fraction, via_decimal))
	return via_fraction

class UncertainDigit(Exception):
	"""raised when a printer declines to name a digit -- Chez's ~E emits '#' past
	what it could confidently round-trip. Tracked separately, not guessed at."""

def parse_dp(value_string: str):
	value_string = value_string.split("|", 1)[0]
	if "#" in value_string:
		raise UncertainDigit(value_string)
	t = Decimal(value_string).as_tuple()
	d = int("".join(map(str, t.digits)))
	return d, t.exponent

def bits_to_float(bits_str: str) -> float:
	return struct.unpack("<d", struct.pack("<Q", int(bits_str)))[0]

def check_fixed(op, nd, floats, rows):
	print(f"\n--- {op} ---")
	ok = True
	for impl, values in sorted(rows[op].items()):
		mismatches, declined, examples = 0, 0, []
		for i, f in enumerate(floats):
			try:
				got_dp = parse_dp(values[i])
			except UncertainDigit:
				declined += 1
				continue
			true_dp = correctly_rounded_digits(f, nd)
			if got_dp != true_dp:
				mismatches += 1
				if len(examples) < MAX_EXAMPLES:
					examples.append((i, f, got_dp, true_dp))
		status = "PASS" if not mismatches else f"FAIL ({mismatches}/{len(floats)} mismatches)"
		suffix = f"  [DECLINED: {declined}/{len(floats)} -- printer marked a digit uncertain]" if declined else ""
		print(f"  {impl:15s} {status}{suffix}")
		for i, f, got, true in examples:
			print(f"    index={i} f={f!r} got={got} expected={true}")
		ok &= not mismatches
	return ok

def check_short(floats, rows):
	print("\n--- format_short ---")
	ok = True
	for impl, values in sorted(rows["format_short"].items()):
		roundtrip_fails, minimality_fails, declined, examples = 0, 0, 0, []
		for i, f in enumerate(floats):
			try:
				d, p = parse_dp(values[i])
			except UncertainDigit:
				declined += 1
				continue
			if nearest_double(d, p) != f:
				roundtrip_fails += 1
				if len(examples) < MAX_EXAMPLES:
					examples.append(("roundtrip", i, f, values[i]))
				continue
			n = len(str(d))
			total_len = n + max(p, 0)  # actual printed digit span, incl. implicit trailing zeros
			if n > 1:
				short_d, short_p = correctly_rounded_digits(f, n - 1)
				short_total_len = len(str(short_d)) + max(short_p, 0)
				if short_total_len < total_len and nearest_double(short_d, short_p) == f:
					minimality_fails += 1
					if len(examples) < MAX_EXAMPLES:
						examples.append(("not-minimal", i, f, values[i]))
		status = "PASS" if not (roundtrip_fails or minimality_fails) else \
			f"FAIL (roundtrip={roundtrip_fails}, not-minimal={minimality_fails})"
		suffix = f"  [DECLINED: {declined}/{len(floats)} -- printer marked a digit uncertain]" if declined else ""
		print(f"  {impl:15s} {status}{suffix}")
		for kind, i, f, v in examples:
			print(f"    [{kind}] index={i} f={f!r} value={v!r}")
		ok &= not (roundtrip_fails or minimality_fails)
	return ok

def check_parse(floats, rows):
	print("\n--- parse_text ---")
	ok = True
	for impl, values in sorted(rows["parse_text"].items()):
		mismatches, examples = 0, []
		for i, f in enumerate(floats):
			got = bits_to_float(values[i])
			if got != f:
				mismatches += 1
				if len(examples) < MAX_EXAMPLES:
					examples.append((i, f, got))
		print(f"  {impl:15s} {'PASS' if not mismatches else f'FAIL ({mismatches}/{len(floats)})'}")
		for i, f, got in examples:
			print(f"    index={i} expected={f!r} got={got!r}")
		ok &= not mismatches
	return ok

def main():
	floats = load_floats(FLOATS_CSV)
	rows = load_validate(VALIDATE_CSV)
	print(f"loaded {len(floats)} floats, validating against {VALIDATE_CSV}\n")

	ok = True
	ok &= check_fixed("format_fixed6", 6, floats, rows)
	ok &= check_fixed("format_fixed17", 17, floats, rows)
	ok &= check_short(floats, rows)
	ok &= check_parse(floats, rows)

	print()
	if ORACLE_DISAGREEMENTS:
		print(f"=== ORACLE DISAGREEMENT: Fraction and Decimal oracles disagreed "
			f"{len(ORACLE_DISAGREEMENTS)} time(s) -- results above are NOT trustworthy ===")
		for fn, a1, a2, via_fraction, via_decimal in ORACLE_DISAGREEMENTS[:MAX_EXAMPLES]:
			print(f"    {fn}({a1!r}, {a2!r}): fraction={via_fraction!r} decimal={via_decimal!r}")
		return 2
	print("=== ALL PASSED ===" if ok else "=== SOME FAILED ===")
	return 0 if ok else 1

if __name__ == "__main__":
	raise SystemExit(main())
