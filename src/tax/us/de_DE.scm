(define-module (gnucash tax de_DE))

(use-modules (gnucash gnc-module))
(gnc:module-load "gnucash/app-utils" 0)

(export gnc:txf-get-payer-name-source)
(export gnc:txf-get-form)
(export gnc:txf-get-description)
(export gnc:txf-get-format)
(export gnc:txf-get-multiple)
(export gnc:txf-get-category-key)
(export gnc:txf-get-help)
(export gnc:txf-get-codes)
(export gnc:txf-get-code-info)
(export txf-help-categories)
(export txf-income-categories)
(export txf-expense-categories)

(define gnc:*tax-label* (N_ "Tax"))
(define gnc:*tax-nr-label* (N_ "Tax Number"))

(export gnc:*tax-label* gnc:*tax-nr-label*)

(load-from-path "txf-de_DE.scm")
(load-from-path "txf-help-de_DE.scm")
