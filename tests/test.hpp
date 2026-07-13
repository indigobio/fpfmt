#ifndef TEST_HPP
#define TEST_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
// includes
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "../src/fpfmt.hpp"
#include <string>
#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <set>
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////////////////////////
// test helpers
///////////////////////////////////////////////////////////////////////////////////////////////////
inline double safeUnpackPack(double f) {
	// caller handles special cases per paper
	if (f == 0.0) return f;
	if (std::isinf(f)) return f;
	if (std::isnan(f)) return f;
	
	auto [m, e] = unpack64(f);
	return pack64(m >> 11, e + 11); // caller provides 53-bit mantissa per paper
}

inline std::string unrounded_string(Unrounded u) {
	uint64_t n = u.val >> 2;
	uint64_t h = ((u.val >> 1) & 1) ? 5 : 0;
	char buf[64];
	snprintf(buf, sizeof(buf), "⟨%llu.%llu%s⟩", (unsigned long long)n, (unsigned long long)h, (u.val & 1) ? "+" : "");
	return std::string(buf);
}

struct IvyTest {
	int n;
	double f;
	uint64_t d;
	int p;
};

bool parse_ivy_line(const std::string& line, IvyTest& out) {
	if (line.empty() || line[0] != '(') return false;
	std::istringstream ss(line);
	char c;
	std::string ftoa, is;
	double f;
	uint64_t d;
	int n, p;
	ss >> c >> n >> ftoa >> f >> c >> is >> d >> p;
	if (ss.fail()) return false;
	out = { n, f, d, p };
	return true;
}

std::vector<IvyTest> load_ivy_tests(const std::string& path) {
	std::vector<IvyTest> tests;
	std::ifstream file(path);
	if (!file.is_open()) {
		printf("ERROR: could not open %s\n", path.c_str());
		return tests;
	}
	std::string line;
	while (std::getline(file, line)) {
		IvyTest t;
		if (parse_ivy_line(line, t)) tests.push_back(t);
	}
	return tests;
}

#endif