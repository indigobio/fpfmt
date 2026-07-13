(print-subnormal-precision #t)

(define script-dir
  (let ([dir (path-parent (car (command-line)))])
    (if (string=? dir "") "." dir)
	)
)
(define results-dir (string-append script-dir "/results"))
(define floats-csv (string-append script-dir "/../data/bench_floats.csv"))
(define reps 25)
(define batch 100)

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

; formats f to nd significant digits in exponential notation, e.g. "1.234e+05"
(define (fixed-width-string f nd)
  (format #f (string-append "~," (number->string (- nd 1)) "E") f)
)

;;; fixed width
;;; Compared against fpfmt::format(f, nd), NOT the raw fixedWidth+fmt core --
;;; Scheme's ~E formatting and number->string always pay for sign/zero/inf/nan
;;; handling internally, same as fpfmt's format() wrapper, so this is the
;;; apples-to-apples comparison. Comparing them against the bare fixedWidth/fmt
;;; internals (which explicitly skip that handling) would give fpfmt an
;;; unearned speed advantage.
;;; fixed6
(let* ([scatter-inputs (map (lambda (f) (cons f (/ (log (abs f)) (log 2)))) floats)] [points (scatter scatter-inputs (lambda (f) (fixed-width-string f 6)))])
  (append-scatter (string-append results-dir "/scatter_format_fixed6.csv") "native_scheme" points)
  (append-agg (string-append results-dir "/bench_format.csv") "native_scheme" "format_fixed6" (median (map cdr points)))
)

;;; fixed17
(let* ([scatter-inputs (map (lambda (f) (cons f (/ (log (abs f)) (log 2)))) floats)] [points (scatter scatter-inputs (lambda (f) (fixed-width-string f 17)))])
  (append-scatter (string-append results-dir "/scatter_format_fixed17.csv") "native_scheme" points)
  (append-agg (string-append results-dir "/bench_format.csv") "native_scheme" "format_fixed17" (median (map cdr points)))
)

;;; short
(let* ([scatter-inputs (map (lambda (f) (cons f (/ (log (abs f)) (log 2)))) floats)] [points (scatter scatter-inputs (lambda (f) (number->string f)))])
  (append-scatter (string-append results-dir "/scatter_format_short.csv") "native_scheme" points)
  (append-agg (string-append results-dir "/bench_format.csv") "native_scheme" "format_short" (median (map cdr points)))
)

;;; parse
(define parse-texts (map number->string floats))
(let* ([scatter-inputs (map (lambda (s f) (cons s (/ (log (abs f)) (log 2)))) parse-texts floats)] [points (scatter scatter-inputs (lambda (s) (string->number s)))])
  (append-scatter (string-append results-dir "/scatter_parse.csv") "native_scheme" points)
  (append-agg (string-append results-dir "/bench_parse.csv") "native_scheme" "parse_text" (median (map cdr points)))
)

;;; -----------------------------------------------------------------------------------------------
;;; validation -- actual output values per input, for bench_validation.py
;;; -----------------------------------------------------------------------------------------------
(define validate-csv (string-append results-dir "/validate.csv"))

(append-validate validate-csv "native_scheme" "format_short" (map number->string floats))
(append-validate validate-csv "native_scheme" "format_fixed6" (map (lambda (f) (fixed-width-string f 6)) floats))
(append-validate validate-csv "native_scheme" "format_fixed17" (map (lambda (f) (fixed-width-string f 17)) floats))
(append-validate validate-csv "native_scheme" "parse_text"
  (map (lambda (s) (number->string (double->bits (string->number s)))) parse-texts))