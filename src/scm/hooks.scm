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

;;; 
;;; Code to support emacs-inspired hooks.
;;;

;;;; This is not functional yet, but it should be close...

;;; Private

;; Central repository for all hooks -- so we can look them up later by name.
(define gnc:*hooks* '())

;;; Developers

(define (gnc:hook-define name description)
  (let ((hook-data (vector name description '())))
    (set! gnc:*hooks* (assoc-set! gnc:*hooks* name hook-data))
    hook-data))

(define (gnc:hook-danglers-get hook)
  (vector-ref hook 2))

(define (gnc:hook-danglers-set! hook danglers)
  (vector-set! hook 2 danglers))

(define (gnc:hook-danglers->list hook)
  (gnc:hook-danglers-get hook))

(define (gnc:hook-replace-danglers hook function-list)
  (gnc:hook-danglers-set! hook function-list))

(define (gnc:hook-run-danglers hook . args)
  (gnc:debug "Running functions on hook " (gnc:hook-name-get hook))
  (for-each (lambda (dangler)
              (if (gnc:debugging?)
                  (begin
                    (display "  ") (display dangler) (newline)))
              (apply dangler args))
            (gnc:hook-danglers-get hook)))

;;; Public

(define (gnc:hook-lookup name)
  (assoc-ref gnc:*hooks* name))

(define (gnc:hook-add-dangler hook function)
  (let ((danglers (gnc:hook-danglers-get hook)))
    (gnc:hook-danglers-set! hook (append danglers (list function)))))

(define (gnc:hook-remove-dangler hook function)
  (let ((danglers (gnc:hook-danglers-get hook)))
    (gnc:hook-danglers-set! hook (delq! function danglers))))

(define (gnc:hook-description-get hook)
  (vector-ref hook 1))

(define (gnc:hook-name-get hook)
  (vector-ref hook 0))

(define gnc:*startup-hook*
  (gnc:hook-define
   'startup-hook
   "Functions to run at startup.  Hook args: ()"))

(define gnc:*shutdown-hook*
  (gnc:hook-define 
   'shutdown-hook
   "Functions to run at guile shutdown.  Hook args: ()"))

(define gnc:*ui-shutdown-hook*
  (gnc:hook-define 
   'ui-shutdown-hook
   "Functions to run at ui shutdown.  Hook args: ()"))

(define gnc:*main-window-opened-hook*
  (gnc:hook-define
   'main-window-opened-hook
   "Functions to run whenever the main window is opened.  Hook args: (window)"))

;;(let ((hook (gnc:hook-lookup 'startup-hook)))
;;  (display (gnc:hook-name-get hook))
;;  (newline)
;;  (display (gnc:hook-description-get hook))
;;  (newline)
;;  (gnc:hook-add-dangler hook (lambda ()
;;                                   (display "Running a simple startup hook")
;;                                   (newline)))
;;  (gnc:hook-run-danglers hook))
