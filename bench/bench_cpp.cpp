#include "bench.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
// native wrappers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void native_format_short(char *dst, double f) {
	auto res = std::to_chars(dst, dst + 63, f);
	*res.ptr = '\0';
}

static void native_format_fixed(char *dst, double f, int nd) {
	auto res = std::to_chars(dst, dst + 63, f, std::chars_format::scientific, nd - 1);
	*res.ptr = '\0';
}

static double native_parse_text(char *start, char *end) {
	char saved = *end;
	*end = '\0';
	double f = strtod(start, nullptr);
	*end = saved;
	return f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void gen_text(const std::vector<double> &floats, std::vector<char> &buf) {
	char tmp[64];
	for (double f : floats) {
		int n = snprintf(tmp, sizeof(tmp), "%.16e", f);
		tmp[n] = '\n';
		buf.insert(buf.end(), tmp, tmp + n + 1);
	}
	buf.back() = '\0';
}

static std::vector<std::string> gen_text_strings(const std::vector<double> &floats) {
	std::vector<std::string> out;
	char tmp[64];
	for (double f : floats) {
		int n = snprintf(tmp, sizeof(tmp), "%.16e", f);
		out.push_back(std::string(tmp, n));
	}
	return out;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
	auto fv = load_floats();
	double *f = fv.data();
	int nf = (int)fv.size();
	char dst[64];

	std::vector<BenchResult> results;

	// format short
	results.push_back({"native_cpp", "format_short", bench_short(dst, 25, f, nf, native_format_short)});
	std::vector<ScatterPoint> format_short_scatter;
	scatter_short("native_cpp", f, nf, native_format_short, format_short_scatter);
	write_scatter_csv(format_short_scatter, "bench/results/scatter_format_short.csv");

	// format fixed
	for (int nd : {6, 17}) {
		std::string op = "format_fixed" + std::to_string(nd);
		results.push_back({"native_cpp", op, bench_fixed(dst, 25, f, nf, nd, native_format_fixed)});

		std::vector<ScatterPoint> format_fixed_scatter;
		scatter_fixed("native_cpp", f, nf, nd, native_format_fixed, format_fixed_scatter);
		write_scatter_csv(format_fixed_scatter, ("bench/results/scatter_format_fixed" + std::to_string(nd) + ".csv").c_str());
	}
	write_csv(results, "bench/results/bench_format.csv");
	results.clear();

	// parse
	std::vector<char> text;
	gen_text(fv, text);
	auto texts = gen_text_strings(fv);

	results.push_back({"native_cpp", "parse_text", bench_parse(25, text.data(), native_parse_text)});

	std::vector<ScatterPoint> parse_scatter;
	scatter_parse_text("native_cpp", texts, native_parse_text, parse_scatter);
	write_scatter_csv(parse_scatter, "bench/results/scatter_parse.csv");
	write_csv(results, "bench/results/bench_parse.csv");

	///////////////////////////////////////////////////////////////////////////////////////////////
	// validation -- actual output values per input, for bench_validation.py
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::vector<ValidatePoint> validate;
	char vbuf[64];
	for (int i = 0; i < nf; i++) {
		native_format_short(vbuf, f[i]);
		validate.push_back({"native_cpp", "format_short", i, vbuf});

		native_format_fixed(vbuf, f[i], 6);
		validate.push_back({"native_cpp", "format_fixed6", i, vbuf});

		native_format_fixed(vbuf, f[i], 17);
		validate.push_back({"native_cpp", "format_fixed17", i, vbuf});

		char* start = const_cast<char*>(texts[i].c_str());
		double parsed = native_parse_text(start, start + texts[i].size());
		uint64_t bits;
		memcpy(&bits, &parsed, 8);
		validate.push_back({"native_cpp", "parse_text", i, std::to_string(bits)});
	}
	write_validate_csv(validate, "bench/results/validate.csv");

	return 0;
}
