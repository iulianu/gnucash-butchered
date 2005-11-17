;; This program is free software; you can redistribute it and/or    
;; modify it under the terms of the GNU General Public License as   
;; published by the Free Software Foundation; either version 2 of   
;; the License, or (at your option) any later version.              
;;                                                                  
;; This program is distributed in the hope that it will be useful,  
;; but WITHOUT ANY WARRANTY; without even the implied warranty of   
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    
;; GNU General Public License for more details.                     
;;                                                                  
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, contact:
;;
;; Free Software Foundation           Voice:  +1-617-542-5942
;; 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
;; Boston, MA  02110-1301,  USA       gnu@gnu.org

;; Copyright 2002 Joshua Sled <jsled@asynchronous.org>
;;
;; pretty literal copies of similar code from gnumeric-1.0.8, except we want
;; positive values to be returned [as gnucash will handle the credit/debit
;; appropriately]

(define (gnc:ipmt rate per nper pv fv type)
  (* -1 (* rate
           (- 0 (calc-principal pv
                                (calc-pmt rate nper pv fv type)
                                rate (- per 1)))
        ))
)

(define (gnc:ppmt rate per nper pv fv type)
  (let* ((pmt (calc-pmt rate nper pv fv type))
         (ipmt (* rate
                  (calc-principal pv pmt rate (- per 1)))))
    (* -1 (-
           pmt
           (* -1 ipmt))))
)

(define (gnc:pmt rate nper pv fv type)
  (* -1 (calc-pmt rate nper pv fv type)))

(define (gnc:foobar val) val)

;;;;;
;; below: not-exposed/"private" functions, used by the "public" functions
;; above.
;;;;;

(define (calc-pmt rate nper pv fv type)
  (let ((pvif (calc-pvif rate nper))
        (fvifa (calc-fvifa rate nper)))
   (/ (- (* (- 0 pv) pvif) fv)
      (* fvifa 
         (+ 1.0
            (* rate type)))))
)

(define (calc-pvif rate nper)
  (expt (+ 1 rate) nper)
)

(define (calc-fvifa rate nper)
  (/ (- (expt (+ 1 rate) nper) 1) rate)
)

(define (calc-principal pv pmt rate per)
  (+ (* pv (expt (+ 1.0 rate) per))
     (* pmt (/ (- (expt (+ 1 rate) per)
                  1)
               rate)))
)

