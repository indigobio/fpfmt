#include "fpfmt.hpp"

extern "C" {

int fpfmt_format_short(char* dst, double f) { return format(dst, f); }

int fpfmt_format_fixed(char* dst, double f, int nd) { return format(dst, f, nd); }

double fpfmt_parse_text(const char* start, int len) { return parseText(start, start + len); }

}
