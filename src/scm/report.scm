;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; report.scm : structures/utilities for representing reports 
;; Copyright 2000 Bill Gribble <grib@gnumatic.com>
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

(require 'hash-table)
(require 'record)

(gnc:support "report.scm")

;; This hash should contain all the reports available and will be used
;; to generate the reports menu whenever a new window opens and to
;; figure out what to do when a report needs to be generated.
;;
;; The key is the string naming the report and the value is the 
;; report definition structure.
(define *gnc:_report-templates_* (make-hash-table 23))

;; this is a hash of 'report ID' to instantiated report.  the 
;; report id is generated at report-record creation time. 
(define *gnc:_reports_* (make-hash-table 23))
(define *gnc:_report-next-serial_* 0)

(define (gnc:report-menu-setup win)
  (define menu (gnc:make-menu "_Reports" (list "_Accounts")))
  (define menu-namer (gnc:new-menu-namer))

  (define (add-report-menu-item name report)
    (if (gnc:report-in-menu? report)
        (let* ((title (string-append (_ "Report") ": " (_ name)))
               (item #f))

          (set! item
                (gnc:make-menu-item
                 ((menu-namer 'add-name) name)
                 (sprintf #f (_ "Display the %s report") name)
                 (list "_Reports" "")
                 (lambda ()
                   (gnc:backtrace-if-exception 
                    (lambda ()
                      (let ((rept (gnc:make-report
                                   (gnc:report-template-name report))))
                        (gnc:report-in-main-window rept)))))))
          (gnc:add-extension item))))
  
  ;; add the menu option to edit style sheets 
  (gnc:add-extension menu)
  (gnc:add-extension
   (gnc:make-menu-item 
    ((menu-namer 'add-name) (_ "Style Sheets..."))
    (_ "Edit report style sheets.")
    (list "_Reports" "")
    (lambda ()
      (gnc:style-sheet-dialog-open))))

  (gnc:add-extension 
   (gnc:make-separator (list "_Reports" "")))

  ;; push reports (new items added on top of menu)
  (hash-for-each add-report-menu-item *gnc:_report-templates_*))

(define <report-template>
  (make-record-type "<report-template>"
                    ;; The data items in a report record
                    '(version name options-generator options-editor
                              renderer in-menu?)))

(define (gnc:define-report . args) 
  ;; For now the version is ignored, but in the future it'll let us
  ;; change behaviors without breaking older reports.
  ;;
  ;; The renderer should be a function that accepts one argument, a
  ;; set of options, and generates the report. the renderer must
  ;; return as its final value an <html-document> object.

  (define (blank-report)
    ;; Number of #f's == Number of data members
    ((record-constructor <report-template>) #f #f #f gnc:default-options-editor #f #t))

  (define (args-to-defn in-report-rec args)
    (let ((report-rec (if in-report-rec
                          in-report-rec
                          (blank-report))))
      (if (null? args)
          in-report-rec
          (let ((id (car args))
               (value (cadr args))
               (remainder (cddr args)))
            ((record-modifier <report-template> id) report-rec value)
            (args-to-defn report-rec remainder)))))

  (let ((report-rec (args-to-defn #f args)))
    (if (and report-rec
             (gnc:report-template-name report-rec))
        (hash-set! *gnc:_report-templates_*
                   (gnc:report-template-name report-rec) report-rec)
        (gnc:warn "gnc:define-report: bad report"))))

(define gnc:report-template-version
  (record-accessor <report-template> 'version))
(define gnc:report-template-name
  (record-accessor <report-template> 'name))
(define gnc:report-template-options-generator
  (record-accessor <report-template> 'options-generator))
(define gnc:report-template-options-editor
  (record-accessor <report-template> 'options-editor))
(define gnc:report-template-renderer
  (record-accessor <report-template> 'renderer))
(define gnc:report-in-menu?
  (record-accessor <report-template> 'in-menu?))

(define (gnc:report-template-new-options report-template)
  (let ((generator (gnc:report-template-options-generator report-template))
        (namer 
         (gnc:make-string-option 
          (N_ "General") (N_ "Report name") "0a"
          (N_ "Enter a descriptive name for this report")
          (gnc:report-template-name report-template)))
        (stylesheet 
         (gnc:make-multichoice-option 
          (N_ "General") (N_ "Stylesheet") "0b"
          (N_ "Select a stylesheet for the report.")
          (string->symbol (N_ "Default"))
          (map 
           (lambda (ss)
             (vector 
              (string->symbol (gnc:html-style-sheet-name ss))
              (gnc:html-style-sheet-name ss)
              (string-append (gnc:html-style-sheet-name ss) 
                             (_ " Stylesheet"))))
           (gnc:get-html-style-sheets)))))
    
    (if (procedure? generator)
        (let ((options (generator)))
          (gnc:register-option options stylesheet)
          (gnc:register-option options namer)
          options)
        (let ((options (gnc:new-options)))
          (gnc:register-option options stylesheet)
          (gnc:register-option options names)
          options))))

(define <report> 
  (make-record-type "<report>" 
                    '(type id options parents children 
                           dirty? display-list ctext)))

(define gnc:report-type 
  (record-accessor <report> 'type))

(define gnc:report-set-type!
  (record-modifier <report> 'type))

(define gnc:report-id 
  (record-accessor <report> 'id))

(define gnc:report-set-id!
  (record-modifier <report> 'id))

(define gnc:report-options 
  (record-accessor <report> 'options))

(define gnc:report-set-options!
  (record-modifier <report> 'options))

(define gnc:report-children 
  (record-accessor <report> 'children))

(define gnc:report-set-parents!
  (record-modifier <report> 'parents))

(define gnc:report-parents 
  (record-accessor <report> 'parents))

(define gnc:report-set-children!
  (record-modifier <report> 'children))

(define (gnc:report-add-child! report child)
  (gnc:report-add-parent! child report)
  (gnc:report-set-children! 
   report (cons child (gnc:report-children report))))

(define (gnc:report-add-child-by-id! report child)
  (let ((childrep (gnc:find-report child)))
    (if childrep
        (begin 
          (gnc:report-add-parent! childrep report)
          (gnc:report-set-children! 
           report (cons childrep (gnc:report-children report)))))))

(define (gnc:report-add-parent! report parent)
  (gnc:report-set-parents! 
   report (cons parent (gnc:report-parents report))))

(define gnc:report-dirty? 
  (record-accessor <report> 'dirty?))

(define gnc:report-set-dirty?-internal!
  (record-modifier <report> 'dirty?))

(define (gnc:report-set-dirty?! report val)
  (gnc:report-set-dirty?-internal! report val)
  (if val 
      (begin 
        ;; mark the parents as dirty 
        (for-each 
         (lambda (parent)
           (gnc:report-set-dirty?! parent val))
         (gnc:report-parents report))

        ;; reload the window 
        (for-each 
         (lambda (win)
           (gnc:report-window-reload win))
         (gnc:report-display-list report)))))

(define gnc:report-display-list 
  (record-accessor <report> 'display-list))

(define gnc:report-set-display-list!
  (record-modifier <report> 'display-list))

(define gnc:report-ctext 
  (record-accessor <report> 'ctext))

(define gnc:report-set-ctext!
  (record-modifier <report> 'ctext))

(define (gnc:report-register-display report window)
  (if (and window report)
      (if (not (member window (gnc:report-display-list report)))
          (gnc:report-set-display-list! 
           report 
           (cons window (gnc:report-display-list report))))))
  
(define (gnc:report-unregister-display report window)
  (if (and window report)
      (if (member window (gnc:report-display-list report))
          (gnc:report-set-display-list! 
           report 
           (delete window (gnc:report-display-list report))))))
  

(define (gnc:make-report template-name . rest)
  (let ((r ((record-constructor <report>) 
            template-name #f #f '() '() #t '() #f))
        (template (hash-ref *gnc:_report-templates_* template-name))
        (id *gnc:_report-next-serial_*))
    (gnc:report-set-id! r id)
    (set! *gnc:_report-next-serial_* (+ 1 id))
    (let ((options 
           (if (not (null? rest))
               (car rest)
               (gnc:report-template-new-options template))))
      (gnc:report-set-options! r options))

    (hash-set! *gnc:_reports_* (gnc:report-id r) r)
    id))

(define (gnc:make-report-options template-name)
  (let ((template (hash-ref *gnc:_report-templates_* template-name)))
    (if template
        (gnc:report-template-new-options template)
        #f)))

(define (gnc:report-options-editor report) 
  (let ((template 
         (hash-ref  *gnc:_report-templates_* 
                    (gnc:report-type report))))
    (if template
        (gnc:report-template-options-editor template)
        #f)))

(define (gnc:report-name report) 
  (gnc:option-value
   (gnc:lookup-option (gnc:report-options report)
                      (N_ "General") (N_ "Report name"))))

(define (gnc:report-stylesheet report)
  (gnc:html-style-sheet-find 
   (symbol->string (gnc:option-value
                    (gnc:lookup-option 
                     (gnc:report-options report)
                     (N_ "General") 
                     (N_ "Stylesheet"))))))

(define (gnc:report-set-stylesheet! report stylesheet)
  (gnc:option-set-value
   (gnc:lookup-option 
    (gnc:report-options report)
    (N_ "General") 
    (N_ "Stylesheet"))
   (string->symbol 
    (gnc:html-style-sheet-name stylesheet))))
  
;;; (define (gnc:report-default-options-editor)
;;;   (let* ((option-db #f)
;;;          (option-dlg #f))
;;;     (define (editor options action report-win)
;;;       (if (string? action)
;;;           (cond 
;;;            ;; open: start the options editor. 
;;;            ((string=? action "open")
;;;             (set! option-db 
;;;                   (gnc:option-db-new options))
;;;             (set! option-dlg 
;;;                   (gnc:options-dialog-new #t))
;;;             (gnc:build-options-dialog-contents 
;;;              option-dlg option-db)
;;;             ;; set up the default callbacks 
;;;             (gnc:report-default-options-setup option-dlg report-win))
           
;;;            ;; close: shut it down, probably because the report window
;;;            ;; is getting closed. 
;;;            ((string=? action "close")
;;;             (gnc:options-dialog-destroy option-dlg)))
             
(define (gnc:all-report-template-names)
  (sort 
   (hash-fold 
    (lambda (k v p)
      (cons k p)) 
    '() *gnc:_report-templates_*)
   string<?))

(define (gnc:report-remove-by-id id)
  (let ((r (hash-ref *gnc:_reports_* id)))
    (for-each 
     (lambda (child)
       (gnc:report-remove-by-id (gnc:report-id child)))
     (gnc:report-children r))
    (hash-remove! *gnc:_reports_* id)))

(define (gnc:find-report id) 
  (hash-ref *gnc:_reports_* id))

(define (gnc:backtrace-if-exception proc . args)
  (define (dumper key . args)
    (let ((stack (make-stack #t dumper)))
      (display-backtrace stack (current-error-port))
      (apply display-error stack (current-error-port) args)
      (throw 'ignore)))
  
  (catch 
   'ignore
   (lambda () 
     (lazy-catch #t 
                 (lambda () (apply proc args))
                 dumper))
   (lambda (key . args)
     #f)))


(define (gnc:report-render-html report)
  (if (and (not (gnc:report-dirty? report))
           (gnc:report-ctext report))
      ;; if there's clean cached text, return it 
      (begin 
        (gnc:report-ctext report))
      
      ;; otherwise, rerun the report 
      (let ((template (hash-ref *gnc:_report-templates_* 
                                (gnc:report-type report))))
        (if template
            (let* ((renderer (gnc:report-template-renderer template))
                   (stylesheet (gnc:report-stylesheet report))
                   (doc (renderer report))
                   (html #f))
              (gnc:html-document-set-style-sheet! doc stylesheet)
              (set! html (gnc:html-document-render doc))
              (gnc:report-set-ctext! report html)
              (gnc:report-set-dirty?! report #f)              
              html)
            #f))))

;; render the body of the report document (ignoring style sheet)
(define (gnc:report-render-body report)
  (if (and (not (gnc:report-dirty? report))
           (gnc:report-ctext report))
      ;; if there's clean cached text, return it 
      (begin 
        (gnc:report-ctext report))
      
      ;; otherwise, rerun the report 
      (let ((template (hash-ref *gnc:_report-templates_* 
                                (gnc:report-type report))))
        (if template
            (let* ((renderer (gnc:report-template-renderer template))
                   (stylesheet (gnc:report-stylesheet report))
                   (doc (renderer report))
                   (html #f))
              
              (gnc:html-document-push-style 
               doc (gnc:html-style-sheet-style stylesheet))
              (set! html (gnc:html-document-render-body doc))
              (gnc:report-set-ctext! report html)
              (gnc:report-set-dirty?! report #f)              
              html)
            #f))))


(define (gnc:report-run id)
  (gnc:backtrace-if-exception 
   (lambda ()
     (let ((report (gnc:find-report id))
           (start-time (gettimeofday))
           (html #f))
       (if report
           (begin 
             (set! html (gnc:report-render-html report))
             (display "total time to run report: ")
             (display (gnc:time-elapsed start-time (gettimeofday)))
             (newline)
             html)
           #f)))))
              
(gnc:hook-add-dangler gnc:*main-window-opened-hook* gnc:report-menu-setup)


