;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  simple-obj.scm
;;;  rudimentary "class" system for straight Scheme 
;;;
;;;  Bill Gribble <grib@billgribble.com> 20 Feb 2000 
;;;  $Id$
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(gnc:support "qif-import/simple-obj.scm")

;;  this is an extremely rudimentary object system.  Each object is a
;;  cons cell, where the car is a symbol with the class name and the
;;  cdr is a vector of the slots.  
;;
;; the "class object" is an instance of simple-class which just has
;; the name of the class and an alist of slot names to vector indices
;; as its slots.
;;
;; by convention, I name class objects (defined with make-simple-class)
;; <class-name> with class-smybol 'class-name.  For example,
;;
;; (define <test-class> (make-simple-class 'test-class '(slot-1 slot-2)))
;; (define t (make-simple-obj <test-class>))
;; t ==> (test-class . #(#f #f))

;; the 'simple-class' class.  
(define (make-simple-class class-symbol slot-names) 
  (make-record-type (symbol->string class-symbol) slot-names))

(define (simple-obj-getter class slot)  
  (record-accessor class slot))

(define (simple-obj-setter class slot)
  (record-modifier class slot))

(define (simple-obj-print obj)
  (write obj))

(define (make-simple-obj class)
  (let ((ctor (record-constructor class))
        (field-defaults 
         (map (lambda (v) #f) (record-type-fields class))))
    (apply ctor field-defaults)))
                 
