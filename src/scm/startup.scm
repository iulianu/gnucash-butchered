;;;; startup.scm
;;
;; Minimal startup code.  This file should just contain enough code to
;; get the arguments parsed and things like gnc:*load-path* set up.
;; After that *everything* should be loaded via gnc:load.

;; This load should go away when guile gets fixed.  as of guile1.3,
;; it's not.  You have to do this manually, unless you call scm_shell,
;; which we can't.
(let ((boot-file (if (assoc 'prefix %guile-build-info)
		     (string-append (cdr (assoc 'prefix %guile-build-info))
				    "/share/guile/"
				    (version)
				    "/ice-9/boot-9.scm")
                     "/usr/share/guile/1.3a/ice-9/boot-9.scm")))
  (primitive-load boot-file))

(define gnc:*command-line-files* #f)

;;;; Warning functions...

(define (gnc:warn . items)
  (display "gnucash: [W] ")
  (for-each (lambda (i) (display i)) items)
  (newline))

(define (gnc:error . items)
  (display "gnucash: [E] ")
  (for-each (lambda (i) (display i)) items)
  (newline))

(define (gnc:debug . items)
  (if (gnc:config-var-value-get gnc:*debugging?*)
      (begin
        (display "gnucash: [D] ")
        (for-each (lambda (i) (display i)) items)
        (newline))))

(define (gnc:startup)
  (gnc:debug "starting up.")
  #t)

(define (gnc:shutdown exit-status)
  (gnc:debug "Shutdown -- exit-status: " exit-status)

  (gnc:hook-run-danglers gnc:*shutdown-hook*)
  (_gnc_shutdown_ exit-status)
  (exit exit-status))

(define gnc:*user-config-loaded?* #f)

(define (gnc:load-user-config)
  (gnc:debug "loading user configuration")

  (let ((user-file
         (string-append (getenv "HOME") "/.gnucash/config.user"))
        (auto-file
         (string-append (getenv "HOME") "/.gnucash/config.auto")))
    
    (if (access? user-file F_OK)
        (if (false-if-exception (primitive-load user-file))
            (set! gnc:user-config-loaded #t)
            (begin
              (gnc:warn "failure loading " user-file)
              #f))
        (if (access? auto-file F_OK)
            (if (false-if-exception (primitive-load auto-file))
                (set! gnc:*user-config-loaded?* #t)
                (begin
                  (gnc:warn "failure loading " auto-file)
                  #f))))))

(define gnc:*system-config-loaded?* #f)

(define (gnc:load-system-config)
  (gnc:debug "loading system configuration")

  (let ((system-config (string-append
                        (gnc:config-var-value-get gnc:*config-dir*)
                        "/config")))
    
    (if (false-if-exception (primitive-load system-config))
        (set! gnc:*system-config-loaded?* #t)        
        (begin
          (gnc:warn "failure loading " system-config)
          #f))))

(define gnc:_load-path-directories_ #f)
(define gnc:_doc-path-directories_ #f)

(define (directory? path)
  ;; This follows symlinks normally.
  (let* ((status (stat path))
         (type (if status (stat:type status) #f)))
    (eq? type 'directory)))
  

(define (gnc:directory-subdirectories dir-name)
  ;; Return a recursive list of the subdirs of dir-name, including
  ;; dir-name.  Follow symlinks.
  
  (let ((dir-port (opendir dir-name)))
    (if (not dir-port)
        #f
        (do ((item (readdir dir-port) (readdir dir-port))
             (dirs '()))
            ((eof-object? item) (reverse dirs))

          (if (not (or (string=? item ".")
                       (string=? item "..")))
              (let* ((full-path (string-append dir-name "/" item)))
                ;; ignore symlinks, etc.
                (if (access? full-path F_OK)
                    (let* ((status (lstat full-path))
                           (type (if status (stat:type status) #f)))
                      (if (and (eq? type 'directory))
                          (set! dirs
                                (cons full-path 
                                      (append 
                                       (gnc:directory-subdirectories full-path)
                                       dirs))))))))))))

(define (gnc:_path-expand_ items default-items)
  (if
   (null? items)
   '()  
   (let ((item (car items))
         (other-items (cdr items)))
     (cond
      ((eq? item 'default)
       (append 
        (gnc:_path-expand_ default-items))
       (gnc:_path-expand_ other-items default-items))
      ((string? item)
       (if (and (char=? #\( (string-ref item 0))
                (char=? #\) (string-ref item (- (string-length item) 1))))
           
           (let ((current-dir
                  (make-shared-substring item 1 (- (string-length item) 1))))
             
             (if (directory? current-dir)
                 (let ((subdirs (gnc:directory-subdirectories current-dir))
                       (rest (gnc:_path-expand_ other-items default-items)))
                   (cons current-dir (append subdirs rest)))
                 (begin
                   (gnc:warn "Non-directory " current-dir
                             "in gnc:_path-expand_ item."
                             "Ignoring.")
                   '())))
           (if (directory? item)
               (begin
                 (gnc:warn "Non-directory " item "in gnc:_path-expand_ item."
                           "Ignoring.")
                 '())
               (cons item (gnc:_path-expand_ other-items default-items)))))
      (else (gnc:warn "Invalid item " item " in gnc:_path-expand_.  Ignoring")
            (gnc:_path-expand_ other-items default-items))))))

(define (gnc:_load-path-update_ items)
  (let ((result (gnc:_path-expand_
                 items
                 (gnc:config-var-default-value-get gnc:*load-path*))))
    (if result
        (begin
          (set! gnc:_load-path-directories_ result)
          result)
        #f)))

(define (gnc:_doc-path-update_ items)
  (let ((result (gnc:_path-expand_
                 items
                 (gnc:config-var-default-value-get gnc:*doc-path*))))
    (if result
        (begin
          (set! gnc:_doc-path-directories_ result)
          result)
        #f)))

(define (gnc:find-in-directories file directories)
  "Find file named 'file' anywhere in 'directories'.  'file' must be a
string and 'directories' must be a list of strings."

  (gnc:debug "gnc:find-in-directories looking for " file " in " directories)
  
  (do ((rest directories (cdr rest))
       (finished? #f)
       (result #f))
      ((or (null? rest) finished?) result)
    
    (let ((file-name (string-append (car rest) "/" file)))
      (gnc:debug "  checking for " file-name)
      (if (access? file-name F_OK)
          (begin
            (gnc:debug "found file " file-name)
            (set! finished? #t)
            (set! result file-name))))))

;; It may make sense to dump this in favor of guile's load-path later,
;; but for now this works, and having gnc things separate may be less
;; confusing and avoids shadowing problems.

(define (gnc:load name)
  "Name must be a string.  The system attempts to locate the file of
the given name and load it.  The system will attempt to locate the
file in all of the directories specified by gnc:*load-path*."
  
  (let ((file-name (gnc:find-in-directories name gnc:_load-path-directories_)))
    (if (not file-name)
        #f
        (if (false-if-exception (primitive-load file-name))
            (begin
              (gnc:debug "loaded file " file-name)
              #t)
            (begin
              (gnc:warn "failure loading " file-name)
              #f)))))

;;; config-var: You can create them, set values, find out of the value
;;; is different from the default, and you can get a description.  You
;;; can also specify an action function which will be called whenever
;;; the value is changed.  The action function receives the special
;;; var and the new value as arguments and should return either #f if
;;; the modification should be rejected, or a list containing the
;;; result otherwise.

;;; Finally, a config var has two states, "officially" modified, and
;;; unofficially modified.  You control what kind of modification
;;; you're making with the second argument to
;;; gnc:config-var-value-set!  The idea is that options specified on
;;; the command line will set the value of these config vars, but that
;;; setting is considered transient.  Other settings (like from the UI
;;; preferences panel, or normal user code) should be considered
;;; permanent, and if they leave the variable value different from the
;;; default, should be saved to ~/.gnucash/config.auto.

(define (gnc:make-config-var description
                             set-action-func
                             equality-func
                             default)
  (vector description set-action-func equality-func #f default default))

(define (gnc:config-var-description-get var) (vector-ref var 0))

(define (gnc:config-var-action-func-get var) (vector-ref var 1))

(define (gnc:config-var-equality-func-get var) (vector-ref var 2))

(define (gnc:config-var-modified? var) (vector-ref var 3))
(define (gnc:config-var-modified?-set! var value) (vector-set! var 3 value))

(define (gnc:config-var-default-value-get var) (vector-ref var 4))
(define (gnc:config-var-default-value-set! var value) (vector-set! var 4 value))

(define (gnc:config-var-value-get var) (vector-ref var 5))
(define (gnc:config-var-value-set! var is-config-mod? value)
  (let ((set-action (gnc:config-var-action-func-get var))
        (result '(value)))
    (if set-action (set! result (set-action var value)))
    (if result
        (begin
          (if is-config-mod? (gnc:config-var-modified?-set var #t))
          (vector-set! var 5 (car result))))))

(define (gnc:config-var-value-is-default? var)
  (if (not (gnc:config-var-modified? var))
      #t
      (let (equal-values? gnc:config-var-equality-func-get var)
        (equal-values? 
         (gnc:config-var-default-value-get var)
         (gnc:config-var-value-get var)))))


;;;; Preferences

(define gnc:*arg-show-usage*
  (gnc:make-config-var
   "Generate an argument summary."
   (lambda (var value) (if (boolean? value) (list value) #f))
   eq?
   #f))

(define gnc:*arg-show-help*
  (gnc:make-config-var
   "Generate an argument summary."
   (lambda (var value) (if (boolean? value) (list value) #f))
   eq?
   #f))

(define gnc:*debugging?*
  (gnc:make-config-var
   "Enable debugging code."
   (lambda (var value) (if (boolean? value) (list value) #f))
   eq?
   #f))

(define gnc:*startup-file*
  (gnc:make-config-var
   "Initial lowest level scheme startup file."
   (lambda (var value)
     ;; You can't change the startup file from here since this is the
     ;; startup file...
     #f)
   string=?
   gnc:_startup-file-default_))

(define gnc:*config-dir*
  (gnc:make-config-var
   "Configuration directory."
   (lambda (var value) (if (string? value) (list value) #f))
   string=?
   gnc:_config-dir-default_))

(define gnc:*share-dir*
  (gnc:make-config-var
   "Shared files directory."
   (lambda (var value) (if (string? value) (list value) #f))
   string=?
   gnc:_share-dir-default_))

(define gnc:*load-path*
  (gnc:make-config-var
   "A list of strings indicating the load path for (gnc:load name).
Any path element enclosed in parentheses will automatically be
expanded to that directory and all its subdirectories whenever this
variable is modified.  The symbol element default will expand to the default directory.  i.e. (gnc:config-var-value-set! gnc:*load-path* '(\"/my/dir/\" default))"
   (lambda (var value)
     (if (not (list? value))
         #f
         (let ((result (gnc:_load-path-update_ value)))
           (if (list? result)
               (list result)
               #f))))
   equal?
   (list 
    (string-append "(" (getenv "HOME") "/.gnucash/scm)")
    (string-append "(" gnc:_share-dir-default_ "/scm)"))))

(define gnc:*doc-path*
  (gnc:make-config-var
   "A list of strings indicating where to look for documentation files
Any path element enclosed in parentheses will automatically be
expanded to that directory and all its subdirectories whenever this
variable is modified.  The symbol element default will expand to the
default directory.  i.e. (gnc:config-var-value-set! gnc:*doc-path*
'(\"/my/dir/\" default))"
   (lambda (var value)
     (if (not (list? value))
         #f
         (let ((result (gnc:_doc-path-update_ value)))
           (if (list? result)
               (list result)
               #f))))
   equal?
   (list 
    (string-append "(" (getenv "HOME") "/.gnucash/doc)")
    (string-append "(" gnc:_doc-dir-default_ ")"))))


(define gnc:*prefs*
  (list
   
   (cons
    "usage"
    (cons 'boolean
          (lambda (val)
            (gnc:config-var-value-set! gnc:*arg-show-usage* #f val))))
   (cons
    "help"
    (cons 'boolean
          (lambda (val)
            (gnc:config-var-value-set! gnc:*arg-show-help* #f val))))
   (cons
    "debug"
    (cons 'boolean
          (lambda (val)
            (gnc:config-var-value-set! gnc:*debugging?* #f val))))
   
   (cons
    "startup-file"
    (cons 'string
          (lambda (val)
            (gnc:config-var-value-set! gnc:*startup-file* #f val))))
   
   (cons
    "config-dir"
    (cons 'string
          (lambda (val)
            (gnc:config-var-value-set! gnc:*config-dir* #f val))))
   
   (cons
    "share-dir"
    (cons 'string
          (lambda (val)
            (gnc:config-var-value-set! gnc:*share-dir* #f val))))
   

   (cons
    "load-path"
    (cons 'string
          (lambda (val)
            (let ((path-list
                   (call-with-input-string val (lambda (port) (read port)))))
              (if (list? path-list)
                  (gnc:config-var-value-set! gnc:*load-path* #f path-list)
                  (begin
                    (gnc:error "non-list given for --load-path: " val)
                    (gnc:shutdown 1)))))))
   
   (cons
    "doc-path"
    (cons 'string
          (lambda (val)
            (let ((path-list
                   (call-with-input-string val (lambda (port) (read port)))))
              (if (list? path-list)
                  (gnc:config-var-value-set! gnc:*doc-path* #f path-list)
                  (begin
                    (gnc:error "non-list given for --doc-path: " val)
                    (gnc:shutdown 1)))))))
   
   (cons "load-user-config" (cons 'boolean gnc:load-user-config))
   (cons "load-system-config" (cons 'boolean gnc:load-system-config))))


;; also "-c"


(define (gnc:cmd-line-get-boolean-arg args)
  ;; --arg         means #t
  ;; --arg true    means #t
  ;; --arg false   means #f

  (if (not (pair? args))
      ;; Special case of end of list
      (list #t args)
      (let ((arg (car args)))
        (if (string=? arg "false")
            (list #f (cdr args))
            (list #t 
                  (if (string=? arg "true")
                      (cdr args)
                      args))))))

(define (gnc:cmd-line-get-integer-arg args)
  (let ((arg (car args)))    
    (let ((value (string->number arg)))
      (if (not value)
          #f
          (if (not (exact? value))
              #f
              (list value (cdr args)))))))

(define (gnc:cmd-line-get-string-arg args)
  (list (car args) (cdr args)))

(define (gnc:prefs-show-usage)
  (display "usage: gnucash [ option ... ] [ datafile ]") (newline))


(define (gnc:handle-command-line-args)
  (gnc:debug "handling command line arguments")
  
  (let ((files-to-open '())
        (result #t))
    
    (do ((rest (cdr (program-arguments))) ; initial cdr skips argv[0]
         (quit? #f)
         (item #f))
        ((or quit? (null? rest)))
      
      (set! item (car rest))      
      
      (gnc:debug "handling arg " item)
 
      (if (not (string=? "--" (make-shared-substring item 0 2)))
          (begin
            (gnc:debug "non-option " item ", assuming file")
            (set! rest (cdr rest))
            (set! files-to-open (cons item files-to-open)))
          
          ;; Got something that looks like an option...
          (let* ((arg-string (make-shared-substring item 2))
                 (arg-def (assoc-ref gnc:*prefs* arg-string)))
            
            (if (not arg-def)
                (begin
                  (gnc:prefs-show-usage)
                  (set! result #f)
                  (set! quit? #t))
                
                (let* ((arg-type (car arg-def))
                       (arg-parse-result
                        (case arg-type
                          ((boolean) (gnc:cmd-line-get-boolean-arg (cdr rest)))
                          ((string) (gnc:cmd-line-get-string-arg (cdr rest)))
                          ((integer)
                           (gnc:cmd-line-get-integer-arg (cdr rest)))
                          (else
                           (gnc:error "bad argument type " arg-type ".")
                           (gnc:shutdown 1)))))

                  (if (not arg-parse-result)
                      (begin                
                        (set result #f)
                        (set! quit? #t))
                      (let ((parsed-value (car arg-parse-result))
                            (remaining-args (cadr arg-parse-result)))
                        ((cdr arg-def) parsed-value) 
                        (set! rest remaining-args))))))))
    (if result
        (gnc:debug "files to open: " files-to-open))
    
    (set! gnc:*command-line-files* files-to-open)
    
    result))

;;;; Now the fun begins.

(gnc:startup)

(if (not (gnc:handle-command-line-args))
    (gnc:shutdown 1))

;;; Now we can load a bunch of files.

(gnc:load "hooks.scm")
(gnc:load "doc.scm")

;;; Load the system and user configs

(if (not gnc:*user-config-loaded?*)
    (if (not (gnc:load-system-config))
        (gnc:shutdown 1)))

(if (not gnc:*system-config-loaded?*)
    (if (not (gnc:load-user-config))
        (gnc:shutdown 1)))

(gnc:hook-run-danglers gnc:*startup-hook*)

(if (or (gnc:config-var-value-get gnc:*arg-show-usage*)
         (gnc:config-var-value-get gnc:*arg-show-help*))
     (begin
       (gnc:prefs-show-usage)
       (gnc:shutdown 0)))

(if (not (= (gnucash_lowlev_app_init) 0))
    (gnc:shutdown 0))

(if (pair? gnc:*command-line-files*)
     ;; You can only open single files right now...
     (gnucash_ui_open_file (car gnc:*command-line-files*))
     (gnucash_ui_select_file))

(gnucash_lowlev_app_main)

(gnc:shutdown 0)
