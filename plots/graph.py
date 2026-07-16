import os
import glob
import platform
import subprocess
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "../bench/results")
OUT_DIR = os.path.join(SCRIPT_DIR, "graphs")
CDF_TRIM = 0.0005

def _cpu_brand():
	system = platform.system()
	try:
		if system == "Darwin":
			return subprocess.check_output(
				["sysctl", "-n", "machdep.cpu.brand_string"], text=True, timeout=2).strip()
		if system == "Linux":
			with open("/proc/cpuinfo") as f:
				for line in f:
					if line.lower().startswith("model name"):
						return line.split(":", 1)[1].strip()
		if system == "Windows":
			try:
				out = subprocess.check_output(["wmic", "cpu", "get", "name"], text=True, timeout=2)
				return out.splitlines()[1].strip()
			except Exception:
				out = subprocess.check_output(
					["powershell", "-NoProfile", "-Command", "(Get-CimInstance Win32_Processor).Name"],
					text=True, timeout=3)
				return out.strip()
	except Exception:
		pass
	return None

def hardware_label():
	base = f"{platform.system() or 'unknown OS'} {platform.machine() or ''}".strip()
	brand = _cpu_brand()
	return f"{base} -- {brand}" if brand else base

HW_LABEL = hardware_label()

IMPL_COLORS = {
	"fpfmt_unopt": "#f69948",
	"fpfmt_ss": "#e498f3",
	"fpfmt_c": "#17becf",
	"fpfmt": "#2ca02c",
	"uscale": "#1f77b4",
	"native_cpp":"#282BD2",
	"native_c": "#8c564b",
	"native_ruby": "#d62728",
	"native_scheme": "#9E36F8"
}

IMPL_ORDER = ["fpfmt", "fpfmt_unopt", "fpfmt_ss", "fpfmt_c", "uscale", "native_cpp", "native_c", "native_ruby", "native_scheme"]

def _match_impl_key(name):
	for key in sorted(IMPL_ORDER, key=len, reverse=True):
		if key in name:
			return key
	return None

def impl_color(name):
	key = _match_impl_key(name)
	return IMPL_COLORS[key] if key else "#000000"

def impl_order_key(name):
	key = _match_impl_key(name)
	return IMPL_ORDER.index(key) if key else len(IMPL_ORDER)

def load_scatter(pattern):
	files = glob.glob(os.path.join(RESULTS_DIR, pattern))
	if not files:
		return None
	dfs = []
	for f in sorted(files):
		try:
			dfs.append(pd.read_csv(f))
		except Exception as e:
			print(f"Warning: could not read {f}: {e}")
	if not dfs:
		return None
	return pd.concat(dfs, ignore_index=True)

def plot_scatter(df, title, out_path):
	fig, ax = plt.subplots(figsize=(10, 5))

	impls = sorted(df["impl"].unique(), key=impl_order_key)
	for impl in impls:
		sub = df[df["impl"] == impl]
		ax.scatter(sub["log2f"], sub["ns"], s=1, alpha=0.35, color=impl_color(impl), label=impl)

	ax.set_xlabel("log2(f)")
	ax.set_ylabel("time (ns) [log scale]")
	ax.set_yscale('log')
	ax.set_title(f"{title}\n{HW_LABEL}", fontsize=11)
	ax.legend(markerscale=8, loc="upper left", fontsize=9)
	ax.grid(True, alpha=0.3)
	ax.xaxis.set_major_locator(ticker.MultipleLocator(200))

	plt.tight_layout()
	plt.savefig(out_path, format="pdf")
	plt.close()
	print(f"  wrote {os.path.basename(out_path)}")
 
def plot_cdf(df, title, out_path):
	fig, ax = plt.subplots(figsize=(8, 5))

	impls = sorted(df["impl"].unique(), key=impl_order_key)
	for impl in impls:
		vals = df[df["impl"] == impl]["ns"].values.copy()
		vals.sort()

		n   = len(vals)
		lo  = int(n * CDF_TRIM)
		hi  = int(n * (1 - CDF_TRIM))
		vals = vals[lo:hi]

		n2 = len(vals)
		y  = np.arange(1, n2 + 1) / n2
		ax.plot(vals, y, color=impl_color(impl), label=impl, linewidth=1.5)

	ax.set_xlabel("time (ns)")
	ax.set_ylabel("fraction completed")
	ax.set_title(f"{title}\n{HW_LABEL}", fontsize=11)
	ax.legend(loc="lower right", fontsize=9)
	ax.grid(True, alpha=0.3)
	ax.set_ylim(0, 1)

	plt.tight_layout()
	plt.savefig(out_path, format="pdf")
	plt.close()
	print(f"  wrote {os.path.basename(out_path)}")
 
def out(name):
	return os.path.join(OUT_DIR, name)
 
def section(title):
	print(f"\n--- {title} ---")

def main():
	os.makedirs(OUT_DIR, exist_ok=True)

	# fixed width raw core algorithm
	df = load_scatter(f"scatter_fixed6.csv")
	if df is None:
		print(f"no data: scatter_fixed6.csv")
	section(f"fixed6")
	plot_scatter(df, f"FixedWidth(f, 6) — core algorithm", out(f"scatter_fixed6.pdf"))
	plot_cdf    (df, f"FixedWidth(f, 6) — core algorithm — CDF", out(f"cdf_fixed6.pdf"))

	df = load_scatter(f"scatter_fixed17.csv")
	if df is None:
		print(f"no data: scatter_fixed17.csv")
	section(f"fixed17")
	plot_scatter(df, f"FixedWidth(f, 17) — core algorithm", out(f"scatter_fixed17.pdf"))
	plot_cdf    (df, f"FixedWidth(f, 17) — core algorithm — CDF", out(f"cdf_fixed17.pdf"))

	# short raw core algorithm
	df = load_scatter("scatter_short.csv")
	if df is None:
		print("no data: scatter_short.csv")
	else:
		section("short")
		full = df[~df["impl"].str.contains("raw")]
		raw  = df[ df["impl"].str.contains("raw")]
		if not full.empty:
			plot_scatter(full, "Fmt(Short(f)) — core algorithm", out("scatter_short.pdf"))
			plot_cdf    (full, "Fmt(Short(f)) — core algorithm — CDF", out("cdf_short.pdf"))
		if not raw.empty:
			plot_scatter(raw, "Short(f) [raw]", out("scatter_short_raw.pdf"))
			plot_cdf    (raw, "Short(f) [raw] — CDF", out("cdf_short_raw.pdf"))

	# format complete
	df = load_scatter("scatter_format_short.csv")
	if df is None:
		print("no data: scatter_format_short.csv")
	else:
		section("format_short")
		plot_scatter(df, "Format(f) — complete", out("scatter_format_short.pdf"))
		plot_cdf    (df, "Format(f) — complete — CDF", out("cdf_format_short.pdf"))

	for nd in (6, 17):
		df = load_scatter(f"scatter_format_fixed{nd}.csv")
		if df is None:
			print(f"no data: scatter_format_fixed{nd}.csv")
			continue
		section(f"format_fixed{nd}")
		plot_scatter(df, f"Format(f, {nd}) — complete", out(f"scatter_format_fixed{nd}.pdf"))
		plot_cdf    (df, f"Format(f, {nd}) — complete — CDF", out(f"cdf_format_fixed{nd}.pdf"))

	# parse
	df = load_scatter("scatter_parse.csv")
	if df is None:
		print("no data: scatter_parse.csv")
	else:
		section("parse")
		raw  = df[ df["impl"].str.contains("raw")]
		text = df[~df["impl"].str.contains("raw")]
		if not raw.empty:
			plot_scatter(raw, "Parse(d, p) [raw]", out("scatter_parse_raw.pdf"))
			plot_cdf    (raw, "Parse(d, p) [raw] — CDF", out("cdf_parse_raw.pdf"))
		if not text.empty:
			plot_scatter(text, "Parse(text)", out("scatter_parse_text.pdf"))
			plot_cdf    (text, "Parse(text) — CDF", out("cdf_parse_text.pdf"))
 
if __name__ == "__main__":
	main()
