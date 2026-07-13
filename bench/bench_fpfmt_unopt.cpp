#include "bench.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
// fpfmt_unopt wrappers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void fpfmt_unopt_fixed(char *dst, double f, int nd) {
	auto [d, p] = unopt::fixedWidth(f, nd);
	int n = unopt::fmt(dst, d, p, nd);
	dst[n] = '\0';
}

static void fpfmt_unopt_short(char *dst, double f) {
	auto [d, p] = unopt::shortWidth(f);
	int nd = unopt::digits(d);
	int n = unopt::fmt(dst, d, p, nd);
	dst[n] = '\0';
}

static void fpfmt_unopt_short_raw(uint64_t *dp, int64_t *pp, double f) {
	auto [d, p] = unopt::shortWidth(f);
	*dp = d;
	*pp = p;
}

static void fpfmt_unopt_format_short(char *dst, double f) {
	int n = unopt::format(dst, f);
	dst[n] = '\0';
}

static void fpfmt_unopt_format_fixed(char *dst, double f, int nd) {
	int n = unopt::format(dst, f, nd);
	dst[n] = '\0';
}

static double fpfmt_unopt_parse_raw(uint64_t d, int p) { return unopt::parse(d, p); }

static double fpfmt_unopt_parse_text(char *start, char *end) {
	return unopt::parseText(start, end);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
static void gen_text(const std::vector<double> &floats, std::vector<char> &buf) {
	char tmp[64];
	for (double f : floats) {
		auto [d, p] = unopt::fixedWidth(f, 17);
		int n = unopt::fmt(tmp, d, p, 17);
		tmp[n] = '\n';
		buf.insert(buf.end(), tmp, tmp + n + 1);
	}
	buf.back() = '\0';
}

static std::vector<std::string> gen_text_strings(const std::vector<double> &floats) {
	std::vector<std::string> out;
	char tmp[64];
	for (double f : floats) {
		auto [d, p] = unopt::fixedWidth(f, 17);
		int n = unopt::fmt(tmp, d, p, 17);
		tmp[n] = '\0';
		out.push_back(std::string(tmp, n));
	}
	return out;
}

static void gen_raw(const std::vector<double> &floats, std::vector<int64_t> &raw) {
	for (double f : floats) {
		auto [d, p] = unopt::fixedWidth(f, 17);
		raw.push_back((int64_t)d);
		raw.push_back((int64_t)p);
	}
}

int main() {
	auto fv = load_floats();
	double *f = fv.data();
	int nf = (int)fv.size();
	char dst[64];

	std::vector<BenchResult> results;

	// fixed width -- raw core algorithm
	for (int nd : {6, 17}) {
		std::string op = "fixed" + std::to_string(nd);
		results.push_back({"fpfmt_unopt", op, bench_fixed(dst, 25, f, nf, nd, fpfmt_unopt_fixed)});

		std::vector<ScatterPoint> scatter;
		scatter_fixed("fpfmt_unopt", f, nf, nd, fpfmt_unopt_fixed, scatter);
		write_scatter_csv(scatter, ("bench/results/scatter_fixed" + std::to_string(nd) + ".csv").c_str());
	}
	write_csv(results, "bench/results/bench_fixed.csv");
	results.clear();

	// short - raw core algorithm
	uint64_t dp;
	int64_t pp;
	results.push_back({"fpfmt_unopt", "short", bench_short(dst, 25, f, nf, fpfmt_unopt_short)});
	results.push_back({"fpfmt_unopt", "short_raw", bench_short_raw(&dp, &pp, 25, f, nf, fpfmt_unopt_short_raw)});

	std::vector<ScatterPoint> short_scatter;
	scatter_short("fpfmt_unopt", f, nf, fpfmt_unopt_short, short_scatter);
	scatter_short_raw("fpfmt_unopt_raw", f, nf, fpfmt_unopt_short_raw, short_scatter);
	write_scatter_csv(short_scatter, "bench/results/scatter_short.csv");
	write_csv(results, "bench/results/bench_short.csv");
	results.clear();

	// format short
	results.push_back({"fpfmt_unopt", "format_short", bench_short(dst, 25, f, nf, fpfmt_unopt_format_short)});
	
	std::vector<ScatterPoint> format_short_scatter;
	scatter_short("fpfmt_unopt", f, nf, fpfmt_unopt_format_short, format_short_scatter);
	write_scatter_csv(format_short_scatter, "bench/results/scatter_format_short.csv");

	// format fixed
	for (int nd : {6, 17}) {
		std::string op = "format_fixed" + std::to_string(nd);
		results.push_back({"fpfmt_unopt", op, bench_fixed(dst, 25, f, nf, nd, fpfmt_unopt_format_fixed)});

		std::vector<ScatterPoint> format_fixed_scatter;
		scatter_fixed("fpfmt_unopt", f, nf, nd, fpfmt_unopt_format_fixed, format_fixed_scatter);
		write_scatter_csv(format_fixed_scatter, ("bench/results/scatter_format_fixed" + std::to_string(nd) + ".csv").c_str());
	}
	write_csv(results, "bench/results/bench_format.csv");
	results.clear();

	// parse
	std::vector<char> text;
	gen_text(fv, text);
	auto texts = gen_text_strings(fv);
	std::vector<int64_t> raw;
	gen_raw(fv, raw);

	results.push_back({"fpfmt_unopt", "parse_raw", bench_parse_raw(25, raw.data(), (int)raw.size(), fpfmt_unopt_parse_raw)});
	results.push_back({"fpfmt_unopt", "parse_text", bench_parse(25, text.data(), fpfmt_unopt_parse_text)});

	std::vector<ScatterPoint> parse_scatter;
	scatter_parse_raw("fpfmt_unopt_raw", raw.data(), (int)raw.size(), fpfmt_unopt_parse_raw, parse_scatter);
	scatter_parse_text("fpfmt_unopt", texts, fpfmt_unopt_parse_text, parse_scatter);
	write_scatter_csv(parse_scatter, "bench/results/scatter_parse.csv");
	write_csv(results, "bench/results/bench_parse.csv");

	///////////////////////////////////////////////////////////////////////////////////////////////
	// validation -- actual output values per input, for bench_validation.py
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::vector<ValidatePoint> validate;
	char vbuf[64];
	for (int i = 0; i < nf; i++) {
		fpfmt_unopt_format_short(vbuf, f[i]);
		validate.push_back({"fpfmt_unopt", "format_short", i, vbuf});

		fpfmt_unopt_format_fixed(vbuf, f[i], 6);
		validate.push_back({"fpfmt_unopt", "format_fixed6", i, vbuf});

		fpfmt_unopt_format_fixed(vbuf, f[i], 17);
		validate.push_back({"fpfmt_unopt", "format_fixed17", i, vbuf});

		char* start = const_cast<char*>(texts[i].c_str());
		double parsed = fpfmt_unopt_parse_text(start, start + texts[i].size());
		uint64_t bits;
		memcpy(&bits, &parsed, 8);
		validate.push_back({"fpfmt_unopt", "parse_text", i, std::to_string(bits)});
	}
	write_validate_csv(validate, "bench/results/validate.csv");

	return 0;
}
