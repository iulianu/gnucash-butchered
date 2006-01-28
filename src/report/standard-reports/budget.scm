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
(require 'sort)

(gnc:module-load "gnucash/report/report-system" 0)
(gnc:module-load "gnucash/gnome-utils" 0) ;for gnc:html-build-url

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

(define optname-budget (N_ "Budget"))

;; options generator
(define (budget-report-options-generator)
  (let ((options (gnc:new-options)))

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
     options gnc:pagename-general optname-price-source "c" 'weighted-average)

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
        '(asset liability income expense)
        (gnc:group-get-subaccounts (gnc:get-current-group))))
     #f)

    ;; Set the general page as default option tab
    (gnc:options-set-default-section options gnc:pagename-general)

    options)
  )

(define (gnc:html-table-add-budget-values!
         html-table acct-table budget params)

  (define (gnc:html-table-add-budget-line!
           html-table rownum colnum
           budget acct exchange-fn)
    (let* ((num-periods (gnc:budget-get-num-periods budget))
           (period 0)
           )
      (while (< period num-periods)
             (let* ((bgt-col (+ (* period 2) colnum 1))
                    (act-col (+ 1 bgt-col))

                    (comm (gnc:account-get-commodity acct))
                    (numeric-val (gnc:budget-get-account-period-value
                                  budget acct period))

                    (bgt-val (gnc:make-gnc-monetary
                              comm numeric-val))
                    (numeric-val (gnc:budget-get-account-period-actual-value
                                  budget acct period))
                    (act-val (gnc:make-gnc-monetary
                              comm numeric-val))
                    (reverse-balance? (gnc:account-reverse-balance? acct))
                    )

               (cond (reverse-balance? (set! act-val
                                       (gnc:monetary-neg act-val))))


               (gnc:html-table-set-cell!
                html-table
                rownum bgt-col bgt-val)

               (gnc:html-table-set-cell!
                html-table
                rownum act-col act-val)

               (set! period (+ period 1))
               )
             )
      )
    )
  (define (gnc:html-table-add-budget-headers!
           html-table colnum budget)
    (let* ((num-periods (gnc:budget-get-num-periods budget))
           (period 0)
           )

      ;; prepend 2 empty rows
      (gnc:html-table-prepend-row! html-table '())
      (gnc:html-table-prepend-row! html-table '())

      ;; make the column headers
      (while (< period num-periods)
             (let* ((bgt-col (+ (* period 2) colnum 1))
                    (act-col (+ 1 bgt-col))
                    (date (gnc:budget-get-period-start-date budget period))
                    )
               (gnc:html-table-set-cell!
                html-table 0 bgt-col (gnc:print-date date))

               (gnc:html-table-set-cell!
                html-table
                1 bgt-col "Bgt")

               (gnc:html-table-set-cell!
                html-table
                1 act-col "Act")

               (set! period (+ period 1))
               )
             )
      )
    )

  (let* ((num-rows (gnc:html-acct-table-num-rows acct-table))
	 (rownum 0)
         (numcolumns (gnc:html-table-num-columns html-table))
	 ;;(html-table (or html-table (gnc:make-html-table)))
	 (get-val (lambda (alist key)
		    (let ((lst (assoc-ref alist key)))
		      (if lst (car lst) lst))))
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
              budget acct exchange-fn)
             (set! rownum (+ rownum 1)) ;; increment rownum
             )
           ) ;; end of while

    ;; column headers
    (gnc:html-table-add-budget-headers! html-table colnum budget)

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
         (display-depth (get-option gnc:pagename-accounts
                                    optname-display-depth))
         (show-subaccts? (get-option gnc:pagename-accounts
                                     optname-show-subaccounts))
         (accounts (get-option gnc:pagename-accounts
                               optname-accounts))
         (row-num 0) ;; ???
	 (work-done 0)
	 (work-to-do 0)
         ;;(report-currency (get-option gnc:pagename-general
         ;;                             optname-report-currency))
         (show-full-names? (get-option gnc:pagename-general
                                       optname-show-full-names))
         (separator (gnc:account-separator-char))

         (doc (gnc:make-html-document))
         ;;(table (gnc:make-html-table))
         ;;(txt (gnc:make-html-text))
         )

    ;; is account in list of accounts?
    (define (same-account? a1 a2)
      (string=? (gnc:account-get-guid a1) (gnc:account-get-guid a2)))

    (define (same-split? s1 s2)
      (string=? (gnc:split-get-guid s1) (gnc:split-get-guid s2)))

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
      (string<? (gnc:account-get-full-name a) (gnc:account-get-full-name b)))

    ;; helper for account depth
    (define (account-get-depth account)
      (define (account-get-depth-internal account-internal depth)
        (let ((parent (gnc:account-get-parent-account account-internal)))
          (if parent
            (account-get-depth-internal parent (+ depth 1))
            depth)))
      (account-get-depth-internal account 1))

    (define (accounts-get-children-depth accounts)
      (apply max
	     (map (lambda (acct)
		    (let ((children
			   (gnc:account-get-immediate-subaccounts acct)))
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

    (if (not (or (null? accounts) (null? budget) (not budget)))

        (let* ((tree-depth (if (equal? display-depth 'all)
                               (accounts-get-children-depth accounts)
                               display-depth))
               ;;(account-disp-list '())

               ;; Things seem to crash if I don't set 'end-date to
               ;; _something_ but the actual value isn't used.
               (env (list (list 'end-date (gnc:get-today))
                          (list 'display-tree-depth tree-depth)
                          (list 'depth-limit-behavior 'flatten)
                          ))
               (acct-table #f)
               (html-table (gnc:make-html-table))
               (params '())
               (report-name (get-option gnc:pagename-general
                                        gnc:optname-reportname))
               )

          (gnc:html-document-set-title!
           doc (sprintf #f (_ "%s - %s")
                        report-name (gnc:budget-get-name budget)))

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
           html-table acct-table budget params)

          ;; hmmm... I expected that add-budget-values would have to
          ;; clear out any unused columns to the right, out to the
          ;; table width, since the add-account-balance had put stuff
          ;; there, but it doesn't seem to matter.

          (gnc:html-document-add-object! doc html-table)
          )

        ;; error condition: either no accounts or no budgets specified
        (gnc:html-document-add-object!
         doc
         (gnc:html-make-generic-options-warning
	  reportname (gnc:report-id report-obj))))

    (gnc:report-finished)
    doc))

(gnc:define-report
 'version 1
 'name reportname
 'menu-path (list gnc:menuname-income-expense)
 'options-generator budget-report-options-generator
 'renderer budget-renderer)

