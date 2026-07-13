(define script-dir
  (let ([dir (path-parent (car (command-line)))])
    (if (string=? dir "") "." dir)
	)
)
(define results-dir (string-append script-dir "/results"))
(define floats-csv (string-append script-dir "/../data/bench_floats.csv"))
(define reps 25)
(define batch 100)

;;; -----------------------------------------------------------------------------------------------
;;; fpfmt FFI bindings -- loads the shared library built from src/fpfmt_ffi.cpp (see Makefile)
;;; -----------------------------------------------------------------------------------------------
(define (find-shared-lib base)
  (cond
    [(file-exists? (string-append base ".dylib")) (string-append base ".dylib")]
    [(file-exists? (string-append base ".so")) (string-append base ".so")]
    [else (error 'find-shared-lib "no shared library found for" base)]
	)
)

(load-shared-object (find-shared-lib (string-append script-dir "/libfpfmt_ffi")))

(define fpfmt-format-short-raw (foreign-procedure "fpfmt_format_short" (u8* double) int))
(define fpfmt-format-fixed-raw (foreign-procedure "fpfmt_format_fixed" (u8* double int) int))
(define fpfmt-parse-text-raw (foreign-procedure "fpfmt_parse_text" (string int) double))

(define fmt-buf (make-bytevector 64))

(define (buf->string n)
  (let ([out (make-bytevector n)])
    (bytevector-copy! fmt-buf 0 out 0 n)
    (utf8->string out)
	)
)

(define (fpfmt-format-short f) (buf->string (fpfmt-format-short-raw fmt-buf f)))
(define (fpfmt-format-fixed f nd) (buf->string (fpfmt-format-fixed-raw fmt-buf f nd)))
(define (fpfmt-parse-text s) (fpfmt-parse-text-raw s (string-length s)))

;;; -----------------------------------------------------------------------------------------------
;;; helpers
;;; -----------------------------------------------------------------------------------------------
(define (bits->double bits)
  ; Chez has no direct uint64-bits->double, so we build it via bytevectors
  (let ([bv (make-bytevector 8)])
		(bytevector-u64-set! bv 0 bits (native-endianness))
		(bytevector-ieee-double-ref bv 0 (native-endianness))
	)
)

(define (double->bits f)
  (let ([bv (make-bytevector 8)])
		(bytevector-ieee-double-set! bv 0 f (native-endianness))
		(bytevector-u64-ref bv 0 (native-endianness))
	)
)

(define (load-floats path)
  (call-with-input-file path
    (lambda (p)
      (get-line p) ; skip header
      (let loop ([acc '()])
        (let ([line (get-line p)])
          (if (eof-object? line)
              (reverse acc)
              (loop (cons (bits->double (string->number line)) acc))
          )
        )
      )
    )
  )
)

(define (now-ns)
  (let ([t (current-time)])
    (+ (* (time-second t) 1000000000) (time-nanosecond t))
	)
)

(define (timed-batch proc x b)
  (let ([start (now-ns)])
    (let loop ([i 0])
      (when (< i b)
        (proc x)
        (loop (+ i 1))
			)
		)
    (- (now-ns) start)
	)
)

(define (median lst)
  (let* ([sorted (list-sort < lst)] [len (length sorted)])
    (list-ref sorted (quotient len 2))
	)
)

(define (scatter inputs proc)
  (map
    (lambda (pair)
      (let* ([x (car pair)] [log2f (cdr pair)] [times (map (lambda (_) (timed-batch proc x batch)) (iota reps))] [med-ns (median times)] [ns-per-call (/ med-ns batch)])
        (cons log2f ns-per-call)
			)
		)
	inputs)
)

(define (append-scatter path impl points)
  (let* ([exists (file-exists? path)] [p (if exists (open-output-file path 'append) (open-output-file path))])
    (unless exists (display "impl,log2f,ns" p) (newline p))
    (for-each
      (lambda (pt)
        (display impl p) (display "," p)
        (display (format #f "~,6f" (car pt)) p) (display "," p)
        (display (format #f "~,2f" (cdr pt)) p)
        (newline p)
			)
    points)
    (close-port p)
	)
  (printf "appended ~a rows to ~a (impl=~a)~n" (length points) path impl)
)

(define (append-agg path impl op ns)
  (let* ([exists (file-exists? path)] [p (if exists (open-output-file path 'append) (open-output-file path))])
    (unless exists (display "impl,op,median_ns" p) (newline p))
    (display impl p) (display "," p)
    (display op p) (display "," p)
    (display (format #f "~,2f" ns) p)
    (newline p)
    (close-port p)
	)
  (printf "appended agg row to ~a (impl=~a, op=~a)~n" path impl op)
)

(define (append-validate path impl op values)
  (let* ([exists (file-exists? path)] [p (if exists (open-output-file path 'append) (open-output-file path))])
    (unless exists (display "impl,op,index,value" p) (newline p))
    (let loop ([vs values] [i 0])
      (unless (null? vs)
        (display impl p) (display "," p)
        (display op p) (display "," p)
        (display i p) (display "," p)
        (display (car vs) p)
        (newline p)
        (loop (cdr vs) (+ i 1))
			)
		)
    (close-port p)
	)
  (printf "appended ~a validate rows to ~a (impl=~a, op=~a)~n" (length values) path impl op)
)

;;; -----------------------------------------------------------------------------------------------
;;; main
;;; -----------------------------------------------------------------------------------------------
(define floats (load-floats floats-csv))
(printf "loaded ~a floats~n" (length floats))

;;; fpfmt_ss is benched through fpfmt::format(), same as fpfmt/native_cpp/native_ruby/
;;; native_scheme's format_* entries -- see the comment in bench_scheme.ss for why the
;;; wrapper (not the raw fixedWidth/shortWidth+fmt core) is the apples-to-apples
;;; comparison. This variant additionally pays for the Scheme->C FFI call overhead.
(define (scatter-inputs-for floats) (map (lambda (f) (cons f (/ (log (abs f)) (log 2)))) floats))

;;; fixed6
(let* ([points (scatter (scatter-inputs-for floats) (lambda (f) (fpfmt-format-fixed f 6)))])
  (append-scatter (string-append results-dir "/scatter_format_fixed6.csv") "fpfmt_ss" points)
  (append-agg (string-append results-dir "/bench_format.csv") "fpfmt_ss" "format_fixed6" (median (map cdr points)))
)

;;; fixed17
(let* ([points (scatter (scatter-inputs-for floats) (lambda (f) (fpfmt-format-fixed f 17)))])
  (append-scatter (string-append results-dir "/scatter_format_fixed17.csv") "fpfmt_ss" points)
  (append-agg (string-append results-dir "/bench_format.csv") "fpfmt_ss" "format_fixed17" (median (map cdr points)))
)

;;; short
(let* ([points (scatter (scatter-inputs-for floats) fpfmt-format-short)])
  (append-scatter (string-append results-dir "/scatter_format_short.csv") "fpfmt_ss" points)
  (append-agg (string-append results-dir "/bench_format.csv") "fpfmt_ss" "format_short" (median (map cdr points)))
)

;;; parse -- same parse-texts (Scheme's own shortest round-trip form) as bench_scheme.ss's
;;; parse benchmark, so both impls parse identical input text.
(define parse-texts (map number->string floats))
(let* ([scatter-inputs (map (lambda (s f) (cons s (/ (log (abs f)) (log 2)))) parse-texts floats)] [points (scatter scatter-inputs fpfmt-parse-text)])
  (append-scatter (string-append results-dir "/scatter_parse.csv") "fpfmt_ss" points)
  (append-agg (string-append results-dir "/bench_parse.csv") "fpfmt_ss" "parse_text" (median (map cdr points)))
)

;;; -----------------------------------------------------------------------------------------------
;;; validation -- actual output values per input, for bench_validation.py
;;; -----------------------------------------------------------------------------------------------
(define validate-csv (string-append results-dir "/validate.csv"))

(append-validate validate-csv "fpfmt_ss" "format_short" (map fpfmt-format-short floats))
(append-validate validate-csv "fpfmt_ss" "format_fixed6" (map (lambda (f) (fpfmt-format-fixed f 6)) floats))
(append-validate validate-csv "fpfmt_ss" "format_fixed17" (map (lambda (f) (fpfmt-format-fixed f 17)) floats))
(append-validate validate-csv "fpfmt_ss" "parse_text"
  (map (lambda (s) (number->string (double->bits (fpfmt-parse-text s)))) parse-texts))
