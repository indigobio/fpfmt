#!/usr/bin/env python3
import sys
from fractions import Fraction

def load_rows(path):
	rows = []
	with open(path) as f:
		for line in f:
			line = line.strip()
			if not line.startswith('('):
				continue  # comments/headers
			try:
				p1 = line.index('ftoa ') + 5
				p2 = line.index(')', p1)
				hexf = line[p1:p2]
				p3 = line.index(' is ', p2) + 4
				n = int(line[1:line.index(' ftoa')])
				d_str, p_str = line[p3:].split()
				rows.append((n, hexf, int(d_str), int(p_str), line))
			except (ValueError, IndexError):
				rows.append((None, None, None, None, line))
	return rows

def check_row(n, hexf, d, p):
	x = float.fromhex(hexf)
	exact = Fraction(x)

	if n < 1 or n > 19:
		return False, f"N={n} out of range"
	lo10, hi10 = 10 ** (n - 1), 10 ** n
	if not (lo10 <= d < hi10):
		return False, f"D={d} does not have N={n} digits"

	candidate = Fraction(d) * Fraction(10) ** p
	half_ulp = Fraction(1, 2) * Fraction(10) ** p
	diff = exact - candidate

	if abs(diff) > half_ulp:
		return False, "not within half a ULP of the claimed value"
	if abs(diff) == half_ulp and d % 2 != 0:
		return False, "exact tie resolved to the wrong (odd) parity"
	return True, None

def validate_file(path):
	rows = load_rows(path)
	total = failed = parse_errors = 0
	for n, hexf, d, p, raw in rows:
		if n is None:
			parse_errors += 1
			print(f"PARSE ERROR in {path}: {raw}")
			continue
		total += 1
		ok, why = check_row(n, hexf, d, p)
		if not ok:
			failed += 1
			print(f"FAIL [{path}] {raw}\n reason: {why}")
	print(f"{path}: {total} rows checked, {failed} failed, {parse_errors} parse errors")
	return failed + parse_errors

def main():
	bad = 0
	for path in ("data/test.ivy", "data/test.ib"):
		if validate_file(path) != 0:
			bad += 1
	print()
	print("ALL TEST DATA VALIDATED" if bad == 0 else "SOME TEST DATA INVALID")
	return 0 if bad == 0 else 1

if __name__ == "__main__":
	sys.exit(main())
