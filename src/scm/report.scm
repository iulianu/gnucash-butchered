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

(require 'hash-table)

(require 'record)
(gnc:support "report.scm")

;; We use a hash to store the report info so that whenever a report
;; is requested, we'll look up the action to take dynamically. That
;; makes it easier for us to allow changing the report definitions
;; on the fly later, and it should have no appreciable performance
;; effect.

(define *gnc:_report-info_* (make-hash-table 23))
;; This hash should contain all the reports available and will be used
;; to generate the reports menu whenever a new window opens and to
;; figure out what to do when a report needs to be generated.
;;
;; The key is the string naming the report and the value is the report
;; structure.

(define (gnc:run-report report-name options)
  ;; Return a string consisting of the contents of the report.

  (define (display-report-list-item item port)
    (cond
     ((string? item) (display item port))
     ((null? item) #t)
     ((list? item) (map (lambda (item) (display-report-list-item item port))
                        item))
     (else (gnc:warn "gnc:run-report - " item " is the wrong type."))))

;; Old version assumed flat lists
;   (define (report-output->string lines)
;     (call-with-output-string
;      (lambda (port)
;        (for-each
;         (lambda (item) (display-report-list-item item port))
;         lines))))

;; New version that processes a _tree_ rather than a flat list of
;; strings.  This means that we can pass in somewhat "more structured"
;; data.

  (define (output-tree-to-port tree port)
    (cond 
     ((pair? tree)
      (output-tree-to-port (car tree) port) 
      (output-tree-to-port (cdr tree) port))
     ((string? tree)
      (display-report-list-item tree port)
      (newline port))
     ((null? tree)
      #f) ;;; Do Nothing...
     (tree  ;;; If it's not #f
      (display-report-list-item "<B> Error - Bad atom! </b>" port)
      (display-report-list-item tree port)
      (display "Err: (")
      (write tree)
      (display ")")
      (newline)
      (newline port))))

    (define (report-output->string tree)
      (call-with-output-string
       (lambda (port)
	 (output-tree-to-port tree port))))
  
    (let ((report (hash-ref *gnc:_report-info_* report-name)))
      (if report
	  (let* ((renderer (gnc:report-renderer report))
		 (lines    (renderer options))
		 (output   (report-output->string lines)))
	    output)
	  #f)))

(define (gnc:report-menu-setup win)
  (define menu (gnc:make-menu "_Reports" (list "_Accounts")))
  (define menu-namer (gnc:new-menu-namer))

  (define (add-report-menu-item name report)
    (let* ((report-string "Report")
           (title (string-append (gnc:_ report-string) ": " (gnc:_ name)))
           (item #f))

      (if (gnc:debugging?)
          (let ((options (false-if-exception (gnc:report-new-options report))))
            (if options
                (gnc:options-register-translatable-strings options))
            (gnc:register-translatable-strings report-string name)))

      (set! item
            (gnc:make-menu-item
             ((menu-namer 'add-name) name)
             (string-append "Display the " name " report.")
             (list "_Reports" "")
             (lambda ()
               (let ((options (false-if-exception
                               (gnc:report-new-options report))))
                 (gnc:report-window title
                                    (lambda () (gnc:run-report name options))
                                    options)))))
      (gnc:add-extension item)))

  (gnc:add-extension menu)

  (hash-for-each add-report-menu-item *gnc:_report-info_*))

(define report-record-structure
  (make-record-type "report-record-structure"
                    ; The data items in a report record
                    '(version name options-generator renderer)))

(define (gnc:define-report . args) 
  ;; For now the version is ignored, but in the future it'll let us
  ;; change behaviors without breaking older reports.
  ;;
  ;; The renderer should be a function that accepts one argument,
  ;; a set of options, and generates the report.
  ;;
  ;; This code must return as its final value a collection of strings in
  ;; the form of a list of elements where each element (recursively) is
  ;; either a string, or a list containing nothing more than strings and
  ;; lists of strings.  Any null lists will be ignored.  The final html
  ;; output will be produced by an in-order traversal of the tree
  ;; represented by the list.  i.e. ("a" (("b" "c") "d") "e") produces
  ;; "abcde" in the output.
  ;;
  ;; For those who speak BNF-ish the output should look like
  ;;
  ;; report -> string-list
  ;; string-list -> ( items ) | ()
  ;; items -> item items | item
  ;; item -> string | string-list
  ;; 
  ;; Valid examples:
  ;;
  ;; ("<html>" "</html>")
  ;; ("<html>" " some text " "</html>")
  ;; ("<html>" ("some" ("other" " text")) "</html>")

  (define (blank-report)
    ;; Number of #f's == Number of data members
    ((record-constructor report-record-structure) #f #f #f #f))

  (define (args-to-defn in-report-rec args)
    (let ((report-rec (if in-report-rec
                          in-report-rec
                          (blank-report))))
     (if (null? args)
         in-report-rec
         (let ((id (car args))
               (value (cadr args))
               (remainder (cddr args)))
           ((record-modifier report-record-structure id) report-rec value)
           (args-to-defn report-rec remainder)))))

  (let ((report-rec (args-to-defn #f args)))
    (if (and report-rec
             (gnc:report-name report-rec))
        (hash-set! *gnc:_report-info_*
                   (gnc:report-name report-rec) report-rec)
        (gnc:warn "gnc:define-report: bad report"))))

(define gnc:report-version
  (record-accessor report-record-structure 'version))
(define gnc:report-name
  (record-accessor report-record-structure 'name))
(define gnc:report-options-generator
  (record-accessor report-record-structure 'options-generator))
(define gnc:report-renderer
  (record-accessor report-record-structure 'renderer))

(define (gnc:report-new-options report)
  (let ((generator (gnc:report-options-generator report)))
    (if (procedure? generator)
        (generator)
        #f)))

(gnc:hook-add-dangler gnc:*main-window-opened-hook* gnc:report-menu-setup)
