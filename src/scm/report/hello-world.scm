;; -*-scheme-*-

;; This is a sample guile report generator for GnuCash.
;; It illustrates the basic techniques used to create
;; new reports for GnuCash.

(gnc:support "report/hello-world.scm")

;; Putting your functions in a (let ()) block hides them
;; from the rest of guile.
(let ()

  ;; This function will generate a set of options that GnuCash
  ;; will use to display a dialog where the user can select
  ;; values for your report's parameters.
  (define (options-generator)

    ;; This will be the new set of options.
    (define gnc:*hello-world-options* (gnc:new-options))

    ;; This is just a helper function for making options.
    ;; See gnucash/src/scm/options.scm for details.
    (define (gnc:register-hello-world-option new-option)
      (gnc:register-option gnc:*hello-world-options* new-option))

    ;; This is a boolean option. It is in Section 'Hello, World!'
    ;; and is named 'Boolean Option'. Its sorting key is 'a',
    ;; thus it will come before options with sorting keys
    ;; 'b', 'c', etc. in the same section. The default value
    ;; is #t (true). The phrase 'This is a boolean option'
    ;; will be displayed as help text when the user puts
    ;; the mouse pointer over the option.
    (gnc:register-hello-world-option
     (gnc:make-simple-boolean-option
      "Hello, World!" "Boolean Option"
      "a" "This is a boolean option." #t))

    ;; This is a multichoice option. The user can choose between
    ;; the values 'first, 'second, 'third, or 'fourth. These are guile
    ;; symbols. The value 'first will be displayed as "First Option"
    ;; and have a help string of "Help for first option.". The default
    ;; value is 'third.
    (gnc:register-hello-world-option
     (gnc:make-multichoice-option
      "Hello, World!" "Multi Choice Option"
      "b" "This is a multi choice option." 'third
      (list #(first  "First Option"   "Help for first option")
            #(second "Second Option"  "Help for second option")
            #(third  "Third Option"   "Help for third option")
            #(fourth "Fourth Options" "The fourth option rules!"))))

    ;; This is a string option. Users can type anything they want
    ;; as a value. The default value is "Hello, World". This is
    ;; in the same section as the option above. It will be shown
    ;; after the option above because its key is 'b' while the
    ;; other key is 'a'.
    (gnc:register-hello-world-option
     (gnc:make-string-option
      "Hello, World!" "String Option"
      "c" "This is a string option" "Hello, World"))

    ;; This is a date/time option. The user can pick a date and,
    ;; possibly, a time. Times are stored as a pair
    ;; (seconds . nanoseconds) measured from Jan 1, 1970, i.e.,
    ;; Unix time. The last option is false, so the user can only
    ;; select a date, not a time. The default value is the current
    ;; time.
    (gnc:register-hello-world-option
     (gnc:make-date-option
      "Hello, World!" "Just a Date Option"
      "d" "This is a date option"
      (lambda () (cons 'absolute (cons (current-time) 0)))
      #f 'absolute #f ))

    ;; This is another date option, but the user can also select
    ;; the time.
    (gnc:register-hello-world-option
     (gnc:make-date-option
      "Hello, World!" "Time and Date Option"
      "e" "This is a date option with time"
      (lambda () (cons 'absolute (cons (current-time) 0)))
      #t 'absolute #f ))

    (gnc:register-hello-world-option
     (gnc:make-date-option
      "Hello, World!" "Combo Date Option"
      "y" "This is a combination date option"
      (lambda () (cons 'relative 'start-cal-year))
      #f 'both '(start-cal-year start-prev-year end-prev-year) ))

    (gnc:register-hello-world-option
     (gnc:make-date-option
      "Hello, World!" "Relative Date Option"
      "x" "This is a relative date option"
      (lambda () (cons 'relative 'start-cal-year))
      #f 'relative '(start-cal-year start-prev-year end-prev-year) ))

    ;; This is a number range option. The user can enter a number
    ;; between a lower and upper bound given below. There are also
    ;; arrows the user can click to go up or down, the amount changed
    ;; by a single click is given by the step size.
    (gnc:register-hello-world-option
     (gnc:make-number-range-option
      "Hello, World!" "Number Option"
      "ee" "This is a number option."
      1500.0  ;; default
      0.0     ;; lower bound
      10000.0 ;; upper bound
      2.0     ;; number of decimals
      0.01    ;; step size
      ))

    ;; This is a color option, defined by rgba values. A color value
    ;; is a list where the elements are the red, green, blue, and
    ;; alpha channel values respectively. The penultimate argument
    ;; (255) is the allowed range of rgba values. The final argument
    ;; (#f) indicates the alpha value should be ignored. You can get
    ;; a color string from a color option with gnc:color-option->html,
    ;; which will scale the values appropriately according the range.
    (gnc:register-hello-world-option
     (gnc:make-color-option
      "Hello, World!" "Background Color"
      "f" "This is a color option"
      (list #xf6 #xff #xdb 0)
      255
      #f))

    ;; This is an account list option. The user can select one
    ;; or (possibly) more accounts from the list of accounts
    ;; in the current file. Values are scheme handles to actual
    ;; C pointers to accounts. They can be used in conjunction
    ;; with the wrapped C functions in gnucash/src/g-wrap/gnc.gwp.
    ;; The #f value indicates that any account will be accepted.
    ;; Instead of a #f values, you could provide a function that
    ;; accepts a list of account values and returns a pair. If
    ;; the first element is #t, the second element is the list
    ;; of accounts actually accepted. If the first element is
    ;; #f, the accounts are rejected and the second element is
    ;; and error string. The last argument is #t which means
    ;; the user is allowed to select more than one account.
    ;; The default value for this option is the currently
    ;; selected account in the main window, if any.
    (gnc:register-hello-world-option
     (gnc:make-account-list-option
      "Hello Again" "An account list option"
      "g" "This is an account list option"
      (lambda () (gnc:get-current-accounts))
      #f #t))

    ;; This is a list option. The user can select one or (possibly)
    ;; more values from a list. The list of acceptable values is
    ;; the same format as a multichoice option. The value of the
    ;; option is a list of symbols.
    (gnc:register-hello-world-option
     (gnc:make-list-option
      "Hello Again" "A list option"
      "h" "This is a list option"
      (list 'good)
      (list #(good  "The Good" "Good option")
            #(bad   "The Bad"  "Bad option")
            #(ugly  "The Ugly" "Ugly option"))))

    ;; This option is for testing. When true, the report generates
    ;; an exception.
    (gnc:register-hello-world-option
     (gnc:make-simple-boolean-option
      "Testing" "Crash the report"
      "a" (string-append "This is for testing. "
                         "Your reports probably shouldn't have an "
                         "option like this.") #f))

    (gnc:options-set-default-section gnc:*hello-world-options*
                                     "Hello, World!")

    gnc:*hello-world-options*)


  ;; Now we create a string database that will make it easier to
  ;; internationalize this report. Strings that are stored into
  ;; this database are registered with another component that
  ;; can save all such strings into a file suitable for gettext
  ;; parsing. Strings that are retrieved from the database are
  ;; automatically translated, if a translation is available.
  ;; Otherwise, the original string is used.
  (define string-db (gnc:make-string-database))

  ;; Here is a helper function for getting the translation of
  ;; a boolean value.
  (define (hello-world:bool->string value)
    (string-db 'lookup (if value 'true 'false)))

  ;; This is a helper function to generate an html list of account names
  ;; given an account list option.
  (define (account-list accounts)
    (let ((names (map gnc:account-get-name accounts)))
      (if (null? accounts)
          (list "<p>" (string-db 'lookup 'no-accounts) "</p>")
          (list "<p>" (string-db 'lookup 'selected-accounts) "</p>"
                "<ul>"
                (map (lambda (name) (list "<li>" name "</li>")) names)
                "</ul>"))))

  ;; This is a helper function to generate an html list for the list option.
  (define (list-option-list values)
    (let ((names (map symbol->string values)))
      (if (null? values)
          (list "<p>" (string-db 'lookup 'no-values) "</p>")
          (list "<p>" (string-db 'lookup 'selected-values) "</p>"
                "<ul>"
                (map (lambda (name) (list "<li>" name "</li>")) names)
                "</ul>"))))

  ;; Here's another helper function. You can guess what it does.
  (define (bold string)
    (string-append "<b>" string "</b>"))

  ;; Here's a helper function for making some of the paragraphs below.
  (define (make-para key . values)
    (let ((args (append (list #f (string-db 'lookup key)) values)))
      (html-para (apply sprintf args))))


  ;; This is the rendering function. It accepts a database of options
  ;; and generates a (possibly nested) list of html. The "flattened"
  ;; and concatenated list should be valid html. The option database
  ;; passed to the function is one created by the options-generator
  ;; function defined above.
  (define (hello-world-renderer options)

    ;; These are some helper functions for looking up option values.
    (define (get-op section name)
      (gnc:lookup-option options section name))

    (define (op-value section name)
      (gnc:option-value (get-op section name)))

    ;; The first thing we do is make local variables for all the specific
    ;; options in the set of options given to the function. This set will
    ;; be generated by the options generator above.
    (let ((bool-val     (op-value "Hello, World!" "Boolean Option"))
          (mult-val     (op-value "Hello, World!" "Multi Choice Option"))
          (string-val   (op-value "Hello, World!" "String Option"))
          (date-val     (gnc:date-option-absolute-time (op-value "Hello, World!" "Just a Date Option")))
          (date2-val    (gnc:date-option-absolute-time (op-value "Hello, World!" "Time and Date Option")))
	  (rel-date-val (gnc:date-option-absolute-time (op-value "Hello, World!" "Relative Date Option")))
	  (combo-date-val (gnc:date-option-absolute-time (op-value "Hello, World!" "Combo Date Option")))
          (num-val      (op-value "Hello, World!" "Number Option"))
          (color-op     (get-op   "Hello, World!" "Background Color"))
          (accounts     (op-value "Hello Again"   "An account list option"))
          (list-val     (op-value "Hello Again"   "A list option"))
          (crash-val    (op-value "Testing"       "Crash the report")))

      ;; Crash if asked to.
      (if crash-val (string-length #f));; string-length needs a string

      (let ((time-string (strftime "%X" (localtime (current-time))))
            (date-string (strftime "%x" (localtime (car date-val))))
            (date-string2 (strftime "%x %X" (localtime (car date2-val))))
	    (rel-date-string (strftime "%x" (localtime (car rel-date-val))))
	(combo-date-string (strftime "%x" (localtime (car combo-date-val)))))

        ;; Here's where we generate the html. A real report would need
        ;; much more code and involve many more utility functions. See
        ;; the other reports for details. Note that you can used nested
        ;; lists here, as well as arbitrary functions.

        (list
         (html-start-document-color (gnc:color-option->html color-op))

         ;; Here we get the title using the string database and 'lookup.
         "<h2>" (string-db 'lookup 'title) "</h2>"

         ;; Here we user our paragraph helper
         (make-para 'para-1
                    (string-append "<tt>" gnc:_share-dir-default_
                                   "/gnucash/scm/report" "</tt>"))

         (make-para 'para-2
                    (string-append
                     "<a href=\"mailto:gnucash-devel@gnucash.org\">"
                     "<tt>gnucash-devel@gnucash.org</tt></a>")
                    (string-append
                     "<a href=\"http://www.gnucash.org\">"
                     "<tt>www.gnucash.org</tt></a>"))

         (make-para 'time-string (bold time-string))

         (make-para 'bool-string (bold (hello-world:bool->string bool-val)))

         (make-para 'multi-string (bold (symbol->string mult-val)))

         (make-para 'string-string (bold string-val))

         (make-para 'date-string (bold date-string))

         (make-para 'time-date-string (bold date-string2))
	 
	 (make-para 'rel-date-string (bold rel-date-string))

	 (make-para 'combo-date-string (bold combo-date-string))

         (make-para 'num-string-1 (bold (number->string num-val)))

         ;; Here we print the value of the number option formatted as
         ;; currency. When printing currency values, you should use
         ;; the function (gnc:amount->string), which is defined in
         ;; report-utilities. This functions will format the number
         ;; appropriately in the current locale. Don't try to format
         ;; it yourself -- it will be wrong in other locales.
         (make-para 'num-string-2
                    (bold
                     (gnc:amount->string num-val
                                         (gnc:default-print-info #f))))

         (list-option-list list-val)

         (account-list accounts)

         (make-para 'nice-day)

         (html-end-document)))))


  ;; Now we store the actual strings we are going to use and
  ;; finally define the report. Commands to execute must come
  ;; after definitions in guile.


  ;; Here we store the string used for the title of the report.
  ;; The key 'title will be used to get the string (or the
  ;; translation of the string) later.
  (string-db 'store 'title "Hello, World")

  ;; Now we store the first paragraph as one entire string. The '%s'
  ;; in the string will be used to substitute for a filename using
  ;; sprintf. In general, it is better to use sprintf on a whole
  ;; sentence or paragraph, than to build up the string using
  ;; concatentation. When it is translated into other languages,
  ;; the order of the phrases may need to be changed.
  (let ((string (string-append "This is a sample GnuCash report. "
                               "See the guile (scheme) source code in %s "
                               "for details on writing your own reports, "
                               "or extending existing reports.")))
    (string-db 'store 'para-1 string))

  ;; Second paragraph
  (let ((string
         (string-append
          "For help on writing reports, or to contribute your brand "
          "new, totally cool report, consult the mailing list %s. "
          "For details on subscribing to that list, see %s.")))
    (string-db 'store 'para-2 string))


  ;; Here we create some more phrases used in the report.
  (string-db 'store 'time-string "The current time is %s.")
  (string-db 'store 'bool-string "The boolean option is %s.")
  (string-db 'store 'multi-string "The multi-choice option is %s.")
  (string-db 'store 'string-string "The string option is %s.")
  (string-db 'store 'date-string "The date option is %s.")
  (string-db 'store 'time-date-string "The date and time option is %s.")
  (string-db 'store 'rel-date-string "The relative date option is %s.")
  (string-db 'store 'combo-date-string "The combination date option is %s.")
  (string-db 'store 'num-string-1 "The number option is %s.")
  (string-db 'store 'num-string-2
             "The number option formatted as currency is %s.")
  (string-db 'store 'nice-day "Have a nice day!")

  (string-db 'store 'true "true")
  (string-db 'store 'false "false")

  (string-db 'store 'no-accounts
             "There are no selected accounts in the account list option.")
  (string-db 'store 'selected-accounts
             "The accounts selected in the account list option are:")

  (string-db 'store 'no-values
             "You have selected no values in the list option.")
  (string-db 'store 'selected-values
             "The items selected in the list option are:")


  ;; Here we define the actual report with gnc:define-report
  (gnc:define-report

   ;; The version of this report.
   'version 1

   ;; The name of this report. This will be used, among other things,
   ;; for making its menu item in the main menu. You need to use the
   ;; untranslated value here! It will be registered and translated
   ;; elsewhere.
   'name "Hello, World"

   ;; The options generator function defined above.
   'options-generator options-generator

   ;; The rendering function defined above.
   'renderer hello-world-renderer))
