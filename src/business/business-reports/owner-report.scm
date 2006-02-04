;; -*-scheme-*-
;; owner-report.scm -- Print out a detailed owner report, which is a
;;		       summary of invoices and payments for a particular
;;		       company (the owner) applied to an account.
;;
;; Created by:  Derek Atkins <warlord@MIT.EDU>
;; Copyright (c) 2002, 2003 Derek Atkins <warlord@MIT.EDU>
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


(define-module (gnucash report owner-report))

(use-modules (srfi srfi-1))
(use-modules (ice-9 slib))
(use-modules (gnucash gnc-module))
(use-modules (gnucash main))		; for gnc:debug

(gnc:module-load "gnucash/report/report-system" 0)
(gnc:module-load "gnucash/business-utils" 0)

(use-modules (gnucash report standard-reports))
(use-modules (gnucash report business-reports))

(define acct-string (N_ "Account"))
(define owner-string (N_ "Company"))
(define owner-page gnc:pagename-general)

(define date-header (N_ "Date"))
(define due-date-header (N_ "Due Date"))
(define reference-header (N_ "Reference"))
(define type-header (N_ "Type"))
(define desc-header (N_ "Description"))
(define amount-header (N_ "Amount"))

(define-macro (addto! alist element)
  `(set! ,alist (cons ,element ,alist)))

(define (set-last-row-style! table tag . rest)
  (let ((arg-list 
         (cons table 
               (cons (- (gnc:html-table-num-rows table) 1)
                     (cons tag rest)))))
    (apply gnc:html-table-set-row-style! arg-list)))

(define (date-col columns-used)
  (vector-ref columns-used 0))
(define (date-due-col columns-used)
  (vector-ref columns-used 1))
(define (num-col columns-used)
  (vector-ref columns-used 2))
(define (type-col columns-used)
  (vector-ref columns-used 3))
(define (memo-col columns-used)
  (vector-ref columns-used 4))
(define (value-col columns-used)
  (vector-ref columns-used 5))

(define columns-used-size 6)

(define (build-column-used options)   
  (define (opt-val section name)
    (gnc:option-value 
     (gnc:lookup-option options section name)))
  (define (make-set-col col-vector)
    (let ((col 0))
      (lambda (used? index)
        (if used?
            (begin
              (vector-set! col-vector index col)
              (set! col (+ col 1)))
            (vector-set! col-vector index #f)))))
  
  (let* ((col-vector (make-vector columns-used-size #f))
         (set-col (make-set-col col-vector)))
    (set-col (opt-val "Display Columns" date-header) 0)
    (set-col (opt-val "Display Columns" due-date-header) 1)
    (set-col (opt-val "Display Columns" reference-header) 2)
    (set-col (opt-val "Display Columns" type-header) 3)
    (set-col (opt-val "Display Columns" desc-header) 4)
    (set-col (opt-val "Display Columns" amount-header) 5)
    col-vector))

(define (make-heading-list column-vector)
  (let ((heading-list '()))
    (if (date-col column-vector)
        (addto! heading-list (_ date-header)))
    (if (date-due-col column-vector)
        (addto! heading-list (_ due-date-header)))
    (if (num-col column-vector)
        (addto! heading-list (_ reference-header)))
    (if (type-col column-vector)
	(addto! heading-list (_ type-header)))
    (if (memo-col column-vector)
	(addto! heading-list (_ desc-header)))
    (if (value-col column-vector)
	(addto! heading-list (_ amount-header)))
    (reverse heading-list)))


(define num-buckets 4)
(define (new-bucket-vector)
  (make-vector num-buckets (gnc:numeric-zero)))

(define (make-interval-list to-date)
  (let ((begindate to-date))
    (set! begindate (decdate begindate ThirtyDayDelta))
    (set! begindate (decdate begindate ThirtyDayDelta))
    (set! begindate (decdate begindate ThirtyDayDelta))
    (gnc:make-date-list begindate to-date ThirtyDayDelta)))


(define (make-aging-table options query bucket-intervals reverse?)
  (let ((lots (gnc:query-get-lots query 'query-txn-match-any))
	(buckets (new-bucket-vector))
	(payments (gnc:numeric-zero))
	(currency (gnc:default-currency)) ;XXX
	(table (gnc:make-html-table)))

    (define (in-interval this-date current-bucket)
      (gnc:timepair-lt this-date current-bucket))

    (define (find-bucket current-bucket bucket-intervals date)
      (begin
	(if (>= current-bucket (vector-length bucket-intervals))
	    (gnc:error "sanity check failed in find-bucket")
	    (if (in-interval date (vector-ref bucket-intervals current-bucket))
		current-bucket
		(find-bucket (+ current-bucket 1) bucket-intervals date)))))

    (define (apply-invoice date value)
      (let* ((bucket-index (find-bucket 0 bucket-intervals date))
	     (new-value (gnc:numeric-add-fixed
			 value
			 (vector-ref buckets bucket-index))))
	(vector-set! buckets bucket-index new-value)))

    (define (apply-payment value)
      (set! payments (gnc:numeric-add-fixed value payments)))

    (for-each
     (lambda (lot)
       (let* ((bal (gnc:lot-get-balance lot))
	      (invoice (gnc:invoice-get-invoice-from-lot lot))
	      (post-date (gnc:invoice-get-date-posted invoice)))

	 (if (not (gnc:numeric-zero-p bal))
	     (begin
	       (if reverse?
		   (set! bal (gnc:numeric-neg bal)))
	       (if invoice
		   (begin
		     (apply-invoice post-date bal))
		   (apply-payment bal))))))
     lots)

    (gnc:html-table-set-col-headers!
     table
     (list (_ "0-30 days")
	   (_ "31-60 days")
	   (_ "61-90 days")
	   (_ "91+ days")))

    (gnc:html-table-append-row!
     table
     (reverse (map (lambda (entry)
		     (gnc:make-gnc-monetary currency entry))
		   (vector->list buckets))))

    table))
		 
;;
;; Make sure the caller checks the type first and only calls us with
;; invoice and payment transactions.  we don't verify it here.
;;
;; Return a list of (printed? value odd-row?)
;;
(define (add-txn-row table txn acc column-vector odd-row? printed?
		     inv-str reverse? start-date total)
  (let* ((type (gnc:transaction-get-txn-type txn))
	 (date (gnc:transaction-get-date-posted txn))
	 (due-date #f)
	 (value (gnc:transaction-get-account-value txn acc))
	 (split (gnc:transaction-get-split txn 0))
	 (invoice (gnc:invoice-get-invoice-from-txn txn))
	 (currency (gnc:transaction-get-currency txn))
	 (type-str
	  (cond
	   ((equal? type gnc:transaction-type-invoice)
	    (if invoice
		(gnc:make-html-text
		 (gnc:html-markup-anchor
		  (gnc:invoice-anchor-text invoice)
		  inv-str))
		inv-str))
	   ((equal? type gnc:transaction-type-payment) (_ "Payment, thank you"))
	   (else (_ "Unknown"))))
	 )

    (define (make-row date due-date num type-str memo value)
      (let ((row-contents '()))
	(if (date-col column-vector)
	    (addto! row-contents (gnc:print-date date)))
	(if (date-due-col column-vector)
	    (addto! row-contents 
		    (if (and due-date
			     (not (equal? due-date (cons 0 0))))
			(gnc:print-date due-date)
			"")))
	(if (num-col column-vector)
	    (addto! row-contents num))
	(if (type-col column-vector)
	    (addto! row-contents type-str))
	(if (memo-col column-vector)
	    (addto! row-contents memo))
	(if (value-col column-vector)
	    (addto! row-contents
		    (gnc:make-html-table-cell/markup "number-cell"
						     (gnc:make-gnc-monetary
						      currency value))))
	row-contents))

    (if reverse?
	(set! value (gnc:numeric-neg value)))

    (if (gnc:timepair-later start-date date)
	(begin

	  ; Maybe print out the 'balance' row
	  (if (not printed?)
	      (begin
		(set! printed? #t)
		(if (not (gnc:numeric-zero-p total))
		    (let ((row (make-row start-date #f "" (_ "Balance") "" total))
			  (row-style (if odd-row? "normal-row" "alternate-row")))
		      (gnc:html-table-append-row/markup! table row-style
							 (reverse row))
		      (set! odd-row? (not odd-row?))
		      (set! row-style (if odd-row? "normal-row" "alternate-row")))
		    )))
	  
	  ; Now print out the invoice row
	  (if invoice
	      (set! due-date (gnc:invoice-get-date-due invoice)))

	  (let ((row (make-row date due-date (gnc:transaction-get-num txn)
			       type-str (gnc:split-get-memo split) value))
		(row-style (if odd-row? "normal-row" "alternate-row")))

	    (gnc:html-table-append-row/markup! table row-style
					       (reverse row)))

	  (set! odd-row? (not odd-row?))
	  ))

    (list printed? value odd-row?)
    ))


(define (make-txn-table options query acc start-date end-date)
  (let ((txns (gnc:query-get-transactions query 'query-txn-match-any))
	(used-columns (build-column-used options))
	(total (gnc:numeric-zero))
	(currency (gnc:default-currency)) ;XXX
	(table (gnc:make-html-table))
	(inv-str (gnc:option-value (gnc:lookup-option options "__reg"
						      "inv-str")))
	(reverse? (gnc:option-value (gnc:lookup-option options "__reg"
						      "reverse?"))))

    (gnc:html-table-set-col-headers!
     table
     (make-heading-list used-columns))

    ; Order the transactions properly
    (set! txns (sort txns (lambda (a b) (> 0 (gnc:transaction-order a b)))))

    (let ((printed? #f)
	  (odd-row? #t))
      (for-each
       (lambda (txn)
	 (let ((type (gnc:transaction-get-txn-type txn)))
	   (if
	    (or (equal? type gnc:transaction-type-invoice)
		(equal? type gnc:transaction-type-payment))
	    (let ((result (add-txn-row table txn acc used-columns odd-row? printed?
				       inv-str reverse? start-date total)))

	      (set! printed? (car result))
	      (set! total (gnc:numeric-add-fixed total (cadr result)))
	      (set! odd-row? (caddr result))
	      ))))
       txns))

    (gnc:html-table-append-row/markup! 
     table
     "grand-total"
     (append (cons (gnc:make-html-table-cell/markup
		    "total-label-cell"
		    (if (gnc:numeric-negative-p total)
			(_ "Total Credit")
			(_ "Total Due")))
		   '())
	     (list (gnc:make-html-table-cell/size/markup
		    1 (value-col used-columns)
		    "total-number-cell"
		    (gnc:make-gnc-monetary currency total)))))

    (let* ((interval-vec (list->vector (make-interval-list end-date))))
      (gnc:html-table-append-row/markup!
       table
       "grand-total"
       (list (gnc:make-html-table-cell/size/markup
	      1 (+ 1 (value-col used-columns))
	      "centered-label-cell"
	      (make-aging-table options query interval-vec reverse?)))))

    table))

(define (options-generator acct-type-list owner-type inv-str reverse?)

  (define gnc:*report-options* (gnc:new-options))

  (define (gnc:register-inv-option new-option)
    (gnc:register-option gnc:*report-options* new-option))

  (gnc:register-inv-option
   (gnc:make-internal-option "__reg" "inv-str" inv-str))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option "__reg" "reverse?" "" "" reverse?))

  (gnc:register-inv-option
   (gnc:make-owner-option owner-page owner-string "v"
			  (N_ "The company for this report")
			  (lambda () #f) #f owner-type))

  (gnc:register-inv-option
   (gnc:make-internal-option "__reg" "owner-type" owner-type))

  (gnc:register-inv-option
   (gnc:make-account-sel-limited-option owner-page acct-string "w"
					(N_ "The account to search for transactions")
					#f #f acct-type-list))

  (gnc:options-add-date-interval!
   gnc:*report-options* gnc:pagename-general
   (N_ "From") (N_ "To") "a")

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") date-header
    "b" (N_ "Display the transaction date?") #t))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") due-date-header
    "c" (N_ "Display the transaction date?") #t))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") reference-header
    "d" (N_ "Display the transaction reference?") #t))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") type-header
    "g" (N_ "Display the transaction type?") #t))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") desc-header
    "ha" (N_ "Display the transaction description?") #t))

  (gnc:register-inv-option
   (gnc:make-simple-boolean-option
    (N_ "Display Columns") amount-header
    "hb" "Display the transaction amount?" #t))

  (gnc:register-inv-option
   (gnc:make-string-option
    gnc:pagename-general (N_ "Today Date Format")
    "p" (N_ "The format for the date->string conversion for today's date.")
    "%B %e, %Y"))

  (gnc:options-set-default-section gnc:*report-options* "General")

  gnc:*report-options*)
	     
(define (customer-options-generator)
  (options-generator '(receivable) 'gnc-owner-customer (_ "Invoice") #f))

(define (vendor-options-generator)
  (options-generator '(payable) 'gnc-owner-vendor (_ "Bill") #t))

(define (employee-options-generator)
  (options-generator '(payable) 'gnc-owner-employee (_ "Expense Report") #t))

(define (string-expand string character replace-string)
  (define (car-line chars)
    (take-while (lambda (c) (not (eqv? c character))) chars))
  (define (cdr-line chars)
    (let ((rest (drop-while (lambda (c) (not (eqv? c character))) chars)))
      (if (null? rest)
          '()
          (cdr rest))))
  (define (line-helper chars)
    (if (null? chars)
        ""
        (let ((first (car-line chars))
              (rest (cdr-line chars)))
          (string-append (list->string first)
                         (if (null? rest) "" replace-string)
                         (line-helper rest)))))
  (line-helper (string->list string)))

(define (setup-query q owner account end-date)
  (let* ((guid (gnc:owner-get-guid (gnc:owner-get-end-owner owner))))

    (gnc:query-add-guid-match
     q 
     (list gnc:split-trans gnc:invoice-from-txn gnc:invoice-owner
	   gnc:owner-parentg)
     guid 'query-or)
    (gnc:query-add-guid-match
     q
     (list gnc:split-lot gnc:owner-from-lot gnc:owner-parentg)
     guid 'query-or)
    (gnc:query-add-guid-match
     q
     (list gnc:split-lot gnc:invoice-from-lot gnc:invoice-owner
	   gnc:owner-parentg)
     guid 'query-or)

    (gnc:query-add-single-account-match q account 'query-and)
    (gnc:query-add-date-match-timepair q #f end-date #t end-date 'query-and)
    (gnc:query-set-book q (gnc:get-current-book))
    q))

(define (make-owner-table owner)
  (let ((table (gnc:make-html-table)))
    (gnc:html-table-set-style!
     table "table"
     'attribute (list "border" 0)
     'attribute (list "cellspacing" 0)
     'attribute (list "cellpadding" 0))
    (gnc:html-table-append-row!
     table
     (list
      (string-expand (gnc:owner-get-address-dep owner) #\newline "<br>")))
    (gnc:html-table-append-row!
     table
     (list "<br>"))
    (set-last-row-style!
     table "td"
     'attribute (list "valign" "top"))
    table))

(define (make-date-row! table label date)
  (gnc:html-table-append-row!
   table
   (list
    (string-append label ":&nbsp;")
    (string-expand (gnc:print-date date) #\space "&nbsp;"))))

(define (make-date-table)
  (let ((table (gnc:make-html-table)))
    (gnc:html-table-set-style!
     table "table"
     'attribute (list "border" 0)
     'attribute (list "cellpadding" 0))
    (set-last-row-style!
     table "td"
     'attribute (list "valign" "top"))
    table))

(define (make-myname-table book date-format)
  (let* ((table (gnc:make-html-table))
	 (slots (gnc:book-get-slots book))
	 (name (gnc:kvp-frame-get-slot-path
		slots (append gnc:*kvp-option-path*
			      (list gnc:*business-label* gnc:*company-name*))))
	 (addy (gnc:kvp-frame-get-slot-path
		slots (append gnc:*kvp-option-path*
			      (list gnc:*business-label* gnc:*company-addy*)))))

    (gnc:html-table-set-style!
     table "table"
     'attribute (list "border" 0)
     'attribute (list "align" "right")
     'attribute (list "valign" "top")
     'attribute (list "cellspacing" 0)
     'attribute (list "cellpadding" 0))

    (gnc:html-table-append-row! table (list (if name name "")))
    (gnc:html-table-append-row! table (list (string-expand
					     (if addy addy "")
					     #\newline "<br>")))
    (gnc:html-table-append-row! table (list
				       (strftime
					date-format
					(localtime (car (gnc:get-today))))))
    table))

(define (make-break! document)
  (gnc:html-document-add-object!
   document
   (gnc:make-html-text
    (gnc:html-markup-br))))

(define (reg-renderer report-obj)
  (define (opt-val section name)
    (gnc:option-value
     (gnc:lookup-option (gnc:report-options report-obj) section name)))

  (let* ((document (gnc:make-html-document))
	 (table '())
	 (orders '())
	 (query (gnc:malloc-query))
	 (account (opt-val owner-page acct-string))
	 (owner (opt-val owner-page owner-string))
	 (start-date (gnc:timepair-start-day-time 
		       (gnc:date-option-absolute-time
			(opt-val gnc:pagename-general (N_ "From")))))
	 (end-date (gnc:timepair-end-day-time 
		       (gnc:date-option-absolute-time
			(opt-val gnc:pagename-general (N_ "To")))))
	 (title (string-append (_ type-str) (_ " Report"))))
	 (book (gnc:get-current-book)) ;XXX Grab this from elsewhere
	 (owner-type (opt-val "__reg" "owner-type"))
	 (type-str ""))

    (case owner-type
      ((gnc-owner-customer)
       (set! type-str (N_ "Customer")))
      ((gnc-owner-vendor)
       (set! type-str (N_ "Vendor")))
      ((gnc-owner-employee)
       (set! type-str (N_ "Employee"))))

    (gnc:html-document-set-title! document title)

    (if (gnc:owner-is-valid? owner)
	(begin
	  (setup-query query owner account end-date)

	  (gnc:html-document-set-title!
           (string-append (_ type-str ) (_ " Report: ") (gnc:owner-get-name owner)))

           (gnc:html-document-set-headline!
            document (gnc:html-markup
                      "!" 
                      (_ type-str )
                      (_ " Report: ")
                      (gnc:html-markup-anchor
                       (gnc:owner-anchor-text owner)
                       (gnc:owner-get-name owner))))
	  
	  (if account
	      (begin
		(set! table (make-txn-table (gnc:report-options report-obj)
					    query account start-date end-date))
		(gnc:html-table-set-style!
		 table "table"
		 'attribute (list "border" 1)
		 'attribute (list "cellspacing" 0)
		 'attribute (list "cellpadding" 4)))

	      (set!
	       table
	       (gnc:make-html-text
		(string-append 
		 "No Valid Account Selected.  "
		 "Click on the Options button and select the account to use."))))

	  (gnc:html-document-add-object!
	   document
	   (make-myname-table book (opt-val gnc:pagename-general (N_ "Today Date Format"))))

	  (gnc:html-document-add-object!
	   document
	   (make-owner-table owner))

	  (make-break! document)

	  (gnc:html-document-add-object!
	   document
	   (gnc:make-html-text
	    (string-append
	     (_ "Date Range")
	     ": "
	     (gnc:print-date start-date)
	     " - "
	     (gnc:print-date end-date))))

	  (make-break! document)

	  (gnc:html-document-add-object! document table))

	;; else....
	(gnc:html-document-add-object!
	 document
	 (gnc:make-html-text
	  (string-append
	   "No Valid " type-str	" Selected.  "
	   "Click on the Options button to select a company."))))

    (gnc:free-query query)
    document))

(define (find-first-account type)
  (define (find-first group num index)
    (if (>= index num)
	#f
	(let* ((this-account (gnc:group-get-account group index))
	       (account-type (gw:enum-<gnc:AccountType>-val->sym
			      (gnc:account-get-type this-account) #f)))
	  (if (eq? account-type type)
	      this-account
	      (find-first group num (+ index 1))))))

  (let* ((current-group (gnc:get-current-group))
	 (num-accounts (gnc:group-get-num-accounts
			current-group)))
    (if (> num-accounts 0)
	(find-first current-group num-accounts 0)
	#f)))

(define (find-first-account-for-owner owner)
  (let ((type (gw:enum-<gnc:GncOwnerType>-val->sym
	       (gnc:owner-get-type (gnc:owner-get-end-owner owner)) #f)))
    (case type
      ((gnc-owner-customer)
       (find-first-account 'receivable))

      ((gnc-owner-vendor)
       (find-first-account 'payable))

      ((gnc-owner-employee)
       (find-first-account 'payable))

      ((gnc-owner-job)
       (find-first-account-for-owner (gnc:owner-get-end-owner owner)))

      (else
       #f))))

(gnc:define-report
 'version 1
 'name (N_ "Customer Report")
 'menu-path (list gnc:menuname-business-reports)
 'options-generator customer-options-generator
 'renderer reg-renderer
 'in-menu? #t)

(gnc:define-report
 'version 1
 'name (N_ "Vendor Report")
 'menu-path (list gnc:menuname-business-reports)
 'options-generator vendor-options-generator
 'renderer reg-renderer
 'in-menu? #t)

(gnc:define-report
 'version 1
 'name (N_ "Employee Report")
 'menu-path (list gnc:menuname-business-reports)
 'options-generator employee-options-generator
 'renderer reg-renderer
 'in-menu? #t)

(define (owner-report-create-internal report-name owner account)
  (let* ((options (gnc:make-report-options report-name))
	 (owner-op (gnc:lookup-option options owner-page owner-string))
	 (account-op (gnc:lookup-option options owner-page acct-string)))

    (gnc:option-set-value owner-op owner)
    (gnc:option-set-value account-op account)
    (gnc:make-report report-name options)))

(define (owner-report-create owner account)
  (let ((type (gw:enum-<gnc:GncOwnerType>-val->sym
	       (gnc:owner-get-type (gnc:owner-get-end-owner owner)) #f)))
    (case type
      ((gnc-owner-customer)
       (owner-report-create-internal (N_ "Customer Report") owner account))

      ((gnc-owner-vendor)
       (owner-report-create-internal (N_ "Vendor Report") owner account))

      ((gnc-owner-employee)
       (owner-report-create-internal (N_ "Employee Report") owner account))

      (else #f))))

(define (gnc:owner-report-create owner account)
  ; Figure out an account to use if nothing exists here.
  (if (not account)
      (set! account (find-first-account-for-owner owner)))

  (owner-report-create owner account))

(define (gnc:owner-report-create-internal
	 account split query journal? double? title
	 debit-string credit-string)

  (let* ((temp-owner (gnc:owner-create))
	 (owner (gnc:owner-from-split split temp-owner))
	 (res #f))

    (if owner
	(set! res (gnc:owner-report-create owner account)))

    (gnc:owner-destroy temp-owner)
    res))

(gnc:register-report-hook 'receivable #t
			  gnc:owner-report-create-internal)

(gnc:register-report-hook 'payable #t
			  gnc:owner-report-create-internal)

(export gnc:owner-report-create)
