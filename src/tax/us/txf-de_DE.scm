;; -*-scheme-*-
;; 
;; This file was copied from the file txf.scm by  Richard -Gilligan- Uschold
;;
;; Originally, these were meant to hold the codes for the US tax TXF
;; format. I modified this heavily so that it might become useful for
;; the German Umsatzsteuer-Voranmeldung. 
;; 
;; This file holds all the Kennzahlen for the
;; Umsatzsteuer-Voranmeldung and their explanations, which can be
;; assigned to particular accounts via the "Edit -> Tax options"
;; dialog. The report in taxtxf-de_DE.scm then will extract the
;; numbers for these Kennzahlen from the actual accounts for a given
;; time period, and will write it to some XML file as required by
;; e.g. the Winston software
;; http://www.felfri.de/winston/schnittstellen.htm
;;
(define (gnc:txf-get-payer-name-source categories code)
  (gnc:txf-get-code-info categories code 0))
(define (gnc:txf-get-form categories code)
  (gnc:txf-get-code-info categories code 1))
(define (gnc:txf-get-description categories code)
  (gnc:txf-get-code-info categories code 2))
(define (gnc:txf-get-format categories code)
  (gnc:txf-get-code-info categories code 3))
(define (gnc:txf-get-multiple categories code)
  (gnc:txf-get-code-info categories code 4))
(define (gnc:txf-get-category-key categories code)
  (gnc:txf-get-code-info categories code 5))
(define (gnc:txf-get-help categories code)
  (let ((pair (assv code txf-help-strings)))
    (if pair
        (cdr pair)
        "No help available.")))

(define (gnc:txf-get-codes categories)
  (map car categories))

;;;; Private

(define (gnc:txf-get-code-info categories code index)
  (vector-ref (cdr (assv code categories)) index))

(define txf-help-categories
  (list
   (cons 'H000 #(current "help" "Name of Current account is exported." 0 #f ""))
   (cons 'H002 #(parent "help" "Name of Parent account is exported." 0 #f ""))
   (cons 'H003 #(not-impl "help" "Not implemented yet, Do NOT Use!" 0 #f ""))))

;; We use several formats; nr. 1 means Euro+Cent, nr. 2 means only full Euro

;; Also, we abuse the "category-key" for now to store the Kennzahl as
;; well. We are not yet sure what to use the "form" field for.

;; Format: (name-source  form  description  format  multiple  category-key)
(define txf-income-categories
  (list
   (cons 'N000 #(none "" "Nur zur Voransicht im Steuer-Bericht -- kein Export" 0 #f ""))

   (cons 'K41 #(none "41" "Innergemeinschaftliche Lieferungen an Abnehmer mit USt-IdNr. " 2 #f "41"))
   (cons 'K44 #(none "44" "Innergemeinschaftliche Lieferungen neuer Fahrzeuge an Abnehmer ohne USt-IdNr" 2 #f "44"))
   (cons 'K49 #(none "49" "Innergemeinschaftliche Lieferungen neuer Fahrzeuge au�erhalb eines Unternehmens" 2 #f "49"))
   (cons 'K43 #(none "43" "Weitere steuerfreie Ums�tze mit Vorsteuerabzug" 2 #f "43"))
   (cons 'K48 #(none "48" "Steuerfreie Ums�tze ohne Vorsteuerabzug" 2 #f "48"))
   
   (cons 'K51 #(none "51" "Steuerpflichtige Ums�tze, Steuersatz 16 v.H." 2 #f "51"))
   (cons 'K86 #(none "86" "Steuerpflichtige Ums�tze, Steuersatz 7 v.H." 2 #f "86"))
   (cons 'K35 #(none "35" "Ums�tze, die anderen Steuers�tzen unterliegen (Bemessungsgrundlage)" 2 #f "35"))
   (cons 'K36 #(none "36" "Ums�tze, die anderen Steuers�tzen unterliegen (Steuer)" 1 #f "36"))
   (cons 'K77 #(none "77" "Ums�tze land- und forstwirtschaftlicher Betriebe:: Lieferungen in das �brige Gemeinschaftsgebiet an Abnehmer mit USt-IdNr." 2 #f "77"))
   (cons 'K76 #(none "76" "Ums�tze, f�r die eine Steuer nach � 24 UStG zu entrichten ist (Bemessungsgrundlage)" 2 #f "76"))
   (cons 'K80 #(none "80" "Ums�tze, f�r die eine Steuer nach � 24 UStG zu entrichten ist (Steuer)" 1 #f "80"))

   (cons 'K39 #(none "39" "Anrechnung (Abzug) der festgesetzten Sondervorauszahlung f�r Dauerfristverl�ngerung" 1 #f "39"))
   (cons 'K83 #(none "83" "Verbleibende Umsatzsteuer-Vorauszahlung" 1 #f "83"))

   ))


;; We use several formats; nr. 1 means Euro+Cent, nr. 2 means only full Euro

;; Also, we abuse the "category-key" for now to store the Kennzahl as
;; well. We are not yet sure what to use the "form" field for.

;; Format: (name-source  form  description  format  multiple  category-key)
(define txf-expense-categories
  (list
   (cons 'N000 #(none "" "Nur zur Voransicht im Steuer-Bericht -- kein Export" 0 #f ""))

   (cons 'K91 #(none "91" "Steuerfreie innergemeinschaftliche Erwerbe nach �4b UStG" 2 #f "91"))
   (cons 'K97 #(none "97" "Steuerpflichtige innergemeinschaftliche Erwerbe zum Steuersatz von 16 v.H." 2 #f "97"))
   (cons 'K93 #(none "93" "Steuerpflichtige innergemeinschaftliche Erwerbe zum Steuersatz von 7 v.H." 2 #f "93"))
   (cons 'K95 #(none "95" "Steuerpflichtige innergemeinschaftliche Erwerbe zu anderen Steuers�tzen (Bemessungsgrundlage)" 2 #f "95"))
   (cons 'K98 #(none "98" "Steuerpflichtige innergemeinschaftliche Erwerbe zu anderen Steuers�tzen (Steuer)" 1 #f "98"))
   (cons 'K94 #(none "94" "Innergemeinschaftliche Erwerbe neuer Fahrzeuge von Lieferern ohne USt-IdNr. (Bemessungsgrundlage)" 2 #f "94"))
   (cons 'K96 #(none "96" "Innergemeinschaftliche Erwerbe neuer Fahrzeuge von Lieferern ohne USt-IdNr. (Steuer)" 1 #f "96"))
   (cons 'K42 #(none "42" "Lieferungen des ersten Abnehmers (�25b Abs. 2 UStG) bei innergemeinschaftlichen Dreiecksgesch�ften" 2 #f "42"))

   (cons 'K54 #(none "54" "Ums�tze, f�r die der Leistungsempf�nger die Steuer nach �13b Abs 2 UStG schuldet - zum Steuersatz von 16 v.H. (Bemessungsgrundlage)" 2 #f "54"))
   (cons 'K55 #(none "55" "Ums�tze, f�r die der Leistungsempf�nger die Steuer nach �13b Abs 2 UStG schuldet - zum Steuersatz von 7 v.H. (Bemessungsgrundlage)" 2 #f "55"))
   (cons 'K57 #(none "57" "Ums�tze, f�r die der Leistungsempf�nger die Steuer nach �13b Abs 2 UStG schuldet - zu anderen Steuers�tzen (Bemessungsgrundlage)" 2 #f "57"))
   (cons 'K45 #(none "45" "Ums�tze, f�r die der Leistungsempf�nger die Steuer nach �13b Abs 2 UStG schuldet - Nicht steuerbare Ums�tze (Bemessungsgrundlage)" 2 #f "45"))

   (cons 'K58 #(none "58" "Ums�tze, f�r die der Leistungsempf�nger die Steuer nach �13b Abs 2 UStG schuldet - zu anderen Steuers�tzen (Steuer)" 1 #f "58"))
   (cons 'K65 #(none "65" "Steuer infolge Wechsels der Besteuerungsart/-form" 1 #f "65"))

   (cons 'K66 #(none "66" "Vorsteuerbetr�ge aus Rechnungen von anderen Unternehmern" 1 #f "66"))
   (cons 'K61 #(none "61" "Vorsteuerbetr�ge aus dem innergemeinschaftlichen Erwerb von Gegenst�nden" 1 #f "61"))
   (cons 'K62 #(none "62" "Entrichtete Einfuhrumsatzsteuer" 1 #f "62"))
   (cons 'K67 #(none "67" "Vorsteuerbetr�ge aus Leistungen im Sinne des $13b Abs. 1 UStG" 1 #f "67"))
   (cons 'K63 #(none "63" "Vorsteuerbetr�ge, die nach allgemeinen Durchschnittss�tzen berechnet sind" 1 #f "63"))
   (cons 'K64 #(none "64" "Berichtigung des Vorsteuerabzugs" 1 #f "64"))
   (cons 'K59 #(none "59" "Vorsteuerabzug f�r innergemeinschaftliche Lieferungen neuer Fahrzeuge au�erhalb eines Unternehmens" 1 #f "59"))

   (cons 'K69 #(none "69" "Steuerbetr�ge, die vom letzten Abnehmer eines innergemeinschaftlichen Dreiecksgesch�fts geschuldet werden" 1 #f "69"))

   ))



;;; Register global options in this book
(define (book-options-generator options)
  (define (reg-option new-option)
    (gnc:register-option options new-option))

  (reg-option
   (gnc:make-string-option
    gnc:*tax-label* gnc:*tax-nr-label*
    "a" (N_ "The electronic tax number of your business") ""))
  )

(gnc:register-kvp-option-generator gnc:id-book book-options-generator)
