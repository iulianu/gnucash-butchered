;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;  qif-guess-map.scm
;;;  guess (or load from prefs) mappings from QIF cats/accts to gnc
;;;
;;;  Bill Gribble <grib@billgribble.com> 20 Feb 2000 
;;;  $Id$
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(gnc:support "qif-import/qif-guess-map.scm")

(define GNC-BANK-TYPE 0)
(define GNC-CASH-TYPE 1)
(define GNC-ASSET-TYPE 2)
(define GNC-LIABILITY-TYPE 4)
(define GNC-CCARD-TYPE 3)
(define GNC-STOCK-TYPE 5)
(define GNC-MUTUAL-TYPE 6)
(define GNC-INCOME-TYPE 8)
(define GNC-EXPENSE-TYPE 9)
(define GNC-EQUITY-TYPE 10)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  qif-import:load-map-prefs
;;  load the saved mappings file, and make a table of all the 
;;  accounts with their full names and pointers for later 
;;  guessing of a mapping.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:load-map-prefs)
  (define (extract-all-account-info agroup root-name)
    (if (not agroup)
        '()
        (let ((children-list (gnc:group-get-account-list agroup))
              (names '()))
          
          ;; now descend the tree of child accounts.
          (for-each 
           (lambda (child-acct)
             (let* ((name (gnc:account-get-name child-acct))
                    (fullname 
                     (if (string? root-name)
                         (string-append root-name 
                                        (gnc:account-separator-char)
                                        name)
                         name)))
               (set! names 
                     (append (cons (list name fullname child-acct)
                                   (extract-all-account-info 
                                    (gnc:account-get-children child-acct)
                                    fullname))
                             names))))
           children-list)
          names)))
  
  (define (safe-read)
    (false-if-exception 
     (read)))

  ;; we'll be returning a list of 4 elements:  
  ;;  - a list of all the known gnucash accounts in 
  ;;    (shortname fullname account) format.
  ;;  - a hash of QIF account name to gnucash account info
  ;;  - a hash of QIF category to gnucash account info
  ;;  - a hash of QIF memo/payee to gnucash account info   <-- not yet
  ;;    (older saved prefs may not have this one)
  (let* ((pref-dir (build-path (getenv "HOME") ".gnucash"))
         (pref-filename (build-path pref-dir "qif-accounts-map"))
         (results '()))
    
    ;; first, read the account map and category map from the 
    ;; user's qif-accounts-map file.     
    (if (and (access? pref-dir F_OK)
             (access? pref-dir X_OK) 
             (access? pref-filename R_OK))
        (with-input-from-file pref-filename
          (lambda ()
            (let ((qif-account-list #f)
                  (qif-cat-list #f)
                  (qif-memo-list #f)
                  (qif-account-hash #f)
                  (qif-cat-hash #f)
                  (qif-memo-hash #f))
              (set! qif-account-list (safe-read))
              (if (not (list? qif-account-list))
                  (set! qif-account-hash (make-hash-table 20))
                  (set! qif-account-hash 
                        (qif-import:read-map qif-account-list)))
              
              (set! qif-cat-list (safe-read))
              (if (not (list? qif-cat-list))
                  (set! qif-cat-hash (make-hash-table 20))
                  (set! qif-cat-hash (qif-import:read-map qif-cat-list)))

;;;              (set! qif-memo-list (safe-read))
;;;              (if (not (list? qif-cat-list))
;;;                  (set! qif-memo-hash (make-hash-table 20))
;;;                  (set! qif-memo-hash (qif-import:read-map qif-memo-list)))
                  
;;;              (set! results 
;;;                    (list qif-account-hash qif-cat-hash qif-memo-hash)))))
              (set! results 
                    (list qif-account-hash qif-cat-hash)))))
        (begin 
          (set! results (list (make-hash-table 20)
;;;                              (make-hash-table 20)
                              (make-hash-table 20)))))
    
    ;; now build the list of all known account names 
    (let* ((all-accounts (gnc:get-current-group))
           (all-account-info (extract-all-account-info all-accounts #f)))
      (set! results (cons all-account-info results)))
    results))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  dump the mapping hash tables to a file.  The hash tables are 
;;  updated when the user clicks the big "OK" button on the dialog,
;;  so your selections get lost if you do Cancel.
;;  we initialize the number of transactions to 0 here so 
;;  bogus accounts don't get created if you have funny stuff
;;  in your map.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:write-map hashtab)
  (let ((table '()))
    (for-each 
     (lambda (bin)
       (for-each 
        (lambda (entry)
          (let ((key (car entry))
                (value (cdr entry)))
            (set! table (cons (cons key (simple-obj-to-list value)) table))))
        bin))
     (vector->list hashtab))
    (write table)))

(define (qif-import:read-map tablist)
  (let ((table (make-hash-table 20)))
    (for-each 
     (lambda (entry)
       (let ((key (car entry))
             (value (simple-obj-from-list (cdr entry) <qif-map-entry>)))
         (qif-map-entry:set-display?! value #f)
         (hash-set! table key value)))
     tablist)
    table))

(define (qif-import:save-map-prefs acct-map cat-map) ;; memo-map)
  (let* ((pref-dir (build-path (getenv "HOME") ".gnucash"))
         (pref-filename (build-path pref-dir "qif-accounts-map"))
         (save-ok #f))
    ;; does the file exist? if not, create it; in either case,
    ;; make sure it's a directory and we have write and execute 
    ;; permission. 
    (let ((perm (access? pref-dir F_OK)))
      (if (not perm)
          (mkdir pref-dir))
      (let ((stats (stat pref-dir)))
        (if (and (eq? (stat:type stats) 'directory)
                 (access? pref-dir X_OK)
                 (access? pref-dir W_OK))
            (set! save-ok #t))))        
    
    (if save-ok 
        (with-output-to-file pref-filename
          (lambda ()
            (display ";;; qif-accounts-map\n")
            (display ";;; automatically generated by GNUcash.  DO NOT EDIT\n") 
            (display ";;; (unless you really, really want to).\n") 
            
            (display ";;; map from QIF accounts to GNC accounts") (newline) 
            (qif-import:write-map acct-map)
            
            (display ";;; map from QIF categories to GNC accounts") (newline)
            (qif-import:write-map cat-map)
            
;;;         (display ";;; map from QIF payee/memo to GNC accounts") (newline)
;;;         (qif-import:write-map memo-map)
            (newline))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  here's where we do all the guessing.  We really want to find the
;;  match in the hash table, but failing that we guess intelligently
;;  and then (failing that) not so intelligently. called in the 
;;  dialog routines to rebuild the category and account map pages.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  guess-acct
;;  find an existing gnc acct of the right type and name, or 
;;  specify a type and name for a new one. 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:guess-acct acct-name allowed-types gnc-acct-info)
  ;; see if there's a saved mapping in the hash table or an 
  ;; existing gnucash account with a name that could reasonably
  ;; be said to be the same name (i.e. ABC Bank == abc bank)
  (let* ((mapped-gnc-acct            
          (qif-import:find-similar-acct acct-name allowed-types 
                                        gnc-acct-info))
         (retval (make-qif-map-entry)))
    
    ;; set fields needed for any return value
    (qif-map-entry:set-qif-name! retval acct-name)
    (qif-map-entry:set-allowed-types! retval allowed-types)
    
    (if mapped-gnc-acct
        ;; ok, we've found an existing account that 
        ;; seems to work OK name-wise. 
        (begin           
          (qif-map-entry:set-gnc-name! retval (car mapped-gnc-acct))
          (qif-map-entry:set-allowed-types! retval 
                                            (list (cadr mapped-gnc-acct)))
          (qif-map-entry:set-new-acct?! retval #f))
        ;; we haven't found a match, so by default just create a new
        ;; one.  Try to put the new account in a similar place in
        ;; the hierarchy if there is one. 
        (let ((new-acct-info 
               (qif-import:find-new-acct acct-name allowed-types 
                                         gnc-acct-info)))
          (qif-map-entry:set-gnc-name! retval (car new-acct-info))
          (qif-map-entry:set-allowed-types! retval (list (cadr new-acct-info)))
          (qif-map-entry:set-new-acct?! retval #t)))
    retval))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  qif-import:find-similar-acct
;;  guess a translation from QIF info
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:find-similar-acct qif-acct-name allowed-types 
                                      gnc-acct-info)
  (let* ((same-type-accts '())       
         (matching-name-accts '())
         (retval #f))
    (for-each 
     (lambda (gnc-acct)
       ;; check against allowed-types 
       (let ((acct-matches? #f))
         (for-each
          (lambda (type)
            (if (eq? type (gnc:account-get-type (caddr gnc-acct)))
                (set! acct-matches? #t)))
          allowed-types)
         (if acct-matches? 
             (set! same-type-accts (cons gnc-acct same-type-accts)))))
     gnc-acct-info)
    
    ;; now find one in the same-type-list with a similar name. 
    (for-each 
     (lambda (gnc-acct)
       (if (qif-import:possibly-matching-name? 
            qif-acct-name gnc-acct)
           (set! matching-name-accts 
                 (cons gnc-acct matching-name-accts))))
     same-type-accts)
    
    ;; now we have either nothing, something, or too much :) 
    ;; return the full-name of the first name-matching account 
    (if (not (null? matching-name-accts))
        (set! retval (list 
                      (cadr (car matching-name-accts))
                      (gnc:account-get-type 
                       (caddr (car matching-name-accts)))))
        #f)
    retval))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  qif-import:possibly-matching-name? qif-acct gnc-acct
;;  try various normalizations and permutations of the names 
;;  to see if they could be the same. 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:possibly-matching-name? qif-acct-name gnc-acct)
  (or 
   ;; the QIF acct is the same name as the short name of the 
   ;; gnc acct [ignoring case] (likely)
   (string=? (string-downcase  qif-acct-name)
             (string-downcase (car gnc-acct)))
   
   ;; the QIF acct is the same name as the long name of the 
   ;; gnc acct [ignoring case] (not so likely)
   (string=? (string-downcase qif-acct-name)
             (string-downcase (cadr gnc-acct)))
   
   ;; the QIF name is a substring of the gnc full name.  
   ;; this happens if you have the same tree but a different 
   ;; top-level structure. (i.e. expenses:tax vs. QIF tax)
   (string-match (string-downcase qif-acct-name) 
                 (string-downcase (cadr gnc-acct)))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  qif-import:find-new-acct
;;  Come up with a logical name for a new account based on 
;;  the Quicken name and type of the account  
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (qif-import:find-new-acct qif-acct allowed-types gnc-acct-info)
  (cond ((and (string? qif-acct)
              (string=? qif-acct (default-equity-account)))
         (let ((existing-equity 
                (qif-import:find-similar-acct (default-equity-account)
                                              (list GNC-EQUITY-TYPE)
                                              gnc-acct-info)))
           (if existing-equity 
               (cdr existing-equity)
               (list (default-equity-account) GNC-EQUITY-TYPE))))
        ((and (string? qif-acct)
              (not (string=? qif-acct "")))
         (list qif-acct (car allowed-types)))
        (#t 
         (list "Unspecified" (car allowed-types)))))

