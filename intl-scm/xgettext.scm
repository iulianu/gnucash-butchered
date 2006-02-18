;; Find translatable strings in guile files
(define *base-dir-len* 0)

(define (expand-newlines string out-port)
  (define (output-prefix-newlines chars)
    (if (and (pair? chars) (char=? (car chars) #\newline))
        (begin
          (display "\\n" out-port)
          (output-prefix-newlines (cdr chars)))
        chars))

  (let loop ((chars (string->list string))
             (accum '()))
    (cond
     ((null? chars)
      (if (not (null? accum))
          (write (list->string (reverse accum)) out-port)))
     ((char=? (car chars) #\newline)
      (write (list->string (reverse accum)) out-port)
      (display "\"" out-port)
      (set! chars (output-prefix-newlines chars))
      (display "\"" out-port)
      (if (not (null? chars))
          (display "\n  " out-port))
      (loop chars '()))
     (else
      (loop (cdr chars) (cons (car chars) accum))))))

(define (write-string string out-port filename line-number)
  (display (string-append "/* " (substring filename *base-dir-len*)) out-port)
  ;;(display line-number out-port)
  (display " */\n" out-port)
  (display "_(" out-port)
  (expand-newlines string out-port)
  (display ")\n" out-port))

(define (find-strings-in-item item out-port filename line-no)
  (define (find-internal rest)
    (cond
     ((and (list? rest)                    ; if it's a list
           (= (length rest) 2)             ; of length 2
           (symbol? (car rest))            ; starting with a symbol
           (string? (cadr rest))           ; and ending with a string
           (or (eqv? '_ (car rest))        ; and the symbol is _
               (eqv? 'N_ (car rest))       ; or N_
               (eqv? 'gnc:_ (car rest))))  ; or gnc:_
      (write-string (cadr rest) out-port filename line-no)) ; then write it out

     ((pair? rest)                         ; otherwise, recurse
      (find-internal (car rest))
      (find-internal (cdr rest)))))

  (find-internal item))

(define (count-newlines string)
  (define (count-internal from)
    (let ((index (string-index string #\newline from)))
      (if index
	  (+ 1 (count-internal (+ 1 index)))
	  0)))
  (count-internal 0))

(define (find-strings in-port out-port filename)
  (do ((item (read in-port) (read in-port))
       (line-no 1 (+ 1 line-no)))
      ((eof-object? item) #t)
    (find-strings-in-item item out-port filename line-no)))

(let ((out-port (open "guile-strings.c" (logior O_WRONLY O_CREAT O_TRUNC)))
      (base-dir (cadr (command-line)))
      (in-files (cddr (command-line))))
  (set! *base-dir-len* (+ (string-length base-dir) 1))
  (for-each (lambda (file)
              (call-with-input-file file (lambda (port)
                                           (find-strings port out-port file))))
            in-files))
