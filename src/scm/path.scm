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

(define (gnc:locale-prefixes)
  (let* ((locale (setlocale LC_MESSAGES))
         (strings (cond ((not (string? locale)) ())
                        ((equal? locale "C") ())
                        ((<= (string-length locale) 2) (list locale))
                        (else (list (substring locale 0 2) locale)))))
    (reverse (cons "C" strings))))

(define (gnc:default-doc-dirs)
  (let ((user-paths (list
                     (list (getenv "HOME") ".gnucash" "html")))
        (locale-paths (map (lambda (prefix)
                             (list gnc:_share-dir-default_ "html" prefix))
                           (gnc:locale-prefixes)))
        (pix-paths (map (lambda (prefix)
                          (list gnc:_share-dir-default_ "html" prefix "image"))
                        (gnc:locale-prefixes)))
        (base-paths (list
                     (list gnc:_share-dir-default_ "html"))))
    (map (lambda (paths) (apply build-path paths))
         (append user-paths locale-paths pix-paths base-paths))))

(define (gnc:_expand-doc-path_ new-path)
  ;; FIXME: Bad items should probably cause this to abort with #f or
  ;; throw an exception.
  (gnc:debug "expanding doc-path value " new-path)
  (let ((path-interpret
         (lambda (item)
           (cond ((string? item) (list item))
                 ((symbol? item)
                  (case item
                    ((default) (gnc:default-doc-dirs))
                    ((current)
                     (gnc:config-var-value-get gnc:*doc-path*))
                    (else
                     (gnc:warn "bad item " item " in doc-path.  Ignoring.")
                     '())))
                 (else 
                  (gnc:warn "bad item " item " in doc-path.  Ignoring.")
                  '())))))
    (apply append (map path-interpret new-path))))

(define (gnc:make-home-dir)
  (let ((home-dir (build-path (getenv "HOME") ".gnucash")))
    (if (access? home-dir X_OK) #t (mkdir home-dir #o700))))

(define gnc:load-user-config-if-needed
  (let ((user-config-loaded? #f))
    (lambda ()
      (if (not user-config-loaded?)
          (begin
            (gnc:debug "loading user configuration")

            (let ((user-file
                   (build-path (getenv "HOME") ".gnucash" "config.user"))
                  (auto-file
                   (build-path (getenv "HOME") ".gnucash" "config.auto")))

              (if (access? user-file F_OK)
                  (if (false-if-exception (primitive-load user-file))
                      (set! user-config-loaded? #t)
                      (begin
                        (gnc:warn "failure loading " user-file)
                        #f))
                  (if (access? auto-file F_OK)
                      (if (false-if-exception (primitive-load auto-file))
                          (set! user-config-loaded? #t)
                          (begin
                            (gnc:warn "failure loading " auto-file)
                            #f))))))))))

;; the system config should probably be loaded from some directory
;; that wouldn't be a site wide mounted directory, like /usr/share
;; However, the code below seems to zero in on /usr/share/gnucash/config
;; ... ahh but that's OK, right ??
(define gnc:load-system-config-if-needed
  (let ((system-config-loaded? #f))
    (lambda ()
      (if (not system-config-loaded?)
          (begin
            (gnc:debug "loading system configuration")
            
            (let ((system-config (build-path
                                  (gnc:config-var-value-get gnc:*config-dir*)
                                  "config")))
              
              (if (false-if-exception (primitive-load system-config))
                  (set! system-config-loaded? #t)        
                  (begin
                    (gnc:warn "failure loading " system-config)
                    #f))))))))
