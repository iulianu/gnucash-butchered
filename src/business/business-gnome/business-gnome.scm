(define-module (gnucash business-gnome))
(use-modules (g-wrapped gw-business-gnome))
(use-modules (gnucash gnc-module))

(gnc:module-load "gnucash/gnome-utils" 0)
(gnc:module-load "gnucash/business-core" 0)
(gnc:module-load "gnucash/business-utils" 0)
(gnc:module-load "gnucash/gnome-search" 0)
(gnc:module-load "gnucash/business-core-file" 0)
(gnc:module-load "gnucash/dialog-tax-table" 0)

(gnc:module-load "gnucash/report/report-gnome" 0)

(use-modules (gnucash report business-reports))
(use-modules (gnucash main))		;for gnc:debug

(export gnc:reload-module)

(define (gnc:reload-module name)
  (let ((m (current-module)))
    (load-from-path name)
    (set-current-module m)))

(define (business-report-function)
  (gnc:add-extension
   (gnc:make-menu gnc:menuname-business-reports
		  (list gnc:menuname-reports))))

(gnc:hook-add-dangler gnc:*report-hook* business-report-function)
