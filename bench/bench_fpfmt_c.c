#include "bench.h"
#include "../src/fpfmt_c.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static char *gen_text(double *f, int nf, int *lenOut) {
	char tmp[64];
	int cap = nf * 24, len = 0;
	char *buf = malloc(cap);
	for (int i = 0; i < nf; i++) {
		FloatRep fr = fixedWidth(f[i], 17);
		int n = fmt(tmp, fr.m, fr.e, 17);
		if (len + n + 1 > cap) {
			cap *= 2;
			buf = realloc(buf, cap);
		}
		memcpy(buf + len, tmp, n);
		len += n;
		buf[len++] = '\n';
	}
	if (len > 0) buf[len - 1] = '\0';
	else buf[0] = '\0';
	*lenOut = len;
	return buf;
}

static char **gen_text_strings(double *f, int nf) {
	char **out = malloc(nf * sizeof(char *));
	char tmp[64];
	for (int i = 0; i < nf; i++) {
		FloatRep fr = fixedWidth(f[i], 17);
		int n = fmt(tmp, fr.m, fr.e, 17);
		tmp[n] = '\0';
		out[i] = strdup(tmp);
	}
	return out;
}

static int64_t *gen_raw(double *f, int nf) {
	int64_t *raw = malloc(2 * nf * sizeof(int64_t));
	for (int i = 0; i < nf; i++) {
		FloatRep fr = fixedWidth(f[i], 17);
		raw[2 * i] = (int64_t)fr.m;
		raw[2 * i + 1] = (int64_t)fr.e;
	}
	return raw;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// format wrappers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void fpfmtc_fixed(char *dst, double f, int n) {
	FloatRep fr = fixedWidth(f, n);
	fmt(dst, fr.m, fr.e, n);
}

static void fpfmtc_short(char *dst, double f) {
	FloatRep fr = shortWidth(f);
	fmt(dst, fr.m, fr.e, digits(fr.m));
}

static void fpfmtc_shortRaw(uint64_t *dp, int64_t *pp, double f) {
	FloatRep fr = shortWidth(f);
	*dp = fr.m;
	*pp = fr.e;
}

static void fpfmtc_format_short(char *dst, double f) { format(dst, f, 0); }
static void fpfmtc_format_fixed(char *dst, double f, int nd) { format(dst, f, nd); }

static double fpfmtc_parseText(char *s, char *e) { return parseText(s, e); }

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
		results[nr++] = (BenchResult){"fpfmt_c", opCopy, bench_fixed(dst, 25, f, nf, nd, fpfmtc_fixed)};

		char path[64];
		snprintf(path, sizeof(path), "bench/results/scatter_fixed%d.csv", nd);
		scatter_fixed("fpfmt_c", f, nf, nd, fpfmtc_fixed, path);
	}
	write_csv(results, nr, "bench/results/bench_fixed.csv");
	nr = 0;

	// short -- raw core algorithm
	uint64_t dp;
	int64_t pp;
	results[nr++] = (BenchResult){"fpfmt_c", "short", bench_short(dst, 25, f, nf, fpfmtc_short)};
	results[nr++] = (BenchResult){"fpfmt_c", "short_raw", bench_short_raw(&dp, &pp, 25, f, nf, fpfmtc_shortRaw)};

	scatter_short("fpfmt_c", f, nf, fpfmtc_short, "bench/results/scatter_short.csv");
	scatter_short_raw("fpfmt_c_raw", f, nf, fpfmtc_shortRaw, "bench/results/scatter_short.csv");
	write_csv(results, nr, "bench/results/bench_short.csv");
	nr = 0;

	// format short -- complete, apples-to-apples: pays for the sign/zero/inf/nan dispatch too
	results[nr++] = (BenchResult){"fpfmt_c", "format_short", bench_short(dst, 25, f, nf, fpfmtc_format_short)};
	scatter_short("fpfmt_c", f, nf, fpfmtc_format_short, "bench/results/scatter_format_short.csv");

	// format fixed
	for (int k = 0; k < 2; k++) {
		int nd = nds[k];
		char op[24];
		snprintf(op, sizeof(op), "format_fixed%d", nd);
		char *opCopy = strdup(op);
		results[nr++] = (BenchResult){"fpfmt_c", opCopy, bench_fixed(dst, 25, f, nf, nd, fpfmtc_format_fixed)};

		char path[64];
		snprintf(path, sizeof(path), "bench/results/scatter_format_fixed%d.csv", nd);
		scatter_fixed("fpfmt_c", f, nf, nd, fpfmtc_format_fixed, path);
	}
	write_csv(results, nr, "bench/results/bench_format.csv");
	nr = 0;

	// parse
	int textLen;
	char *text = gen_text(f, nf, &textLen);
	char **texts = gen_text_strings(f, nf);
	int64_t *raw = gen_raw(f, nf);

	results[nr++] = (BenchResult){"fpfmt_c", "parse_raw", bench_parse_raw(25, raw, 2 * nf, parse)};
	results[nr++] = (BenchResult){"fpfmt_c", "parse_text", bench_parse(25, text, fpfmtc_parseText)};

	scatter_parse_raw("fpfmt_c_raw", raw, 2 * nf, parse, "bench/results/scatter_parse.csv");
	scatter_parse_text("fpfmt_c", texts, nf, fpfmtc_parseText, "bench/results/scatter_parse.csv");
	write_csv(results, nr, "bench/results/bench_parse.csv");

	///////////////////////////////////////////////////////////////////////////////////////////////
	// validation -- actual output values per input, for bench_validation.py
	///////////////////////////////////////////////////////////////////////////////////////////////
	ValidatePoint *validate = malloc(4 * nf * sizeof(ValidatePoint));
	int nv = 0;
	char vbuf[64];
	int vlen;
	for (int i = 0; i < nf; i++) {
		vlen = format(vbuf, f[i], 0);
		vbuf[vlen] = '\0';
		validate[nv++] = (ValidatePoint){"fpfmt_c", "format_short", i, strdup(vbuf)};

		vlen = format(vbuf, f[i], 6);
		vbuf[vlen] = '\0';
		validate[nv++] = (ValidatePoint){"fpfmt_c", "format_fixed6", i, strdup(vbuf)};

		vlen = format(vbuf, f[i], 17);
		vbuf[vlen] = '\0';
		validate[nv++] = (ValidatePoint){"fpfmt_c", "format_fixed17", i, strdup(vbuf)};

		double parsed = fpfmtc_parseText(texts[i], texts[i] + strlen(texts[i]));
		uint64_t bits;
		memcpy(&bits, &parsed, 8);
		char *value = malloc(24);
		snprintf(value, 24, "%llu", (unsigned long long)bits);
		validate[nv++] = (ValidatePoint){"fpfmt_c", "parse_text", i, value};
	}
	write_validate_csv(validate, nv, "bench/results/validate.csv");

	return 0;
}
