(define-module (gnucash business-utils))

(use-modules (gnucash gnc-module))
(gnc:module-load "gnucash/app-utils" 0)
(gnc:module-load "gnucash/business-core" 0)

(define gnc:*business-label* (N_ "Business"))
(define gnc:*company-name* (N_ "Company Name"))
(define gnc:*company-addy* (N_ "Company Address"))
(define gnc:*company-id* (N_ "Company ID"))
(define gnc:*company-phone* (N_ "Company Phone Number"))
(define gnc:*company-fax* (N_ "Company Fax Number"))
(define gnc:*company-url* (N_ "Company Website URL"))
(define gnc:*company-email* (N_ "Company Email Address"))
(define gnc:*company-contact* (N_ "Company Contact Person"))

(define (gnc:company-info key)
  ;; Access company info from key-value pairs for current book
  (kvp-frame-get-slot-path-gslist
    (gnc-book-get-slots (gnc-get-current-book))
    (append gnc:*kvp-option-path* (list gnc:*business-label* key))))

(export gnc:*business-label* gnc:*company-name*  gnc:*company-addy* 
        gnc:*company-id*     gnc:*company-phone* gnc:*company-fax* 
        gnc:*company-url*    gnc:*company-email* gnc:*company-contact*
        gnc:company-info)

(load-from-path "business-options.scm")
(load-from-path "business-prefs.scm")
