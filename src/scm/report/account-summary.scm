;; -*-scheme-*-
;; account-summary.scm
;; account(s) summary report
;;
;; Author makes no implicit or explicit guarantee of accuracy of
;;  these calculations and accepts no responsibility for direct
;;  or indirect losses incurred as a result of using this software.
;;
;; Terry D. Boldt (tboldt@attglobal.net>
;; created by modifying other report files extensively - the authors of
;; the modified report files are graciously thanked for their efforts.

(gnc:support "report/account-balance.scm")
(gnc:depend "report-utilities")
(gnc:depend "html-generator.scm")
(gnc:depend "date-utilities.scm")

(let ()

  ;; Options
  (define (accsum-options-generator)
    (let*
        ((gnc:*accsum-track-options* (gnc:new-options))
         ;; register a configuration option for the report
         (gnc:register-accsum-option
          (lambda (new-option)
            (gnc:register-option gnc:*accsum-track-options*
                                 new-option))))

      ;; to-date
      (gnc:register-accsum-option
       (gnc:make-date-option
        "Report Options" "To"
        "a" "Report up to and including this date"
        (lambda ()
          (let ((bdtime (localtime (current-time))))
            (set-tm:sec bdtime 59)
            (set-tm:min bdtime 59)
            (set-tm:hour bdtime 23)
            (cons 'absolute (cons (car (mktime bdtime)) 0))))
        #f 'absolute #f))

      ;; account(s) to do report on
      (gnc:register-accsum-option
       (gnc:make-account-list-option
        "Report Options" "Account"
        "b" "Report on these account(s)"
        (lambda ()
          (let ((current-accounts (gnc:get-current-accounts))
                (num-accounts
                 (gnc:group-get-num-accounts (gnc:get-current-group))))

            (cond ((not (null? current-accounts)) current-accounts)
                  (else
                   (let ((acctlist '()))
                     (gnc:for-loop
                      (lambda(x)
                        (set! acctlist
                              (append!
                               acctlist
                               (list (gnc:group-get-account
                                      (gnc:get-current-group) x)))))
                      0 num-accounts 1)
                     acctlist)))))	
        #f #t))

      (gnc:register-accsum-option
       (gnc:make-simple-boolean-option
        "Report Options" "Sub-Accounts"
        "c" "Include Sub-Accounts of each selected Account" #f))

      gnc:*accsum-track-options*))

;; I copied the following html generation code from the html-generation file
;; because I like the numbers in the balance column aligned at the top of the
;; cell - this aligns the number with the account name - rather than placing
;; the balance number in the default position of the vertical center of cell
;; which makes it difficult to match the balance with the account name.

;; Create a column entry
  (define (accsum_html-table-col val)
    (string-append "<TD valign=top align=right>" (tostring val) "</TD>"))

  (define (accsum_html-table-col-align val align)
    (string-append "<TD align=" align ">" (tostring val) "</TD>"))

;; Create an html table row from a list of entries
  (define (accsum_html-table-row lst)
    (cond ((string? lst) lst)
	  (else
	   (string-append
	    "<TR>"
    	(apply string-append (map accsum_html-table-col lst))
	    "</TR>"))))

; Create an html table row from a list of entries
  (define (accsum_html-table-row-align lst align-list)
    (cond ((string? lst) lst)
	  (else
	   (string-append
    	"<TR>"
	    (apply string-append (map accsum_html-table-col-align lst align-list))
	    "</TR>"))))

;; Create an html table from a list of rows, each containing
;;   a list of column entries
  (define (accsum_html-table hdrlst llst)
    (string-append
     (accsum_html-table-header hdrlst)
     (apply string-append (map accsum_html-table-row llst))
     (accsum_html-table-footer)))

  (define (accsum_html-table-headcol val)
    (string-append "<TH justify=center>" (tostring val) "</TH>"))

  (define (accsum_html-table-header vec)
     (apply string-append "<TABLE cellspacing=10 rules=\"rows\">\n"
            (map accsum_html-table-headcol vec)))

  (define (accsum_html-table-footer)
    "</TABLE>")

  ;; the following should be defined in the report-utilities file (Dave put it there I believe).
  ;; Just in case I am including it here if your copy of report-utilities doesn't have it
  ;; un-comment to use
;;  (define (gnc:group-get-accounts group)
;;    (gnc:group-map-accounts (lambda (a) a) group))

  (define string-db (gnc:make-string-database))

  (define (column-list)
    (map (lambda (key) (string-db 'lookup key))
         (list 'account-name 'balance )))

  (define (non-zero-at-date-accounts accts date)
    (if (null? accts)
        '()
        (let ((acct (car accts))
              (rest (non-zero-at-date-accounts (cdr accts) date)))
          (if (< (gnc:account-get-balance-at-date acct date #t) 0.01)
              rest
              (cons acct rest)))))

  ;; build the table rows for a single account
  ;; date specifies the ending date for the account
  ;; do-children specifies whether to expand the children
  ;; in the table
  ;; each row consists of two columns: account-name account-balance
  ;; the children are a separate table enclosed in the account-name cell
  ;; do not include accounts which have a zero balance
  (define (acc-sum-table-row account date do-children?)
    (let
      ((acc-bal (gnc:account-get-balance-at-date account date #t))
       (children (gnc:account-get-children account)))
      (list
       (if (and do-children? (> (gnc:group-get-num-accounts children) 0))
           (string-append (gnc:account-get-name account)
                          (acc-sum-table
                           (non-zero-at-date-accounts
                            (gnc:group-get-accounts children) date) date #t))
           (gnc:account-get-name account))
       (gnc:amount->string acc-bal
                           (gnc:account-value-print-info account #t)))))

  ;; build the table for the list of 'accounts' passed
  (define (acc-sum-table accnts date do-children?)
    (let ((columns (column-list)))
      (if (null? accnts)
          ""
          (accsum_html-table columns
                      (map (lambda (acct)
                             (acc-sum-table-row acct date do-children?))
                           (non-zero-at-date-accounts accnts date))))))

;; get the total of a list of accounts at the specified date.
;; all children are included in the calculation
  (define (account-total-at-date accnts date)
    (apply +
         (map (lambda (account) (gnc:account-get-balance-at-date account date #t)) accnts))
  )


  (define (accsum-renderer options)
      (let ((acctcurrency "USD")
            (acctname "")
            (enddate (gnc:date-option-absolute-time (gnc:option-value
                      (gnc:lookup-option options "Report Options" "To"))))
            (accounts (gnc:option-value
                       (gnc:lookup-option options "Report Options" "Account")))
            (dosubs (gnc:option-value
                     (gnc:lookup-option options
                                        "Report Options" "Sub-Accounts")))
            (prefix (list "<HTML>" "<head>" "<title>"
                          "Account Summary" "</title>" "</head>" "<BODY>"))
            (suffix  (list "</BODY>" "</HTML>"))
            (rept-data '())
            (rept-text "")
            (rept-total ())
            (slist '()))

        (if (null? accounts)
            (set! rept-text
                  (list "<TR><TD>"
                        (string-db 'lookup 'no-account)
                        "</TD></TR>"))
            (begin

              (set! rept-total
                    (gnc:amount->string
                     (account-total-at-date accounts enddate)
                     (gnc:account-value-print-info (car accounts) #t)))

              ; Grab account names
              (set! acctname
                    (string-join (map gnc:account-get-name accounts) " , "))

              ;; Create HTML
              (set! rept-data (acc-sum-table accounts enddate dosubs))))

        (list prefix
              (if (null? accounts)
                  rept-text
                  (list (sprintf #f
                                 (string-db 'lookup
                                            (if dosubs
                                                'report-for-and
                                                'report-for))
                                 (gnc:print-date enddate)
                                 acctname
                                 rept-total)
                        "<p>\n"))
              rept-data
;;              rept-total
              suffix)))

  ;; Define the strings
  (string-db 'store 'account-name   "Account Name")
  (string-db 'store 'balance        "Balance")
  (string-db 'store 'no-account     "You have not selected an account.")
  (string-db 'store 'report-for     "Date: %s<br>Report for %s.<br>Accounts Total: %s")
  (string-db 'store 'report-for-and "Date: %s<br>Report for %s and all Sub-Accounts.<br>Accounts Total: %s")

  (gnc:define-report
   ;; version
   'version 1
   ;; Name
   'name "Account Summary"
   ;; Options
   'options-generator accsum-options-generator
   ;; renderer
   'renderer accsum-renderer))
