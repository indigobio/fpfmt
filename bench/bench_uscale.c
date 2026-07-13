#include "bench.h"
#include "../src/uscale.c"

///////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static char *gen_text(double *f, int nf, int *lenOut) {
	char tmp[64];
	int cap = nf * 24, len = 0;
	char *buf = malloc(cap);
	for (int i = 0; i < nf; i++) {
		uint64_t d;
		int p;
		FixedWidth(f[i], 17, &d, &p);
		Format(tmp, d, p, 17);
		int n = (int)strlen(tmp);
		if (len + n + 1 > cap) {
			cap *= 2;
			buf = realloc(buf, cap);
		}
		memcpy(buf + len, tmp, n);
		len += n;
		buf[len++] = '\n';
	}
	buf[len - 1] = '\0';
	*lenOut = len;
	return buf;
}

static char **gen_text_strings(double *f, int nf) {
	char **out = malloc(nf * sizeof(char *));
	char tmp[64];
	for (int i = 0; i < nf; i++) {
		uint64_t d;
		int p;
		FixedWidth(f[i], 17, &d, &p);
		Format(tmp, d, p, 17);
		out[i] = strdup(tmp);
	}
	return out;
}

static int64_t *gen_raw(double *f, int nf) {
	int64_t *raw = malloc(2 * nf * sizeof(int64_t));
	for (int i = 0; i < nf; i++) {
		uint64_t d;
		int p;
		FixedWidth(f[i], 17, &d, &p);
		raw[2 * i] = (int64_t)d;
		raw[2 * i + 1] = (int64_t)p;
	}
	return raw;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// format wrappers -- thin signature adapters over uscale.c's formatComplete(), matching the
// void(char*,double)/void(char*,double,int) shapes bench_fixed/bench_short & co. expect
///////////////////////////////////////////////////////////////////////////////////////////////////
static void uscale_format_short(char *dst, double f) { formatComplete(dst, f, 0); }
static void uscale_format_fixed(char *dst, double f, int nd) { formatComplete(dst, f, nd); }

///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////
int main(void) {
	int nf;
	double *f = load_floats("data/bench_floats.csv", &nf);
	char dst[64];

	BenchResult results[8];
	int nr = 0;

	// fixed width -- raw core algorithm
	int nds[2] = {6, 17};
	for (int k = 0; k < 2; k++) {
		int nd = nds[k];
		char op[16];
		snprintf(op, sizeof(op), "fixed%d", nd);
		char *opCopy = strdup(op);
		results[nr++] = (BenchResult){"uscale", opCopy, bench_fixed(dst, 25, f, nf, nd, fixed)};

		char path[64];
		snprintf(path, sizeof(path), "bench/results/scatter_fixed%d.csv", nd);
		scatter_fixed("uscale", f, nf, nd, fixed, path);
	}
	write_csv(results, nr, "bench/results/bench_fixed.csv");
	nr = 0;

	// short -- raw core algorithm
	uint64_t dp;
	int64_t pp;
	results[nr++] = (BenchResult){"uscale", "short", bench_short(dst, 25, f, nf, short1)};
	results[nr++] = (BenchResult){"uscale", "short_raw", bench_short_raw(&dp, &pp, 25, f, nf, shortRaw)};

	scatter_short("uscale", f, nf, short1, "bench/results/scatter_short.csv");
	scatter_short_raw("uscale_raw", f, nf, shortRaw, "bench/results/scatter_short.csv");
	write_csv(results, nr, "bench/results/bench_short.csv");
	nr = 0;

	// format short -- complete, apples-to-apples: pays for the sign/zero/inf/nan dispatch too
	results[nr++] = (BenchResult){"uscale", "format_short", bench_short(dst, 25, f, nf, uscale_format_short)};
	scatter_short("uscale", f, nf, uscale_format_short, "bench/results/scatter_format_short.csv");

	// format fixed
	for (int k = 0; k < 2; k++) {
		int nd = nds[k];
		char op[24];
		snprintf(op, sizeof(op), "format_fixed%d", nd);
		char *opCopy = strdup(op);
		results[nr++] = (BenchResult){"uscale", opCopy, bench_fixed(dst, 25, f, nf, nd, uscale_format_fixed)};

		char path[64];
		snprintf(path, sizeof(path), "bench/results/scatter_format_fixed%d.csv", nd);
		scatter_fixed("uscale", f, nf, nd, uscale_format_fixed, path);
	}
	write_csv(results, nr, "bench/results/bench_format.csv");
	nr = 0;

	// parse
	int textLen;
	char *text = gen_text(f, nf, &textLen);
	char **texts = gen_text_strings(f, nf);
	int64_t *raw = gen_raw(f, nf);

	results[nr++] = (BenchResult){"uscale", "parse_raw", bench_parse_raw(25, raw, 2 * nf, Parse)};
	results[nr++] = (BenchResult){"uscale", "parse_text", bench_parse(25, text, uscalecStrtod)};

	scatter_parse_raw("uscale_raw", raw, 2 * nf, Parse, "bench/results/scatter_parse.csv");
	scatter_parse_text("uscale", texts, nf, uscalecStrtod, "bench/results/scatter_parse.csv");
	write_csv(results, nr, "bench/results/bench_parse.csv");

	///////////////////////////////////////////////////////////////////////////////////////////////
	// validation -- actual output values per input, for bench_validation.py
	///////////////////////////////////////////////////////////////////////////////////////////////
	ValidatePoint *validate = malloc(4 * nf * sizeof(ValidatePoint));
	int nv = 0;
	char vbuf[64];
	for (int i = 0; i < nf; i++) {
		uscale_format_short(vbuf, f[i]);
		validate[nv++] = (ValidatePoint){"uscale", "format_short", i, strdup(vbuf)};

		uscale_format_fixed(vbuf, f[i], 6);
		validate[nv++] = (ValidatePoint){"uscale", "format_fixed6", i, strdup(vbuf)};

		uscale_format_fixed(vbuf, f[i], 17);
		validate[nv++] = (ValidatePoint){"uscale", "format_fixed17", i, strdup(vbuf)};

		double parsed = uscalecStrtod(texts[i], texts[i] + strlen(texts[i]));
		uint64_t bits;
		memcpy(&bits, &parsed, 8);
		char *value = malloc(24);
		snprintf(value, 24, "%llu", (unsigned long long)bits);
		validate[nv++] = (ValidatePoint){"uscale", "parse_text", i, value};
	}
	write_validate_csv(validate, nv, "bench/results/validate.csv");

	return 0;
}
