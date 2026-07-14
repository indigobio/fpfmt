#include "bench.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static char *gen_text(double *f, int nf, int *lenOut) {
	char tmp[64];
	int cap = nf * 24, len = 0;
	char *buf = malloc(cap);
	for (int i = 0; i < nf; i++) {
		int n = snprintf(tmp, sizeof(tmp), "%.16e", f[i]);
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
		snprintf(tmp, sizeof(tmp), "%.16e", f[i]);
		out[i] = strdup(tmp);
	}
	return out;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// native C wrappers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void native_c_format_fixed(char *dst, double f, int nd) {
	snprintf(dst, 64, "%.*e", nd - 1, f);
}

static double native_c_parse_text(char *start, char *end) {
	char saved = *end;
	*end = '\0';
	double f = strtod(start, NULL);
	*end = saved;
	return f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////
int main(void) {
	int nf;
	double *f = load_floats("data/bench_floats.csv", &nf);
	char dst[64];

	BenchResult results[4];
	int nr = 0;

	// format fixed
	int nds[2] = {6, 17};
	for (int k = 0; k < 2; k++) {
		int nd = nds[k];
		char op[24];
		snprintf(op, sizeof(op), "format_fixed%d", nd);
		char *opCopy = strdup(op);
		results[nr++] = (BenchResult){"native_c", opCopy, bench_fixed(dst, 25, f, nf, nd, native_c_format_fixed)};

		char path[64];
		snprintf(path, sizeof(path), "bench/results/scatter_format_fixed%d.csv", nd);
		scatter_fixed("native_c", f, nf, nd, native_c_format_fixed, path);
	}
	write_csv(results, nr, "bench/results/bench_format.csv");
	nr = 0;

	// parse
	int textLen;
	char *text = gen_text(f, nf, &textLen);
	char **texts = gen_text_strings(f, nf);

	results[nr++] = (BenchResult){"native_c", "parse_text", bench_parse(25, text, native_c_parse_text)};
	scatter_parse_text("native_c", texts, nf, native_c_parse_text, "bench/results/scatter_parse.csv");
	write_csv(results, nr, "bench/results/bench_parse.csv");

	///////////////////////////////////////////////////////////////////////////////////////////////
	// validation -- actual output values per input, for bench_validation.py
	///////////////////////////////////////////////////////////////////////////////////////////////
	ValidatePoint *validate = malloc(3 * nf * sizeof(ValidatePoint));
	int nv = 0;
	char vbuf[64];
	for (int i = 0; i < nf; i++) {
		native_c_format_fixed(vbuf, f[i], 6);
		validate[nv++] = (ValidatePoint){"native_c", "format_fixed6", i, strdup(vbuf)};

		native_c_format_fixed(vbuf, f[i], 17);
		validate[nv++] = (ValidatePoint){"native_c", "format_fixed17", i, strdup(vbuf)};

		double parsed = native_c_parse_text(texts[i], texts[i] + strlen(texts[i]));
		uint64_t bits;
		memcpy(&bits, &parsed, 8);
		char *value = malloc(24);
		snprintf(value, 24, "%llu", (unsigned long long)bits);
		validate[nv++] = (ValidatePoint){"native_c", "parse_text", i, value};
	}
	write_validate_csv(validate, nv, "bench/results/validate.csv");

	return 0;
}
