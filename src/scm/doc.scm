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

(define (build-path . elements)
  (string-join elements "/"))

(define (gnc:find-file file directories)
  "Find file named 'file' anywhere in 'directories'.  'file' must be a
string and 'directories' must be a list of strings."

  (gnc:debug "gnc:find-file looking for " file " in " directories)

  (do ((rest directories (cdr rest))
       (finished? #f)
       (result #f))
      ((or (null? rest) finished?) result)

    (let ((file-name (build-path (car rest) file)))
      (gnc:debug "  checking for " file-name)
      (if (access? file-name F_OK)
          (begin
            (gnc:debug "found file " file-name)
            (set! finished? #t)
            (set! result file-name))))))

(define (gnc:find-localized-file file base-directories)
  ;; Find file in path in base directories, or in any localized subdir
  ;; thereof.

  (define (locale-prefixes)
    ;; Mac OS X. 10.1 and earlier don't have LC_MESSAGES. Fall back to
    ;; LC_ALL for those systems.
    (let* ((locale (or (false-if-exception (setlocale LC_MESSAGES))
		       (setlocale LC_ALL)))
           (strings (cond ((not (string? locale)) '())
                          ((equal? locale "C") '())
                          ((<= (string-length locale) 4) (list locale))
                          (else (list (substring locale 0 2)
                                      (substring locale 0 5)
                                      locale)))))
      (reverse (cons "C" strings))))

  (let loop ((prefixes (locale-prefixes))
             (dirs base-directories))
    (if (null? dirs)
        #f
        (or (gnc:find-file file (map (lambda (prefix)
                                       (build-path (car dirs) prefix))
                                     prefixes))
            (gnc:find-file file (list (car dirs)))
            (loop prefixes (cdr dirs))))))

(define (gnc:find-doc-file file)
  (gnc:find-localized-file file (gnc:config-var-value-get gnc:*doc-path*)))

(define (remove-i18n-macros input)
  (cond ((null? input) input)
        ((list? input)
         (cond ((eq? (car input) 'N_) (cadr input))
               (else (cons (remove-i18n-macros (car input))
                           (remove-i18n-macros (cdr input))))))
        (else input)))

(define (fill-out-topics input)
  (define (first-non-blank-url input)
    (cond ((null? input) "")
          ((list? input)
           (cond ((and (string? (car input)) (not (eq? "" (cadr input))))
                  (cadr input))
                 (else (let ((first (first-non-blank-url (car input))))
                         (if (not (eq? "" first))
                             first
                             (first-non-blank-url (cdr input)))))))
          (else "")))

  (cond ((null? input) input)
        ((list? input)
         (cond ((and (string? (car input)) (eq? "" (cadr input)))
                (cons (car input)
                      (cons (first-non-blank-url (caddr input))
                            (fill-out-topics (cddr input)))))
               (else (cons (fill-out-topics (car input))
                           (fill-out-topics (cdr input))))))
        (else input)))

; (define (gnc:load-help-topics fname) 
;   ;; Should this be %load-path, or should we use doc-path, and should
;   ;; topics be a .scm file, or just a file since there's no code in
;   ;; there?
;   (with-input-from-file (%search-load-path fname)
;     (lambda ()
;       (fill-out-topics (remove-i18n-macros (read))))))
