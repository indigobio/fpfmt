# frozen_string_literal: true

require 'csv'

###################################################################################################
# helpers
###################################################################################################
WORKING_DIR = __dir__
RESULTS_DIR = File.join(WORKING_DIR, 'results')
FLOATS_CSV  = File.join(WORKING_DIR, '..', 'data', 'bench_floats.csv')
REPS   = 25
BATCH  = 100

def load_floats(path)
  CSV.read(path, headers: true).map { |row| [row['bits'].to_i].pack('Q').unpack1('d') }
end

def now_ns
  Process.clock_gettime(Process::CLOCK_MONOTONIC, :nanosecond)
end

def scatter(inputs, reps: REPS, batch: BATCH)
  inputs.map do |x, log2f|
    times = Array.new(reps) do
      t = now_ns
      batch.times { yield x }
      (now_ns - t).to_f / batch
    end
    times.sort!
    [log2f, times[reps / 2]]
  end
end

###################################################################################################
# csv helpers
###################################################################################################
def ensure_header(path, header)
  return if File.exist?(path)

  File.write(path, "#{header}\n")
end

def append_scatter(path, impl, points)
  ensure_header(path, 'impl,log2f,ns')
  CSV.open(path, 'a') do |csv|
    points.each { |log2f, ns| csv << [impl, log2f.round(6), ns.round(2)] }
  end
  puts "appended #{points.size} rows to #{path} (impl=#{impl})"
end

def append_agg(path, impl, op, ns)
  ensure_header(path, 'impl,op,median_ns')
  CSV.open(path, 'a') { |csv| csv << [impl, op, ns.round(2)] }
  puts "appended agg row to #{path} (impl=#{impl}, op=#{op}, ns=#{ns.round(2)})"
end

def append_validate(path, impl, op, values)
  ensure_header(path, 'impl,op,index,value')
  CSV.open(path, 'a') do |csv|
    values.each_with_index { |v, i| csv << [impl, op, i, v] }
  end
  puts "appended #{values.size} validate rows to #{path} (impl=#{impl}, op=#{op})"
end

###################################################################################################
# main
###################################################################################################
floats = load_floats(FLOATS_CSV)
inputs = floats.map { |f| [f, Math.log2(f.abs)] }

puts "loaded #{floats.size} floats"

# format fixed
[6, 17].each do |nd|
  points = scatter(inputs) { |f| format("%.#{nd - 1}e", f) }
  append_scatter(File.join(RESULTS_DIR, "scatter_format_fixed#{nd}.csv"), 'native_ruby', points)

  median_all = points.map { |_, ns| ns }.sort[points.size / 2]
  append_agg(File.join(RESULTS_DIR, "bench_format.csv"), 'native_ruby', "format_fixed#{nd}", median_all)
end

# format short
points = scatter(inputs, &:to_s)
append_scatter(File.join(RESULTS_DIR, 'scatter_format_short.csv'), 'native_ruby', points)
median_all = points.map { |_, ns| ns }.sort[points.size / 2]
append_agg(File.join(RESULTS_DIR, 'bench_format.csv'), 'native_ruby', 'format_short', median_all)

# parse
texts = floats.map { |f| format('%.16e', f) }
text_inputs = texts.zip(floats).map { |s, f| [s, Math.log2(f.abs)] }

points = scatter(text_inputs, &:to_f)
append_scatter(File.join(RESULTS_DIR, 'scatter_parse.csv'), 'native_ruby', points)
median_all = points.map { |_, ns| ns }.sort[points.size / 2]
append_agg(File.join(RESULTS_DIR, 'bench_parse.csv'), 'native_ruby', 'parse_text', median_all)

###################################################################################################
# validation -- actual output values per input, for bench_validation.py
###################################################################################################
validate_csv = File.join(RESULTS_DIR, 'validate.csv')

append_validate(validate_csv, 'native_ruby', 'format_short', floats.map(&:to_s))
[6, 17].each do |nd|
  values = floats.map { |f| format("%.#{nd - 1}e", f) }
  append_validate(validate_csv, 'native_ruby', "format_fixed#{nd}", values)
end

parsed_bits = texts.map { |s| [s.to_f].pack('d').unpack1('Q').to_s }
append_validate(validate_csv, 'native_ruby', 'parse_text', parsed_bits)
