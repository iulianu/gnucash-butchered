;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; budget.scm: budget report
;;
;; (C) 2005 by Chris Shoemaker <c.shoemaker@cox.net>
;;
;; based on cash-flow.scm by:
;; Herbert Thoma <herbie@hthoma.de>
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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-module (gnucash report budget))
(use-modules (gnucash main)) ;; FIXME: delete after we finish modularizing.
(use-modules (ice-9 slib))
(use-modules (gnucash gnc-module))

(require 'printf)

(gnc:module-load "gnucash/report/report-system" 0)
(gnc:module-load "gnucash/gnome-utils" 0) ;for gnc-build-url

(define reportname (N_ "Budget Report"))

;; define all option's names so that they are properly defined
;; in *one* place.
;;(define optname-from-date (N_ "From"))
;;(define optname-to-date (N_ "To"))

(define optname-display-depth (N_ "Account Display Depth"))
(define optname-show-subaccounts (N_ "Always show sub-accounts"))
(define optname-accounts (N_ "Account"))

(define optname-price-source (N_ "Price Source"))
(define optname-show-rates (N_ "Show Exchange Rates"))
(define optname-show-full-names (N_ "Show Full Account Names"))
(define optname-select-columns (N_ "Select Columns"))
(define optname-show-budget (N_ "Show Budget"))
(define optname-show-actual (N_ "Show Actual"))
(define optname-show-difference (N_ "Show Difference"))
(define opthelp-show-budget (N_ "Display a column for the budget values"))
(define opthelp-show-actual (N_ "Display a column for the actual values"))
(define opthelp-show-difference (N_ "Display the difference as budget - actual"))
(define optname-show-totalcol (N_ "Show Column with Totals"))
(define optname-rollup-budget (N_ "Roll up budget amounts to parent"))
(define opthelp-rollup-budget (N_ "If parent account does not have its own budget value, use the sum of the child account budget values"))
(define opthelp-show-totalcol (N_ "Display a column with the row totals"))
(define optname-bottom-behavior (N_ "Flatten list to depth limit"))
(define opthelp-bottom-behavior
  (N_ "Displays accounts which exceed the depth limit at the depth limit"))

(define optname-budget (N_ "Budget"))

;; options generator
(define (budget-report-options-generator)
  (let* ((options (gnc:new-options)) 
    (add-option 
     (lambda (new-option)
       (gnc:register-option options new-option))))

    (gnc:register-option
     options
     (gnc:make-budget-option
      gnc:pagename-general optname-budget
      "a" (N_ "Budget")))

    ;; date interval
    ;;(gnc:options-add-date-interval!
    ;; options gnc:pagename-general
    ;; optname-from-date optname-to-date "a")

    (gnc:options-add-price-source!
     options gnc:pagename-general optname-price-source "c" 'average-cost)

    ;;(gnc:register-option
    ;; options
    ;; (gnc:make-simple-boolean-option
    ;;  gnc:pagename-general optname-show-rates
    ;;  "d" (N_ "Show the exchange rates used") #f))

    (gnc:register-option
     options
     (gnc:make-simple-boolean-option
      gnc:pagename-general optname-show-full-names
      "e" (N_ "Show full account names (including parent accounts)") #t))

    ;; accounts to work on
    (gnc:options-add-account-selection!
     options gnc:pagename-accounts
     optname-display-depth optname-show-subaccounts
     optname-accounts "a" 2
     (lambda ()
       (gnc:filter-accountlist-type
        (list ACCT-TYPE-ASSET ACCT-TYPE-LIABILITY ACCT-TYPE-INCOME
                          ACCT-TYPE-EXPENSE)
        (gnc-account-get-descendants-sorted (gnc-get-current-root-account))))
     #f)
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-accounts optname-bottom-behavior
      "c" opthelp-bottom-behavior #f))

    ;; columns to display
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-display optname-show-budget
      "s1" opthelp-show-budget #t))
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-display optname-show-actual
      "s2" opthelp-show-actual #t))
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-display optname-show-difference
      "s3" opthelp-show-difference #f))
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-display optname-show-totalcol
      "s4" opthelp-show-totalcol #f))
    (add-option
     (gnc:make-simple-boolean-option
      gnc:pagename-display optname-rollup-budget
      "s4" opthelp-rollup-budget #f))

      ;; Set the general page as default option tab
    (gnc:options-set-default-section options gnc:pagename-general)

    options)
  )

;; Create the html table for the budget report
;;
;; Parameters
;;   html-table - HTML table to fill in
;;   acct-table - Table of accounts to use
;;   budget - budget to use
;;   params - report parameters
(define (gnc:html-table-add-budget-values!
         html-table acct-table budget params)
  (let* ((get-val (lambda (alist key)
                    (let ((lst (assoc-ref alist key)))
                      (if lst (car lst) lst))))
         (show-actual? (get-val params 'show-actual))
         (show-budget? (get-val params 'show-budget))
         (show-diff? (get-val params 'show-difference))
         (show-totalcol? (get-val params 'show-totalcol))
		 (rollup-budget? (get-val params 'rollup-budget))
        )
  
  ;; Calculate the sum of all budgets of all children of an account for a specific period
  ;;
  ;; Parameters:
  ;;   budget - budget to use
  ;;   children - list of children
  ;;   period - budget period to use
  ;;
  ;; Return value:
  ;;   budget value to use for account for specified period.
  (define (budget-account-sum budget children period)
    (let* ((sum (cond
                   ((null? children) (gnc-numeric-zero))
                   (else (gnc-numeric-add
				             (gnc:get-account-period-rolledup-budget-value budget (car children) period)
                             (budget-account-sum budget (cdr children) period)
                             GNC-DENOM-AUTO GNC-RND-ROUND))
                 )
          ))
    sum)
  )

  ;; Calculate the value to use for the budget of an account for a specific period.
  ;; 1) If the account has a budget value set for the period, use it
  ;; 2) If the account has children, use the sum of budget values for the children
  ;; 3) Otherwise, use 0.
  ;;
  ;; Parameters:
  ;;   budget - budget to use
  ;;   acct - account
  ;;   period - budget period to use
  ;;
  ;; Return value:
  ;;   sum of all budgets for list of children for specified period.
  (define (gnc:get-account-period-rolledup-budget-value budget acct period)
    (let* ((bgt-set? (gnc-budget-is-account-period-value-set budget acct period))
          (children (gnc-account-get-children acct))
          (amount (cond
                    (bgt-set? (gnc-budget-get-account-period-value budget acct period))
            ((not (null? children)) (budget-account-sum budget children period))
            (else (gnc-numeric-zero)))
          ))
    amount)
  )

  (define (negative-numeric-p x)
    (if (gnc-numeric-p x) (gnc-numeric-negative-p x) #f))
  (define (number-cell-tag x)
    (if (negative-numeric-p x) "number-cell-neg" "number-cell"))
  (define (total-number-cell-tag x)
    (if (negative-numeric-p x) "total-number-cell-neg" "total-number-cell"))

  ;; Adds a line to tbe budget report.
  ;;
  ;; Parameters:
  ;;   html-table - html table being created
  ;;   rownum - row number
  ;;   colnum - starting column number
  ;;   budget - budget to use
  ;;   acct - account being displayed
  ;;   rollup-budget? - rollup budget values for account children if account budget not set
  ;;   exchange-fn - exchange function (not used)
  (define (gnc:html-table-add-budget-line!
           html-table rownum colnum
           budget acct rollup-budget? exchange-fn)
    (let* ((num-periods (gnc-budget-get-num-periods budget))
           (period 0)
           (current-col (+ colnum 1))
           (bgt-total (gnc-numeric-zero))
           (bgt-total-unset? #t)
           (act-total (gnc-numeric-zero))
           (comm (xaccAccountGetCommodity acct))
           (reverse-balance? (gnc-reverse-balance acct))
           )
      (while (< period num-periods)
             (let* (
                    
                    ;; budgeted amount
                    (bgt-numeric-val (if rollup-budget?
                                     (gnc:get-account-period-rolledup-budget-value budget acct period)
                                     (gnc-budget-get-account-period-value budget acct period)))
                    (bgt-unset? (if rollup-budget?
                                (gnc-numeric-zero-p bgt-numeric-val)
                                (not (gnc-budget-is-account-period-value-set budget acct period))))
                    (bgt-val (if bgt-unset? "."
                                 (gnc:make-gnc-monetary comm bgt-numeric-val)))

                    ;; actual amount
                    (act-numeric-abs (gnc-budget-get-account-period-actual-value 
                                  budget acct period))
                    (act-numeric-val (if reverse-balance?
                         (gnc-numeric-neg act-numeric-abs)
                         act-numeric-abs))
                    (act-val (gnc:make-gnc-monetary comm act-numeric-val))

                    ;; difference (budget to actual)
                    (dif-numeric-val (gnc-numeric-sub bgt-numeric-val
                                 act-numeric-val GNC-DENOM-AUTO
                                 (+ GNC-DENOM-LCD GNC-RND-NEVER)))
                    (dif-val #f)
                    )

               (if (eq? ACCT-TYPE-INCOME (xaccAccountGetType acct))
                 (set! dif-numeric-val (gnc-numeric-neg dif-numeric-val)))
               (set! dif-val (if (and bgt-unset? (gnc-numeric-zero-p act-numeric-val)) "."
                                 (gnc:make-gnc-monetary comm dif-numeric-val)))
    		   (if (not bgt-unset?)
			     (begin
			   		(set! bgt-total (gnc-numeric-add bgt-total bgt-numeric-val GNC-DENOM-AUTO GNC-RND-ROUND))
					(set! bgt-total-unset? #f))
				 )
			   (set! act-total (gnc-numeric-add act-total act-numeric-val GNC-DENOM-AUTO GNC-RND-ROUND))
               (if show-budget?
                 (begin
                   (gnc:html-table-set-cell/tag!
                    html-table rownum current-col "number-cell" bgt-val)
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-actual?
                 (begin
                   (gnc:html-table-set-cell/tag!
                    html-table rownum current-col (number-cell-tag act-numeric-val) act-val)
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-diff?
                 (begin
                   (gnc:html-table-set-cell/tag!
                    html-table rownum current-col (number-cell-tag dif-numeric-val) dif-val)
                   (set! current-col (+ current-col 1))
                 )
               )
               (set! period (+ period 1))
             )
        )

		;; Totals
		(if show-totalcol?
		   (begin
              (if show-budget?
                 (begin
                   (gnc:html-table-set-cell/tag!
                          html-table rownum current-col "total-number-cell"
                          (if bgt-total-unset? "."
                               (gnc:make-gnc-monetary comm bgt-total)))
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-actual?
                 (begin
                   (gnc:html-table-set-cell/tag!
                    html-table rownum current-col (total-number-cell-tag act-total)
                          (gnc:make-gnc-monetary comm act-total))
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-diff?
                 (let* ((dif-total 
                              (gnc-numeric-sub bgt-total
                                       act-total GNC-DENOM-AUTO
                                       (+ GNC-DENOM-LCD GNC-RND-NEVER)))
                        (dif-val #f)
                       )
                   (if (eq? ACCT-TYPE-INCOME (xaccAccountGetType acct))
                     (set! dif-total (gnc-numeric-neg dif-total)))
                   (set! dif-val (if (and bgt-total-unset? (gnc-numeric-zero-p act-total)) "."
                                 (gnc:make-gnc-monetary comm dif-total)))
                   (gnc:html-table-set-cell/tag!
                    html-table rownum current-col (total-number-cell-tag dif-total)
                    dif-val
                   )
                   (set! current-col (+ current-col 1))
                 )
               )
	        )
		 )
      )
    )
  (define (gnc:html-table-add-budget-headers!
           html-table colnum budget)
    (let* ((num-periods (gnc-budget-get-num-periods budget))
           (period 0)
           (current-col (+ colnum 1))
           )

      ;; prepend 2 empty rows
      (gnc:html-table-prepend-row! html-table '())
      (gnc:html-table-prepend-row! html-table '())

	  (while (< period num-periods)
             (let* (
					 (tc #f)
                     (date (gnc-budget-get-period-start-date budget period))
                   )
               (gnc:html-table-set-cell!
                  html-table 0 (+ current-col period)
				  (gnc-print-date date))
               (set! tc (gnc:html-table-get-cell html-table 0 (+ current-col period)))
			   (gnc:html-table-cell-set-colspan! tc (if show-diff? 3 2))
			   (gnc:html-table-cell-set-tag! tc "centered-label-cell")
               (set! period (+ period 1))
             )
      )
      (if show-totalcol?
         (let* (
            (tc #f))
           (gnc:html-table-set-cell/tag!
            html-table 0 (+ current-col num-periods) "centered-label-cell"
            "Total")
           (set! tc (gnc:html-table-get-cell html-table 0 (+ current-col num-periods)))
           (gnc:html-table-cell-set-colspan! tc (if show-diff? 3 2))
         )
      )

      ;; make the column headers
      (set! period 0)
      (while (< period num-periods)
             (if show-budget?
               (begin
                 (gnc:html-table-set-cell/tag!
                  html-table 1 current-col "centered-label-cell"
                  (_ "Bgt")) ;; Translators: Abbreviation for "Budget"
                  (set! current-col (+ current-col 1))
               )
             )
             (if show-actual?
               (begin 
                 (gnc:html-table-set-cell/tag!
                  html-table 1 current-col "centered-label-cell"
                  (_ "Act")) ;; Translators: Abbreviation for "Actual"
                 (set! current-col (+ current-col 1))
               )
             )
             (if show-diff?
               (begin 
                 (gnc:html-table-set-cell/tag!
                  html-table 1 current-col "centered-label-cell"
                  (_ "Diff")) ;; Translators: Abbrevation for "Difference"
                 (set! current-col (+ current-col 1))
               )
             )
             (set! period (+ period 1))
             )
		 (if show-totalcol?
		    (begin
               (if show-budget?
                 (begin
                   (gnc:html-table-set-cell/tag!
                    html-table 1 current-col "centered-label-cell"
					(_ "Bgt")) ;; Translators: Abbreviation for "Budget"
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-actual?
                 (begin 
                   (gnc:html-table-set-cell/tag!
                    html-table 1 current-col "centered-label-cell"
					(_ "Act")) ;; Translators: Abbreviation for "Actual"
                   (set! current-col (+ current-col 1))
                 )
               )
               (if show-diff?
                 (begin 
                   (gnc:html-table-set-cell/tag!
                    html-table 1 current-col "centered-label-cell"
					(_ "Diff")) ;; Translators: Abbrevation for "Difference"
                   (set! current-col (+ current-col 1))
                 )
               )
			)
		 )
      )
    )

  (let* ((num-rows (gnc:html-acct-table-num-rows acct-table))
         (rownum 0)
         (numcolumns (gnc:html-table-num-columns html-table))
	 ;;(html-table (or html-table (gnc:make-html-table)))
         ;; WARNING: we implicitly depend here on the details of
         ;; gnc:html-table-add-account-balances.  Specifically, we
         ;; assume that it makes twice as many columns as it uses for
          ;; account labels.  For now, that seems to be a valid
         ;; assumption.
         (colnum (quotient numcolumns 2))

	 )

    ''(display (list "colnum: " colnum  "numcolumns: " numcolumns))
    ;; call gnc:html-table-add-budget-line! for each account
    (while (< rownum num-rows)
           (let* ((env (append
			(gnc:html-acct-table-get-row-env acct-table rownum)
			params))
                  (acct (get-val env 'account))
                  (exchange-fn (get-val env 'exchange-fn))
                  )
             (gnc:html-table-add-budget-line!
              html-table rownum colnum
              budget acct rollup-budget? exchange-fn)
             (set! rownum (+ rownum 1)) ;; increment rownum
             )
           ) ;; end of while

    ;; column headers
    (gnc:html-table-add-budget-headers! html-table colnum budget)

    )
    )
  ) ;; end of define

;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; budget-renderer
;; set up the document and add the table
;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (budget-renderer report-obj)
  (define (get-option pagename optname)
    (gnc:option-value
     (gnc:lookup-option
      (gnc:report-options report-obj) pagename optname)))

  (gnc:report-starting reportname)

  ;; get all option's values
  (let* ((budget (get-option gnc:pagename-general optname-budget))
         (budget-valid? (and budget (not (null? budget))))
         (display-depth (get-option gnc:pagename-accounts
                                    optname-display-depth))
         (show-subaccts? (get-option gnc:pagename-accounts
                                     optname-show-subaccounts))
         (accounts (get-option gnc:pagename-accounts
                               optname-accounts))
	     (bottom-behavior (get-option gnc:pagename-accounts optname-bottom-behavior))
         (rollup-budget? (get-option gnc:pagename-display
                                     optname-rollup-budget))
         (row-num 0) ;; ???
         (work-done 0)
         (work-to-do 0)
         ;;(report-currency (get-option gnc:pagename-general
         ;;                             optname-report-currency))
         (show-full-names? (get-option gnc:pagename-general
                                       optname-show-full-names))
         (doc (gnc:make-html-document))
         ;;(table (gnc:make-html-table))
         ;;(txt (gnc:make-html-text))
         )

    ;; is account in list of accounts?
    (define (same-account? a1 a2)
      (string=? (gncAccountGetGUID a1) (gncAccountGetGUID a2)))

    (define (same-split? s1 s2)
      (string=? (gncSplitGetGUID s1) (gncSplitGetGUID s2)))

    (define account-in-list?
      (lambda (account accounts)
        (cond
          ((null? accounts) #f)
          ((same-account? (car accounts) account) #t)
          (else (account-in-list? account (cdr accounts))))))

    (define split-in-list?
      (lambda (split splits)
	(cond
	 ((null? splits) #f)
	 ((same-split? (car splits) split) #t)
	 (else (split-in-list? split (cdr splits))))))

    (define account-in-alist
      (lambda (account alist)
        (cond
	   ((null? alist) #f)
           ((same-account? (caar alist) account) (car alist))
           (else (account-in-alist account (cdr alist))))))

    ;; helper for sorting of account list
    (define (account-full-name<? a b)
      (string<? (gnc-account-get-full-name a) (gnc-account-get-full-name b)))

    ;; helper for account depth
    (define (accounts-get-children-depth accounts)
      (apply max
	     (map (lambda (acct)
		    (let ((children (gnc-account-get-children acct)))
		      (if (null? children)
			  1
			  (+ 1 (accounts-get-children-depth children)))))
		  accounts)))
    ;; end of defines

    ;; add subaccounts if requested
    (if show-subaccts?
        (let ((sub-accounts (gnc:acccounts-get-all-subaccounts accounts)))
          (for-each
            (lambda (sub-account)
              (if (not (account-in-list? sub-account accounts))
                  (set! accounts (append accounts sub-accounts))))
            sub-accounts)))

    (cond
      ((null? accounts)
        ;; No accounts selected.
        (gnc:html-document-add-object! 
         doc 
         (gnc:html-make-no-account-warning 
	  reportname (gnc:report-id report-obj))))
      ((not budget-valid?)
        ;; No budget selected.
        (gnc:html-document-add-object!
          doc (gnc:html-make-generic-budget-warning reportname)))
      (else (begin
        (let* ((tree-depth (if (equal? display-depth 'all)
                               (accounts-get-children-depth accounts)
                               display-depth))
               ;;(account-disp-list '())

               ;; Things seem to crash if I don't set 'end-date to
               ;; _something_ but the actual value isn't used.
               (env (list (list 'end-date (gnc:get-today))
                          (list 'display-tree-depth tree-depth)
		 				  (list 'depth-limit-behavior
						             (if bottom-behavior 'flatten 'summarize))
                          ))
               (acct-table #f)
               (html-table (gnc:make-html-table))
               (params '())
               (paramsBudget
                (list
                 (list 'show-actual
                       (get-option gnc:pagename-display optname-show-actual))
                 (list 'show-budget
                       (get-option gnc:pagename-display optname-show-budget))
                 (list 'show-difference
                       (get-option gnc:pagename-display optname-show-difference))
                 (list 'show-totalcol
                       (get-option gnc:pagename-display optname-show-totalcol))
                 (list 'rollup-budget
                       (get-option gnc:pagename-display optname-rollup-budget))
                )
               )
               (report-name (get-option gnc:pagename-general
                                        gnc:optname-reportname))
               )

          (gnc:html-document-set-title!
           doc (sprintf #f (_ "%s: %s")
                        report-name (gnc-budget-get-name budget)))

          (set! accounts (sort accounts account-full-name<?))

          (set! acct-table
                (gnc:make-html-acct-table/env/accts env accounts))

          ;; We do this in two steps: First the account names...  the
          ;; add-account-balances will actually compute and add a
          ;; bunch of current account balances, too, but we'll
          ;; overwrite them.
          (set! html-table (gnc:html-table-add-account-balances
                            #f acct-table params))

          ;; ... then the budget values
          (gnc:html-table-add-budget-values!
           html-table acct-table budget paramsBudget)

          ;; hmmm... I expected that add-budget-values would have to
          ;; clear out any unused columns to the right, out to the
          ;; table width, since the add-account-balance had put stuff
          ;; there, but it doesn't seem to matter.

          (gnc:html-document-add-object! doc html-table))))
      ) ;; end cond

    (gnc:report-finished)
    doc))

(gnc:define-report
 'version 1
 'name reportname
 'report-guid "810ed4b25ef0486ea43bbd3dddb32b11"
 'menu-path (list gnc:menuname-budget)
 'options-generator budget-report-options-generator
 'renderer budget-renderer)

