#include "fpfmt_c.h"

static char g_buf[64];

const char* fpfmt_format_short_str(double f) {
	int n = format(g_buf, f, 0);
	g_buf[n] = '\0';
	return g_buf;
}

const char* fpfmt_format_fixed_str(double f, int nd) {
	int n = format(g_buf, f, nd);
	g_buf[n] = '\0';
	return g_buf;
}

double fpfmt_parse_text_str(const char* s) {
	return parseText(s, s + strlen(s));
}
