import os
import glob
import platform
import subprocess
import pandas as pd
import numpy as np
import plotly.graph_objects as go

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "../bench/results")
OUT_DIR = os.path.join(SCRIPT_DIR, "interactive_graphs")
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
			import winreg
			key = winreg.OpenKey(
				winreg.HKEY_LOCAL_MACHINE,
				r"HARDWARE\DESCRIPTION\System\CentralProcessor\0"
			)
			name, _ = winreg.QueryValueEx(key, "ProcessorNameString")
			return name.strip()
	except Exception:
		pass
	return None

def hardware_label():
	"""e.g. 'Darwin x86_64 -- Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz', falling
	back to just OS/machine if the CPU brand can't be determined here."""
	base = f"{platform.system() or 'unknown OS'} {platform.machine() or ''}".strip()
	brand = _cpu_brand()
	return f"{base} -- {brand}" if brand else base

HW_LABEL = hardware_label()

IMPL_COLORS = {
	"fpfmt_unopt":  "#f69948",
	"fpfmt_ss":     "#e498f3",
	"fpfmt_c":      "#17becf",
	"fpfmt":        "#2ca02c",
	"uscale": 			"#1f77b4",
	"native_cpp":		"#282BD2",
	"native_c":     "#8c564b",
	"native_ruby":  "#d62728",
	"native_scheme":"#9E36F8",
}

IMPL_ORDER = ["fpfmt", "fpfmt_unopt", "fpfmt_ss", "fpfmt_c", "uscale", "native_cpp", "native_c", "native_ruby", "native_scheme"]

PLOTLY_CONFIG = {
	"scrollZoom": True,
	"displayModeBar": True,
	"displaylogo": False,
	"responsive": True,
	"modeBarButtonsToRemove": [
			"select2d", "lasso2d", "zoom2d", "zoomIn2d", "zoomOut2d", "autoScale2d", "pan2d",
	],
}

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
	return pd.concat(dfs, ignore_index=True) if dfs else None

def scatter_traces(df):
	impls = sorted(df["impl"].unique(), key=impl_order_key)
	traces = []
	for impl in impls:
		sub = df[df["impl"] == impl]
		traces.append(go.Scattergl(
			x=sub["log2f"],
			y=sub["ns"],
			mode="markers",
			marker=dict(size=3, color=impl_color(impl), opacity=0.45),
			name=impl,
			legendgroup=impl,
			showlegend=True,
		))
	return traces

def cdf_traces(df):
	impls = sorted(df["impl"].unique(), key=impl_order_key)
	traces = []
	for impl in impls:
		vals = df[df["impl"] == impl]["ns"].values.copy()
		vals.sort()
		n = len(vals)
		lo = int(n * CDF_TRIM)
		hi = int(n * (1 - CDF_TRIM))
		vals = vals[lo:hi]
		n2 = len(vals)
		y = np.arange(1, n2 + 1) / n2
		traces.append(go.Scatter(
			x=vals,
			y=y,
			mode="lines",
			line=dict(color=impl_color(impl), width=1.8),
			name=impl,
			legendgroup=impl,
			showlegend=True,
		))
	return traces

def base_layout(title):
	return dict(
		title=f"{title}<br><span style='font-size:0.7em;color:#666'>{HW_LABEL}</span>",
		height=520,
		dragmode="pan",
		legend=dict(
			itemsizing="constant",
			font=dict(size=11),
			orientation="h",
			yanchor="top",
			y=-0.18,
			xanchor="center",
			x=0.5,
		),
		margin=dict(t=60, b=100, l=60, r=30),
		template="plotly_white",
	)

def make_scatter_figure(df, title):
	fig = go.Figure(data=scatter_traces(df))
	fig.update_xaxes(title_text="log2(f)", dtick=200)
	fig.update_yaxes(title_text="time (ns)", type="log")
	fig.update_layout(**base_layout(title))
	return fig

def make_cdf_figure(df, title):
	fig = go.Figure(data=cdf_traces(df))
	fig.update_xaxes(title_text="time (ns)")
	fig.update_yaxes(title_text="fraction completed", range=[0, 1], fixedrange=True)
	fig.update_layout(**base_layout(title))
	return fig

RESIZE_FIX_SCRIPT = """
<script>
  function fixPlotSizing() {
    document.querySelectorAll('.plotly-graph-div').forEach(function (gd) {
      if (window.Plotly) Plotly.Plots.resize(gd);
    });
  }
  window.addEventListener('load', function () {
    fixPlotSizing();
    setTimeout(fixPlotSizing, 100);
  });
</script>
"""

SHIFT_DRAG_ZOOM_SCRIPT = """
<script>
  var shiftZoomActive = false;
  function setDragMode(mode) {
    document.querySelectorAll('.plotly-graph-div').forEach(function (gd) {
      if (window.Plotly && gd._fullLayout) Plotly.relayout(gd, {dragmode: mode});
    });
  }
  window.addEventListener('keydown', function (e) {
    if (e.key === 'Shift' && !shiftZoomActive) {
      shiftZoomActive = true;
      setDragMode('zoom');
    }
  });
  window.addEventListener('keyup', function (e) {
    if (e.key === 'Shift') {
      shiftZoomActive = false;
      setDragMode('pan');
    }
  });
  window.addEventListener('blur', function () {
    shiftZoomActive = false;
    setDragMode('pan');
  });
</script>
"""

def figs_to_row_html(figs, first_on_page):
	divs = []
	for i, fig in enumerate(figs):
		include_js = "cdn" if (first_on_page and i == 0) else False
		div = fig.to_html(full_html=False, include_plotlyjs=include_js, config=PLOTLY_CONFIG)
		divs.append(f'<div style="flex:1 1 0; min-width:0;">{div}</div>')
	return f'<div style="display:flex; gap:16px; flex-wrap:wrap;">{"".join(divs)}</div>'

def write_page(path, title, figs):
	body = figs_to_row_html(figs, first_on_page=True)
	html = f"""<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>{title}</title>
  <style>
    body {{ background:#f8f8f8; margin:0; padding:24px 32px; font-family:sans-serif; }}
    h1   {{ font-size:1.2rem; margin-bottom:8px; color:#222; }}
  </style>
</head>
<body>
  <h1>{title} &mdash; {HW_LABEL}</h1>
  {body}
  {RESIZE_FIX_SCRIPT}
</body>
</html>"""
	with open(path, "w") as fh:
		fh.write(html)

def section(title):
	print(f"\n--- {title} ---")

def out(name):
	return os.path.join(OUT_DIR, name)

def add_section(sections, name, df, scatter_title, cdf_title, out_name):
	fig_scatter = make_scatter_figure(df, scatter_title)
	fig_cdf = make_cdf_figure(df, cdf_title)
	write_page(out(out_name), scatter_title, [fig_scatter, fig_cdf])
	print(f"  wrote {out_name}")
	sections.append((name, [fig_scatter, fig_cdf]))

def main():
	os.makedirs(OUT_DIR, exist_ok=True)

	sections = []

	# fixed6 raw core algorithm
	df6 = load_scatter("scatter_fixed6.csv")
	if df6 is not None:
		section("fixed6")
		add_section(sections, "fixed6", df6, "FixedWidth(f, 6) — core algorithm", "FixedWidth(f, 6) — core algorithm — CDF", "int_fixed6.html")
	else:
		print("no data: scatter_fixed6.csv")

	# fixed17
	df17 = load_scatter("scatter_fixed17.csv")
	if df17 is not None:
		section("fixed17")
		add_section(sections, "fixed17", df17, "FixedWidth(f, 17) — core algorithm", "FixedWidth(f, 17) — core algorithm — CDF", "int_fixed17.html")
	else:
		print("no data: scatter_fixed17.csv")

	# short
	dfs = load_scatter("scatter_short.csv")
	if dfs is not None:
		section("short")
		full = dfs[~dfs["impl"].str.contains("raw")]
		raw  = dfs[ dfs["impl"].str.contains("raw")]
		if not full.empty:
			add_section(sections, "short", full, "Fmt(Short(f)) — core algorithm", "Fmt(Short(f)) — core algorithm — CDF", "int_short.html")
		if not raw.empty:
			add_section(sections, "short_raw", raw, "Short(f) [raw]", "Short(f) [raw] — CDF", "int_short_raw.html")
	else:
		print("no data: scatter_short.csv")

	# format complete
	dff = load_scatter("scatter_format_short.csv")
	if dff is not None:
		section("format_short")
		add_section(sections, "format_short", dff, "Format(f) — complete", "Format(f) — complete — CDF", "int_format_short.html")
	else:
		print("no data: scatter_format_short.csv")

	for nd in (6, 17):
		dffx = load_scatter(f"scatter_format_fixed{nd}.csv")
		if dffx is not None:
			section(f"format_fixed{nd}")
			add_section(sections, f"format_fixed{nd}", dffx, f"Format(f, {nd}) — complete", f"Format(f, {nd}) — complete — CDF", f"int_format_fixed{nd}.html")
		else:
			print(f"no data: scatter_format_fixed{nd}.csv")

	# parse
	dfp = load_scatter("scatter_parse.csv")
	if dfp is not None:
		section("parse")
		raw  = dfp[ dfp["impl"].str.contains("raw")]
		text = dfp[~dfp["impl"].str.contains("raw")]
		if not raw.empty:
			add_section(sections, "parse_raw", raw, "Parse(d, p) [raw]", "Parse(d, p) [raw] — CDF", "int_parse_raw.html")
		if not text.empty:
			add_section(sections, "parse_text", text, "Parse(text)", "Parse(text) — CDF", "int_parse_text.html")
	else:
		print("no data: scatter_parse.csv")

	# combined single-file dashboard
	if sections:
		html_parts = []
		first = True
		for name, figs in sections:
			row = figs_to_row_html(figs, first_on_page=first)
			html_parts.append(f"<h2 style='font-family:sans-serif;margin:32px 0 8px'>{name}</h2>\n{row}")
			first = False

		combined = f"""<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>fpfmt benchmarks</title>
  <style>
    body {{ background:#f8f8f8; margin:0; padding:24px 32px; }}
    h1   {{ font-family:sans-serif; font-size:1.4rem; margin-bottom:8px; color:#222; }}
  </style>
</head>
<body>
  <h1>fpfmt benchmark results &mdash; {HW_LABEL}</h1>
  {''.join(html_parts)}
  {RESIZE_FIX_SCRIPT}
</body>
</html>"""

		combined_path = out("int_all.html")
		with open(combined_path, "w") as fh:
			fh.write(combined)
		print(f"\n  wrote int_all.html (all graphs combined)")

if __name__ == "__main__":
	main()