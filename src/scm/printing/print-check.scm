;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  print-check.scm
;;;  print a check from a transaction. 
;;;
;;;  Copyright 2000 Bill Gribble <grib@billgribble.com>
;;;  $Id$
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(use-modules (gnucash printing number-to-words))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  <print-check-format> class
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define <print-check-format>
  (make-simple-class 
   'print-check-format
   '(format 
     position
     date-format
     custom-info)))

(define print-check-format?
  (record-predicate <print-check-format>))

(define print-check-format:format
  (simple-obj-getter <print-check-format> 'format))

(define print-check-format:set-format!
  (simple-obj-setter <print-check-format> 'format))

(define print-check-format:position
  (simple-obj-getter <print-check-format> 'position))

(define print-check-format:set-position!
  (simple-obj-setter <print-check-format> 'position))

(define print-check-format:date-format
  (simple-obj-getter <print-check-format> 'date-format))

(define print-check-format:set-date-format!
  (simple-obj-setter <print-check-format> 'date-format))

(define print-check-format:custom-info
  (simple-obj-getter <print-check-format> 'custom-info))

(define print-check-format:set-custom-info!
  (simple-obj-setter <print-check-format> 'custom-info))
   
(define (make-print-check-format fmt pos dateformat cust)
  (let ((retval (make-simple-obj <print-check-format>)))
    (print-check-format:set-format! retval fmt)
    (print-check-format:set-position! retval pos)
    (print-check-format:set-date-format! retval dateformat)
    (print-check-format:set-custom-info! retval cust)
    retval))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  stock formats 
;;  units for stock formats and positions are points (72/inch)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define gnc:*stock-check-formats*
  '((deluxe  . ((payee . (126.0 150.0))
		(amount-words . (90.0 125.0))
		(amount-number . (400.0 150.0))
		(date . (343.0 183.0))
		(memo . (105.0 73.0))
		(rotate . 90.0)
		(translate . (232.0 300.0))
		(offset . 0.0)))
    (quicken  . ((payee . (90.0 150.0))
		 (amount-words . (90.0 120.0))
		 (amount-number . (500.0 150.0))
		 (date . (500.0 185.0))
		 (memo . (50.0 40.0))))
    ))

(define gnc:*stock-check-positions*
  '((top . 540.0)
    (middle . 288.0)
    (bottom . 36.0)))

(define (gnc:print-check format-info payee amount date memo)
    (let* ((int-part (inexact->exact (truncate amount)))
           (frac-part (inexact->exact 
                     (truncate 
                      (+ (/ .5 100) (* 100 (- amount int-part))))))
           (ps (gnc:print-session-create #t))
           (format #f)
           (offset #f)
           (date-string ""))
      (if (not (eq? (print-check-format:format format-info) 'custom))
          (begin 
            (set! format (assq (print-check-format:format format-info) 
                               gnc:*stock-check-formats*))
            (if (pair? format)
		(begin
		  (set! format (cdr format))
		  (let ((off (assq 'offset format)))
		    (if off (set! offset (cdr off)))))))
          (set! format (print-check-format:custom-info format-info)))
      
      (if (not (or offset (eq? (print-check-format:position format-info) 'custom)))
          (begin 
            (set! offset 
                  (cdr (assq (print-check-format:position format-info)
                             gnc:*stock-check-positions*)))
            (if (pair? offset)
                (set! offset (cdr offset))))
          (set! offset
                (cdr (assq 'position 
                           (print-check-format:custom-info format-info)))))
      
      (let ((fmt (print-check-format:date-format format-info)))
        (if (string=? fmt "custom")
            (let* ((custom-info (print-check-format:custom-info format-info))
                   (date-fmt (assq 'date-format custom-info)))
              (if date-fmt 
                  (set! date-fmt (cdr date-fmt)))
              (set! date-string
                    (strftime date-fmt (localtime date))))
            (begin 
              (set! date-string (strftime fmt (localtime date))))))            

      (let ((translate-pos (assq 'translate format)))
	(if translate-pos
	    (gnc:print-session-translate ps (cadr translate-pos)
					 (caddr translate-pos))))

      (let ((rotate-angle (assq 'rotate format)))
	(if rotate-angle (gnc:print-session-rotate ps (cdr rotate-angle))))

      (let ((date-pos (assq 'date format)))
        (gnc:print-session-moveto ps (cadr date-pos) 
                                  (+ offset (caddr date-pos)))
        (gnc:print-session-text ps date-string))
      
      (let ((payee-pos (assq 'payee format)))
        (gnc:print-session-moveto ps (cadr payee-pos) 
                                  (+ offset (caddr payee-pos)))
        (gnc:print-session-text ps payee))
      
      (let ((number-pos (assq 'amount-number format)))
        (gnc:print-session-moveto ps (cadr number-pos) 
                                  (+ offset (caddr number-pos)))
        (gnc:print-session-text ps (printable-value amount 100)))
      
      (let ((words-pos (assq 'amount-words format)))
        (gnc:print-session-moveto ps (cadr words-pos) 
                                  (+ offset (caddr words-pos)))
        (gnc:print-session-text ps (number-to-words amount 100)))

      (let ((memo-pos (assq 'memo format)))
        (gnc:print-session-moveto ps (cadr memo-pos) 
                                  (+ offset (caddr memo-pos)))
        (gnc:print-session-text ps memo))

      (gnc:print-session-done ps #t)
      (gnc:print-session-print ps)))

