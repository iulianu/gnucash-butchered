;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  business-reports.scm
;;  load the business report definitions
;;
;;  Copyright (c) 2002 Derek Atkins <derek@ihtfp.com>
;;
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
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-module (gnucash report business-reports))
(use-modules (gnucash gnc-module))
(gnc:module-load "gnucash/report/standard-reports" 0)
(gnc:module-load "gnucash/app-utils" 0)

;; to define gnc-build-url
(gnc:module-load "gnucash/html" 0)

;; Guile 2 needs to find this macro at compile time already
(cond-expand
  (guile-2
    (eval-when
      (compile load eval) 
      (define gnc:menuname-business-reports (N_ "_Business"))))
  (else
    (define gnc:menuname-business-reports (N_ "_Business"))))

(define gnc:optname-invoice-number (N_ "Invoice Number"))

(define (guid-ref idstr type guid)
  (gnc-build-url type (string-append idstr guid) ""))

(define (gnc:customer-anchor-text customer)
  (guid-ref "customer=" URL-TYPE-CUSTOMER (gncCustomerReturnGUID customer)))

(define (gnc:job-anchor-text job)
  (guid-ref "job=" URL-TYPE-JOB (gncJobReturnGUID job)))

(define (gnc:vendor-anchor-text vendor)
  (guid-ref "vendor=" URL-TYPE-VENDOR (gncVendorReturnGUID vendor)))

(define (gnc:employee-anchor-text employee)
  (guid-ref "employee=" URL-TYPE-EMPLOYEE (gncEmployeeReturnGUID employee)))

(define (gnc:invoice-anchor-text invoice)
  (guid-ref "invoice=" URL-TYPE-INVOICE (gncInvoiceReturnGUID invoice)))

(define (gnc:owner-anchor-text owner)
  (let ((type (gncOwnerGetType (gncOwnerGetEndOwner owner))))
    (cond
      ((eqv? type GNC-OWNER-CUSTOMER)
       (gnc:customer-anchor-text (gncOwnerGetCustomer owner)))

      ((eqv? type GNC-OWNER-VENDOR)
       (gnc:vendor-anchor-text (gncOwnerGetVendor owner)))

      ((eqv? type GNC-OWNER-EMPLOYEE)
       (gnc:employee-anchor-text (gncOwnerGetEmployee owner)))

      ((eqv? type GNC-OWNER-JOB)
       (gnc:job-anchor-text (gncOwnerGetJob owner)))

      (else
       ""))))

(define (gnc:owner-report-text owner acc)
  (let* ((end-owner (gncOwnerGetEndOwner owner))
	 (type (gncOwnerGetType end-owner))
	 (ref #f))

    (cond
      ((eqv? type GNC-OWNER-CUSTOMER)
       (set! ref "owner=c:"))

      ((eqv? type GNC-OWNER-VENDOR)
       (set! ref "owner=v:"))

      ((eqv? type GNC-OWNER-EMPLOYEE)
       (set! ref "owner=e:"))

      (else (set! ref "unknown-type=")))

    (if ref
	(begin
	  (set! ref (string-append ref (gncOwnerReturnGUID end-owner)))
	  (if (not (null? acc))
	      (set! ref (string-append ref "&acct="
				       (gncAccountGetGUID acc))))
	  (gnc-build-url URL-TYPE-OWNERREPORT ref ""))
	ref)))

;; Creates a new report instance for the given invoice. The given
;; report name must be the name of one existing report template, which
;; is then used to instantiate the new report instance.
(define (gnc:invoice-report-create-withname invoice report-name)
  ;; Look up the internal template-id that belongs to the given report
  ;; name
  (let ((report-template-id
         (gnc:report-template-name-to-id report-name)))

    (if report-template-id
        ;; We found the report template id, so instantiate a report
        ;; and set the invoice option accordingly.
        (let* ((options (gnc:make-report-options report-template-id))
               (invoice-op (gnc:lookup-option options gnc:pagename-general gnc:optname-invoice-number)))

          (gnc:option-set-value invoice-op invoice)
          (gnc:make-report report-template-id options))
        ;; No report template id was found (probably the name has a
        ;; typo), so let's return zero as an invalid report id.
        0
        )))

(export gnc:invoice-report-create-withname)
(export gnc:menuname-business-reports gnc:optname-invoice-number)

(use-modules (gnucash report fancy-invoice))
(use-modules (gnucash report invoice))
(use-modules (gnucash report easy-invoice))
(use-modules (gnucash report taxinvoice))
(use-modules (gnucash report owner-report))
(use-modules (gnucash report job-report))
(use-modules (gnucash report payables))
(use-modules (gnucash report receivables))
(use-modules (gnucash report customer-summary))
(use-modules (gnucash report balsheet-eg))

(define (gnc:invoice-report-create invoice)
  (gnc:invoice-report-create-withname
   invoice
   ;; Feel free to insert a different invoice report name below, such
   ;; as "Tax Invoice"
   "Printable Invoice"))

;; Alternatively, if your preferred report has a "create-internal"
;; function, uncomment the line below and delete the ones above
;;(define gnc:invoice-report-create gnc:invoice-report-create-internal)

(define (gnc:payables-report-create account title show-zeros?)
  (payables-report-create-internal account title show-zeros?))

(define (gnc:receivables-report-create account title show-zeros?)
  (receivables-report-create-internal account title show-zeros?))

(export gnc:invoice-report-create
	gnc:customer-anchor-text gnc:job-anchor-text gnc:vendor-anchor-text
	gnc:invoice-anchor-text gnc:owner-anchor-text gnc:owner-report-text
	gnc:payables-report-create gnc:receivables-report-create)
(re-export gnc:owner-report-create)
