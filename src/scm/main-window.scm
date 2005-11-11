;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; main-window.scm : utilities for dealing with main window
;; Copyright 2001 Bill Gribble <grib@gnumatic.com>
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
;; 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
;; Boston, MA  02111-1307,  USA       gnu@gnu.org
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; account tree options 
;; like reports, we have an integer tree id that is the index into a
;; global hash table, and URLs of the form gnc-acct-tree:id=%d will
;; open to the right window.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define gnc:*acct-tree-options* (make-hash-table 11))
(define gnc:*acct-tree-id* 0)

(define (gnc:find-acct-tree-window-options id)
  (hash-ref gnc:*acct-tree-options* id))

(define (gnc:make-acct-tree-window-options) 
  (let* ((options (gnc:new-options))
         (add-option 
          (lambda (opt)
            (gnc:register-option options opt))))

    (add-option
     (gnc:make-simple-boolean-option
      (N_ "Account Tree") (N_ "Double click expands parent accounts")
      "a" (N_ "Double clicking on an account with children expands \
the account instead of opening a register.") #f))

    (add-option
     (gnc:make-list-option
      (N_ "Account Tree") (N_ "Account types to display")
      "b" ""
      (list 'bank 'cash 'credit 'asset 'liability 'stock
            'mutual 'currency 'income 'expense 'equity 'payable 'receivable)
      (list (list->vector (list 'bank      (N_ "Bank") ""))
            (list->vector (list 'cash      (N_ "Cash") ""))
            (list->vector (list 'credit    (N_ "Credit") ""))
            (list->vector (list 'asset     (N_ "Asset") ""))
            (list->vector (list 'liability (N_ "Liability") ""))
            (list->vector (list 'stock     (N_ "Stock") ""))
            (list->vector (list 'mutual    (N_ "Mutual Fund") ""))
            (list->vector (list 'currency  (N_ "Currency") ""))
            (list->vector (list 'income    (N_ "Income") ""))
            (list->vector (list 'expense   (N_ "Expense") ""))
            (list->vector (list 'equity    (N_ "Equity") ""))
	    (list->vector (list 'payable   (N_ "Accounts Payable") ""))
	    (list->vector (list 'receivable (N_ "Accounts Receivable") "")))))

    options))

(define (gnc:make-new-acct-tree-window)  
  (let ((options (gnc:make-acct-tree-window-options))
        (id gnc:*acct-tree-id*))
    (hash-set! gnc:*acct-tree-options* id options)
    (set! gnc:*acct-tree-id* (+ 1 id))
    (cons options id)))

(define (gnc:free-acct-tree-window id) 
  (hash-remove! gnc:*acct-tree-options* id))


(define (gnc:acct-tree-generate-restore-forms optobj id)
  (string-append
   ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
   (simple-format #f ";; options for account tree id=~S\n" id)
   "(let ((options (gnc:make-acct-tree-window-options)))\n"
   (gnc:generate-restore-forms optobj "options")
   (simple-format
    #f "  (hash-set! gnc:*acct-tree-options* ~A options)\n" id)
   "  \""
   (gnc:html-build-url gnc:url-type-accttree (sprintf #f "%a" id) #f)
   "\")\n\n"))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; book open and close hooks for mdi 
;; 
;; we need to save all the active report and acct tree info during
;; book close.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define (gnc:main-window-save-state session)
  (let* ((book-url (gnc:session-get-url session))
	 (conf-file-name (gnc:html-encode-string book-url))
	 (dotgnucash-dir (build-path (getenv "HOME") ".gnucash"))
         (file-dir (build-path dotgnucash-dir "books"))
         (save-file? #f)
         (book-path #f))

    ;; make sure ~/.gnucash/books is there
    (set! save-file? (and (gnc:make-dir dotgnucash-dir)
                         (gnc:make-dir file-dir)))

    (if (not save-file?) (gnc:warn (_ "Can't save window state")))

    (if (and save-file? conf-file-name)
        (let ((book-path (build-path (getenv "HOME") ".gnucash" "books" 
                                     conf-file-name)))
          (with-output-to-port (open-output-file book-path)
            (lambda ()
              (hash-fold 
               (lambda (k v p)
                 (if (gnc:report-needs-save? v)
                     (display (gnc:report-generate-restore-forms v))))
               #t *gnc:_reports_*)

              (hash-fold 
               (lambda (k v p)
                 (display (gnc:acct-tree-generate-restore-forms v k)) #t)
               #t gnc:*acct-tree-options*)

              (force-output)))
	  ))))

(define (gnc:main-window-book-close-handler session)
  (let ((dead-reports '()))
    ;; get a list of the reports we'll be needing to nuke     
    (hash-fold 
     (lambda (k v p)
       (set! dead-reports (cons k dead-reports)) #t) #t *gnc:_reports_*)

    ;; actually remove them (if they're being displayed, the
    ;; window's reference will keep the report alive until the
    ;; window is destroyed, but find-report will fail)
    (for-each 
     (lambda (dr)
       (hash-remove! *gnc:_reports_* dr))
     dead-reports)))

(define (gnc:main-window-book-open-handler session)
  (define (try-load file-suffix)
    (let ((file (build-path (getenv "HOME") ".gnucash" "books" file-suffix)))
      ;; make sure the books directory is there 
      (if (access? file F_OK)
          (if (not (false-if-exception (primitive-load file)))
              (begin
                (gnc:warn "failure loading " file)
                #f))
          #f)))
  (let* ((book-url (gnc:session-get-url session))
	 (conf-file-name (gnc:html-encode-string book-url))
	 (dead-reports '()))
    (if conf-file-name 
        (try-load conf-file-name))

    ;; the reports have only been created at this point; create their ui component.
    (hash-fold (lambda (key val prior-result)
                (gnc:main-window-open-report (gnc:report-id val)
                                             #f ;; =window: #f/null => open in first window
                                             ))
               #t *gnc:_reports_*)
    ))

(define (gnc:main-window-properties-cb)
  (let* ((book (gnc:get-current-book))
	 (slots (gnc:book-get-slots book)))

    (define (changed_cb)
      (gnc:book-kvp-changed book))
			    
    (gnc:kvp-option-dialog gnc:id-book
			   slots (_ "Book Options")
			   changed_cb)))

(gnc:hook-remove-dangler gnc:*book-closed-hook* 
                         gnc:main-window-book-close-handler)
(gnc:hook-add-dangler gnc:*book-closed-hook* 
                      gnc:main-window-book-close-handler)
