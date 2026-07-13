#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define REPS 25
#define BATCH 1000

///////////////////////////////////////////////////////////////////////////////////////////////////
// timing
///////////////////////////////////////////////////////////////////////////////////////////////////
static uint64_t now_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// bench harness
///////////////////////////////////////////////////////////////////////////////////////////////////
static double bench_fixed(char *dst, int count, double *f, int nf, int digits, void (* volatile fn)(char *, double, int)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dst, f[j], digits);
	return (double)(now_ns() - start) / (count * nf);
}

static double bench_short(char *dst, int count, double *f, int nf, void (* volatile fn)(char *, double)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dst, f[j]);
	return (double)(now_ns() - start) / (count * nf);
}

static double bench_short_raw(uint64_t *dp, int64_t *pp, int count, double *f, int nf, void (* volatile fn)(uint64_t *, int64_t *, double)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dp, pp, f[j]);
	return (double)(now_ns() - start) / (count * nf);
}

static double bench_parse_raw(int count, int64_t *raw, int nraw, double (* volatile fn)(uint64_t, int)) {
	double total = 0;
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++) {
		total = 0;
		for (int j = 0; j < nraw; j += 2)
			total += fn((uint64_t)raw[j], (int)raw[j + 1]);
	}
	(void)total;
	return (double)(now_ns() - start) / (count * (nraw / 2));
}

static double bench_parse(int count, char *text, double (* volatile fn)(char *, char *)) {
	double total = 0;
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++) {
		int s = 0;
		total = 0;
		for (int j = 0;; j++) {
			int c = text[j];
			if (c == '\0' || c == '\n') {
				total += fn(text + s, text + j);
				if (c == '\0') break;
				s = j + 1;
			}
		}
	}
	(void)total;
	int n = 0;
	for (int j = 0; text[j]; j++) if (text[j] == '\n') n++;
	n++;
	return (double)(now_ns() - start) / (count * n);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// scatter -- per-input timing, reps per input, median taken; writes straight to CSV since C has
// no convenient vector<ScatterPoint> to hand off like bench.hpp's write_scatter_csv
///////////////////////////////////////////////////////////////////////////////////////////////////
static int fileExists(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	fclose(f);
	return 1;
}

static FILE *openScatterCsv(const char *path) {
	int exists = fileExists(path);
	FILE *f = fopen(path, "a");
	if (!f) { printf("ERROR: could not open %s\n", path); return NULL; }
	if (!exists) fprintf(f, "impl,log2f,ns\n");
	return f;
}

static int cmpDouble(const void *a, const void *b) {
	double da = *(const double *)a, db = *(const double *)b;
	return (da > db) - (da < db);
}

static void scatter_fixed(const char *impl, double *f, int nf, int digits, void (* volatile fn)(char *, double, int), const char *path) {
	FILE *out = openScatterCsv(path);
	if (!out) return;
	char dst[64];
	double times[REPS];
	for (int i = 0; i < nf; i++) {
		for (int r = 0; r < REPS; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < BATCH; b++)
				fn(dst, f[i], digits);
			times[r] = (double)(now_ns() - s) / BATCH;
		}
		qsort(times, REPS, sizeof(double), cmpDouble);
		fprintf(out, "%s,%.6f,%.2f\n", impl, log2(fabs(f[i])), times[REPS / 2]);
	}
	fclose(out);
	printf("appended to %s\n", path);
}

static void scatter_short(const char *impl, double *f, int nf, void (* volatile fn)(char *, double), const char *path) {
	FILE *out = openScatterCsv(path);
	if (!out) return;
	char dst[64];
	double times[REPS];
	for (int i = 0; i < nf; i++) {
		for (int r = 0; r < REPS; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < BATCH; b++)
				fn(dst, f[i]);
			times[r] = (double)(now_ns() - s) / BATCH;
		}
		qsort(times, REPS, sizeof(double), cmpDouble);
		fprintf(out, "%s,%.6f,%.2f\n", impl, log2(fabs(f[i])), times[REPS / 2]);
	}
	fclose(out);
	printf("appended to %s\n", path);
}

static void scatter_short_raw(const char *impl, double *f, int nf, void (* volatile fn)(uint64_t *, int64_t *, double), const char *path) {
	FILE *out = openScatterCsv(path);
	if (!out) return;
	uint64_t dp;
	int64_t pp;
	double times[REPS];
	for (int i = 0; i < nf; i++) {
		for (int r = 0; r < REPS; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < BATCH; b++)
				fn(&dp, &pp, f[i]);
			times[r] = (double)(now_ns() - s) / BATCH;
		}
		qsort(times, REPS, sizeof(double), cmpDouble);
		fprintf(out, "%s,%.6f,%.2f\n", impl, log2(fabs(f[i])), times[REPS / 2]);
	}
	fclose(out);
	printf("appended to %s\n", path);
}

static void scatter_parse_raw(const char *impl, int64_t *raw, int nraw, double (* volatile fn)(uint64_t, int), const char *path) {
	FILE *out = openScatterCsv(path);
	if (!out) return;
	double times[REPS];
	for (int i = 0; i < nraw; i += 2) {
		uint64_t d = (uint64_t)raw[i];
		int p = (int)raw[i + 1];
		double ref = fn(d, p);
		for (int r = 0; r < REPS; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < BATCH; b++) {
				volatile double sink = fn(d, p);
				(void)sink;
			}
			times[r] = (double)(now_ns() - s) / BATCH;
		}
		qsort(times, REPS, sizeof(double), cmpDouble);
		fprintf(out, "%s,%.6f,%.2f\n", impl, log2(fabs(ref)), times[REPS / 2]);
	}
	fclose(out);
	printf("appended to %s\n", path);
}

static void scatter_parse_text(const char *impl, char **texts, int ntexts, double (* volatile fn)(char *, char *), const char *path) {
	FILE *out = openScatterCsv(path);
	if (!out) return;
	double times[REPS];
	for (int i = 0; i < ntexts; i++) {
		char *start = texts[i];
		char *end = start + strlen(start);
		double ref = fn(start, end);
		for (int r = 0; r < REPS; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < BATCH; b++) {
				volatile double sink = fn(start, end);
				(void)sink;
			}
			times[r] = (double)(now_ns() - s) / BATCH;
		}
		qsort(times, REPS, sizeof(double), cmpDouble);
		fprintf(out, "%s,%.6f,%.2f\n", impl, log2(fabs(ref)), times[REPS / 2]);
	}
	fclose(out);
	printf("appended to %s\n", path);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// result reporting -- mirrors bench.hpp's BenchResult/write_csv
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	const char *impl;
	const char *op;
	double median_ns;
} BenchResult;

static void write_csv(BenchResult *results, int n, const char *path) {
	int exists = fileExists(path);
	FILE *f = fopen(path, "a");
	if (!f) { printf("ERROR: could not open %s\n", path); return; }
	if (!exists) fprintf(f, "impl,op,median_ns\n");
	for (int i = 0; i < n; i++)
		fprintf(f, "%s,%s,%.2f\n", results[i].impl, results[i].op, results[i].median_ns);
	fclose(f);
	printf("appended to %s\n", path);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// validation -- mirrors bench.hpp's ValidatePoint/write_validate_csv
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	const char *impl;
	const char *op;
	int index;
	char *value;
} ValidatePoint;

static void write_validate_csv(ValidatePoint *points, int n, const char *path) {
	int exists = fileExists(path);
	FILE *f = fopen(path, "a");
	if (!f) { printf("ERROR: could not open %s\n", path); return; }
	if (!exists) fprintf(f, "impl,op,index,value\n");
	for (int i = 0; i < n; i++)
		fprintf(f, "%s,%s,%d,%s\n", points[i].impl, points[i].op, points[i].index, points[i].value);
	fclose(f);
	printf("appended to %s\n", path);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// input loading -- reads the same data/bench_floats.csv the other benches share
///////////////////////////////////////////////////////////////////////////////////////////////////
static double *load_floats(const char *path, int *nOut) {
	FILE *f = fopen(path, "r");
	if (!f) {
		printf("ERROR: could not open %s -- run `make floats` first\n", path);
		exit(1);
	}
	char line[64];
	fgets(line, sizeof(line), f);  // skip header
	int cap = 16384, n = 0;
	double *out = (double*)malloc(cap * sizeof(double));
	while (fgets(line, sizeof(line), f)) {
		uint64_t bits = strtoull(line, NULL, 10);
		if (n == cap) {
			cap *= 2;
			out = (double*)realloc(out, cap * sizeof(double));
		}
		memcpy(&out[n], &bits, 8);
		n++;
	}
	fclose(f);
	*nOut = n;
	return out;
}

#endif
