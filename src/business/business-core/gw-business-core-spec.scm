;;; -*-scheme-*-

;(debug-enable 'backtrace)
;(debug-enable 'debug)
;(read-enable 'positions)

(debug-set! maxdepth 100000)
(debug-set! stack    2000000)

(define-module (g-wrapped gw-business-core-spec)
  :use-module (g-wrap))

(use-modules (g-wrap))
(use-modules (g-wrap simple-type))

(use-modules (g-wrap gw-standard-spec))
(use-modules (g-wrap gw-wct-spec))

(use-modules (g-wrapped gw-engine-spec))
(use-modules (g-wrap gw-wct-spec))
(use-modules (g-wrap gw-glib-spec))

(let ((ws (gw:new-wrapset "gw-business-core")))
  
  (gw:wrapset-depends-on ws "gw-standard")
  (gw:wrapset-depends-on ws "gw-wct")
  (gw:wrapset-depends-on ws "gw-glib")

  (gw:wrapset-depends-on ws "gw-engine")
  
  (gw:wrapset-set-guile-module! ws '(g-wrapped gw-business-core))
  
  (gw:wrapset-add-cs-declarations!
   ws
   (lambda (wrapset client-wrapset)
     (list
      "#include <gncAddress.h>\n"
      "#include <gncBillTerm.h>\n"
      "#include <gncCustomer.h>\n"
      "#include <gncEmployee.h>\n"
      "#include <gncEntry.h>\n"
      "#include <gncInvoice.h>\n"
      "#include <gncJob.h>\n"
      "#include <gncOrder.h>\n"
      "#include <gncOwner.h>\n"
      "#include <gncTaxTable.h>\n"
      "#include <gncVendor.h>\n"
      "#include <gncBusGuile.h>\n")))

  (gw:wrapset-add-cs-initializers!
   ws
   (lambda (wrapset client-wrapset status-var)
     (if client-wrapset
         '()
         (gw:inline-scheme '(use-modules (gnucash business-core))))))
  
  ;; The core Business Object Types

  (gw:wrap-as-wct ws '<gnc:GncAddress*> "GncAddress*" "const GncAddress*")
  (gw:wrap-as-wct ws '<gnc:GncBillTerm*> "GncBillTerm*" "const GncBillTerm*")
  (gw:wrap-as-wct ws '<gnc:GncCustomer*> "GncCustomer*" "const GncCustomer*")
  (gw:wrap-as-wct ws '<gnc:GncEmployee*> "GncEmployee*" "const GncEmployee*")
  (gw:wrap-as-wct ws '<gnc:GncEntry*> "GncEntry*" "const GncEntry*")
  (gw:wrap-as-wct ws '<gnc:GncInvoice*> "GncInvoice*" "const GncInvoice*")
  (gw:wrap-as-wct ws '<gnc:GncJob*> "GncJob*" "const GncJob*")
  (gw:wrap-as-wct ws '<gnc:GncOrder*> "GncOrder*" "const GncOrder*")
  (gw:wrap-as-wct ws '<gnc:GncOwner*> "GncOwner*" "const GncOwner*")
  (gw:wrap-as-wct ws '<gnc:GncTaxTable*> "GncTaxTable*" "const GncTaxTable*")
  (gw:wrap-as-wct ws '<gnc:GncVendor*> "GncVendor*" "const GncVendor*")

  (let ((wt (gw:wrap-enumeration ws '<gnc:GncOwnerType> "GncOwnerType")))
    (gw:enum-add-value! wt "GNC_OWNER_NONE" 'gnc-owner-none)
    (gw:enum-add-value! wt "GNC_OWNER_UNDEFINED" 'gnc-owner-undefined)
    (gw:enum-add-value! wt "GNC_OWNER_CUSTOMER" 'gnc-owner-customer)
    (gw:enum-add-value! wt "GNC_OWNER_JOB" 'gnc-owner-job)
    (gw:enum-add-value! wt "GNC_OWNER_VENDOR" 'gnc-owner-vendor)
    #t)

  (let ((wt (gw:wrap-enumeration ws '<gnc:GncAmountType> "GncAmountType")))
    (gw:enum-add-value! wt "GNC_AMT_TYPE_VALUE" 'gnc-amount-type-value)
    (gw:enum-add-value! wt "GNC_AMT_TYPE_PERCENT" 'gnc-amount-type-percent)
    #t)

  (let ((wt (gw:wrap-enumeration ws '<gnc:GncTaxIncluded> "GncTaxIncluded")))
    (gw:enum-add-value! wt "GNC_TAXINCLUDED_YES" 'gnc-tax-included-yes)
    (gw:enum-add-value! wt "GNC_TAXINCLUDED_NO" 'gnc-tax-included-no)
    (gw:enum-add-value! wt "GNC_TAXINCLUDED_USEGLOBAL" 'gnc-tax-included-useglobal)
    #t)

  (let ((wt (gw:wrap-enumeration ws '<gnc:GncBillTermType> "GncBillTermType")))
    (gw:enum-add-value! wt "GNC_TERM_TYPE_DAYS" 'gnc-term-type-days)
    (gw:enum-add-value! wt "GNC_TERM_TYPE_PROXIMO" 'gnc-term-type-proximo)
    #t)

  ;; Wrap the GncAccountValue type (gncTaxTable.h)
  (gw:wrap-simple-type
   ws
   '<gnc:GncAccountValue*> "GncAccountValue*"
   '("gnc_account_value_pointer_p(" scm-var ")")
   '(c-var " = gnc_scm_to_account_value_ptr(" scm-var ");\n")
   '(scm-var " = gnc_account_value_ptr_to_scm(" c-var ");\n"))

  ;;
  ;; Define business Query parameters
  ;;

  (gw:wrap-value ws 'gnc:invoice-from-lot '<gnc:id-type> "INVOICE_FROM_LOT")
  (gw:wrap-value ws 'gnc:invoice-from-txn '<gnc:id-type> "INVOICE_FROM_TXN")
  (gw:wrap-value ws 'gnc:invoice-owner '<gnc:id-type> "INVOICE_OWNER")

  (gw:wrap-value ws 'gnc:owner-from-lot '<gnc:id-type> "OWNER_FROM_LOT")
  (gw:wrap-value ws 'gnc:owner-parentg '<gnc:id-type> "OWNER_PARENTG")

  ;;
  ;; gncAddress.h
  ;;

  ; Set Functions

  (gw:wrap-function
   ws
   'gnc:address-set-name
   '<gw:void>
   "gncAddressSetName"
   '((<gnc:GncAddress*> address) ((<gw:mchars> callee-owned const) name))
   "Set the address name")

  (gw:wrap-function
   ws
   'gnc:address-set-addr1
   '<gw:void>
   "gncAddressSetAddr1"
   '((<gnc:GncAddress*> address) ((<gw:mchars> callee-owned const) addr1))
   "Set the address's addr1")

  (gw:wrap-function
   ws
   'gnc:address-set-addr2
   '<gw:void>
   "gncAddressSetAddr2"
   '((<gnc:GncAddress*> address) ((<gw:mchars> callee-owned const) addr2))
   "Set the address's addr2")

  (gw:wrap-function
   ws
   'gnc:address-set-addr3
   '<gw:void>
   "gncAddressSetAddr3"
   '((<gnc:GncAddress*> address) ((<gw:mchars> callee-owned const) addr3))
   "Set the address's addr3")

  (gw:wrap-function
   ws
   'gnc:address-set-addr4
   '<gw:void>
   "gncAddressSetAddr4"
   '((<gnc:GncAddress*> address) ((<gw:mchars> callee-owned const) addr4))
   "Set the address's addr4")

  ; Get Functions

  (gw:wrap-function
   ws
   'gnc:address-get-name
   '(<gw:mchars> callee-owned const)
   "gncAddressGetName"
   '((<gnc:GncAddress*> address))
   "Return the Address's Name Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-addr1
   '(<gw:mchars> callee-owned const)
   "gncAddressGetAddr1"
   '((<gnc:GncAddress*> address))
   "Return the Address's Addr1 Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-addr2
   '(<gw:mchars> callee-owned const)
   "gncAddressGetAddr2"
   '((<gnc:GncAddress*> address))
   "Return the Address's Addr2 Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-addr3
   '(<gw:mchars> callee-owned const)
   "gncAddressGetAddr3"
   '((<gnc:GncAddress*> address))
   "Return the Address's Addr3 Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-addr4
   '(<gw:mchars> callee-owned const)
   "gncAddressGetAddr4"
   '((<gnc:GncAddress*> address))
   "Return the Address's Addr4 Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-phone
   '(<gw:mchars> callee-owned const)
   "gncAddressGetPhone"
   '((<gnc:GncAddress*> address))
   "Return the Address's Phone Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-fax
   '(<gw:mchars> callee-owned const)
   "gncAddressGetFax"
   '((<gnc:GncAddress*> address))
   "Return the Address's Fax Entry")

  (gw:wrap-function
   ws
   'gnc:address-get-email
   '(<gw:mchars> callee-owned const)
   "gncAddressGetEmail"
   '((<gnc:GncAddress*> address))
   "Return the Address's Email Entry")

  ;;
  ;; gncBillTerm.h
  ;;

  (gw:wrap-function
   ws
   'gnc:bill-term-get-name
   '(<gw:mchars> callee-owned const)
   "gncBillTermGetName"
   '((<gnc:GncBillTerm*> term))
   "Return the Bill-term name")

  (gw:wrap-function
   ws
   'gnc:bill-term-get-description
   '(<gw:mchars> callee-owned const)
   "gncBillTermGetDescription"
   '((<gnc:GncBillTerm*> term))
   "Return the printable Bill-term description")

  ;;
  ;; gncCustomer.h
  ;;

  ; Set Functions

  (gw:wrap-function
   ws
   'gnc:customer-create
   '<gnc:GncCustomer*>
   "gncCustomerCreate"
   '((<gnc:Book*> book))
   "Create a new customer")

  (gw:wrap-function
   ws
   'gnc:customer-set-id
   '<gw:void>
   "gncCustomerSetID"
   '((<gnc:GncCustomer*> customer) ((<gw:mchars> callee-owned const) id))
   "Set the customer ID")

  (gw:wrap-function
   ws
   'gnc:customer-set-name
   '<gw:void>
   "gncCustomerSetName"
   '((<gnc:GncCustomer*> customer) ((<gw:mchars> callee-owned const) name))
   "Set the customer Name")

  (gw:wrap-function
   ws
   'gnc:customer-set-commodity
   '<gw:void>
   "gncCustomerSetCommodity"
   '((<gnc:GncCustomer*> customer) (<gnc:commodity*> commodity))
   "Set the Customer Commodity")

  ; Get Functions

  (gw:wrap-function
   ws
   'gnc:customer-get-guid
   '<gnc:guid-scm>
   "gncCustomerRetGUID"
   '((<gnc:GncCustomer*> customer))
   "Return the GUID of the customer")

  (gw:wrap-function
   ws
   'gnc:customer-lookup
   '<gnc:GncCustomer*>
   "gncCustomerLookupDirect"
   '((<gnc:guid-scm> guid) (<gnc:Book*> book))
   "Lookup the customer with GUID guid.")

  (gw:wrap-function
   ws
   'gnc:customer-get-id
   '(<gw:mchars> callee-owned const)
   "gncCustomerGetID"
   '((<gnc:GncCustomer*> customer))
   "Return the Customer's ID")

  (gw:wrap-function
   ws
   'gnc:customer-get-name
   '(<gw:mchars> callee-owned const)
   "gncCustomerGetName"
   '((<gnc:GncCustomer*> customer))
   "Return the Customer's Name")

  (gw:wrap-function
   ws
   'gnc:customer-get-addr
   '<gnc:GncAddress*>
   "gncCustomerGetAddr"
   '((<gnc:GncCustomer*> customer))
   "Return the Customer's Billing Address")

  (gw:wrap-function
   ws
   'gnc:customer-get-shipaddr
   '<gnc:GncAddress*>
   "gncCustomerGetShipAddr"
   '((<gnc:GncCustomer*> customer))
   "Return the Customer's Shipping Address")

  (gw:wrap-function
   ws
   'gnc:customer-get-notes
   '(<gw:mchars> callee-owned const)
   "gncCustomerGetNotes"
   '((<gnc:GncCustomer*> customer))
   "Return the Customer's Notes")

  (gw:wrap-function
   ws
   'gnc:customer-get-commodity
   '<gnc:commodity*>
   "gncCustomerGetCommodity"
   '((<gnc:GncCustomer*> customer))
   "Get the Customer Commodity")

  ;;
  ;; gncEmployee.h
  ;;

  ;;
  ;; gncEntry.h
  ;;

  (gw:wrap-function
   ws
   'gnc:entry-get-date
   '<gnc:time-pair>
   "gncEntryGetDate"
   '((<gnc:GncEntry*> entry))
   "Return the entry's date")

  (gw:wrap-function
   ws
   'gnc:entry-get-description
   '(<gw:mchars> callee-owned const)
   "gncEntryGetDescription"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Description")

  (gw:wrap-function
   ws
   'gnc:entry-get-action
   '(<gw:mchars> callee-owned const)
   "gncEntryGetAction"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Action")

  (gw:wrap-function
   ws
   'gnc:entry-get-quantity
   '<gnc:numeric>
   "gncEntryGetQuantity"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Quantity")

  (gw:wrap-function
   ws
   'gnc:entry-get-inv-price
   '<gnc:numeric>
   "gncEntryGetInvPrice"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Price")

  (gw:wrap-function
   ws
   'gnc:entry-get-inv-discount
   '<gnc:numeric>
   "gncEntryGetInvDiscount"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Discount")

  (gw:wrap-function
   ws
   'gnc:entry-get-inv-discount-type
   '<gw:int>
   "gncEntryGetInvDiscountType"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's discount type")

  (gw:wrap-function
   ws
   'gnc:entry-get-bill-price
   '<gnc:numeric>
   "gncEntryGetBillPrice"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Price")

  (gw:wrap-function
   ws
   'gnc:entry-get-inv-taxable
   '<gw:bool>
   "gncEntryGetInvTaxable"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Taxable value")

  (gw:wrap-function
   ws
   'gnc:entry-get-bill-taxable
   '<gw:bool>
   "gncEntryGetBillTaxable"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Taxable value")

  (gw:wrap-function
   ws
   'gnc:entry-get-value
   '<gnc:numeric>
   "gncEntryReturnValue"
   '((<gnc:GncEntry*> entry) (<gw:bool> invoice?))
   "Return the Entry's computed Value (after discount)")

  (gw:wrap-function
   ws
   'gnc:entry-get-tax-value
   '<gnc:numeric>
   "gncEntryReturnTaxValue"
   '((<gnc:GncEntry*> entry) (<gw:bool> invoice?))
   "Return the Entry's computed Tax Value")

  (gw:wrap-function
   ws
   'gnc:entry-get-discount-value
   '<gnc:numeric>
   "gncEntryReturnDiscountValue"
   '((<gnc:GncEntry*> entry) (<gw:bool> invoice?))
   "Return the Entry's computed Discount Value")

  (gw:wrap-function
   ws
   'gnc:entry-get-tax-values
   '(gw:glist-of <gnc:GncAccountValue*> callee-owned)
   "gncEntryReturnTaxValues"
   '((<gnc:GncEntry*> entry) (<gw:bool> invoice?))
   "Return the Entry's list of computed Tax Values for each Tax account")

  (gw:wrap-function
   ws
   'gnc:entry-get-invoice
   '<gnc:GncInvoice*>
   "gncEntryGetInvoice"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Invoice")

  (gw:wrap-function
   ws
   'gnc:entry-get-bill
   '<gnc:GncInvoice*>
   "gncEntryGetBill"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Bill")

  (gw:wrap-function
   ws
   'gnc:entry-get-order
   '<gnc:GncOrder*>
   "gncEntryGetOrder"
   '((<gnc:GncEntry*> entry))
   "Return the Entry's Order")

  ;;
  ;; gncInvoice.h
  ;;

  ; Set Functions

  (gw:wrap-function
   ws
   'gnc:invoice-get-guid
   '<gnc:guid-scm>
   "gncInvoiceRetGUID"
   '((<gnc:GncInvoice*> invoice))
   "Return the GUID of the invoice")

  (gw:wrap-function
   ws
   'gnc:invoice-lookup
   '<gnc:GncInvoice*>
   "gncInvoiceLookupDirect"
   '((<gnc:guid-scm> guid) (<gnc:Book*> book))
   "Lookup the invoice with GUID guid.")

  (gw:wrap-function
   ws
   'gnc:invoice-create
   '<gnc:GncInvoice*>
   "gncInvoiceCreate"
   '((<gnc:Book*> book))
   "Create a new invoice")

  (gw:wrap-function
   ws
   'gnc:invoice-set-id
   '<gw:void>
   "gncInvoiceSetID"
   '((<gnc:GncInvoice*> invoice) ((<gw:mchars> callee-owned const) id))
   "Set the Invoice ID")

  (gw:wrap-function
   ws
   'gnc:invoice-set-owner
   '<gw:void>
   "gncInvoiceSetOwner"
   '((<gnc:GncInvoice*> invoice) (<gnc:GncOwner*> owner))
   "Set the Invoice Owner")

  (gw:wrap-function
   ws
   'gnc:invoice-set-date-opened
   '<gw:void>
   "gncInvoiceSetDateOpened"
   '((<gnc:GncInvoice*> invoice) (<gnc:time-pair> date))
   "Set the Invoice-Opened Date")

  (gw:wrap-function
   ws
   'gnc:invoice-set-terms
   '<gw:void>
   "gncInvoiceSetTerms"
   '((<gnc:GncInvoice*> invoice) (<gnc:GncBillTerm*> term))
   "Set the Invoice Terms")

  (gw:wrap-function
   ws
   'gnc:invoice-set-common-commodity
   '<gw:void>
   "gncInvoiceSetCommonCommodity"
   '((<gnc:GncInvoice*> invoice) (<gnc:commodity*> commodity))
   "Set the Invoice Commodity")

  ; Get Functions

  (gw:wrap-function
   ws
   'gnc:invoice-get-book
   '<gnc:Book*>
   "gncInvoiceGetBook"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's book")

  (gw:wrap-function
   ws
   'gnc:invoice-get-id
   '(<gw:mchars> callee-owned const)
   "gncInvoiceGetID"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's ID")

  (gw:wrap-function
   ws
   'gnc:invoice-get-owner
   '<gnc:GncOwner*>
   "gncInvoiceGetOwner"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's Owner")

  (gw:wrap-function
   ws
   'gnc:invoice-get-date-opened
   '<gnc:time-pair>
   "gncInvoiceGetDateOpened"
   '((<gnc:GncInvoice*> invoice))
   "Return the Date the invoice was opened")

  (gw:wrap-function
   ws
   'gnc:invoice-get-date-posted
   '<gnc:time-pair>
   "gncInvoiceGetDatePosted"
   '((<gnc:GncInvoice*> invoice))
   "Return the Date the invoice was posted")

  (gw:wrap-function
   ws
   'gnc:invoice-get-date-due
   '<gnc:time-pair>
   "gncInvoiceGetDateDue"
   '((<gnc:GncInvoice*> invoice))
   "Return the Date the invoice is due")

  (gw:wrap-function
   ws
   'gnc:invoice-get-terms
   '(<gnc:GncBillTerm*>)
   "gncInvoiceGetTerms"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's Terms")

  (gw:wrap-function
   ws
   'gnc:invoice-get-billing-id
   '(<gw:mchars> callee-owned const)
   "gncInvoiceGetBillingID"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's Billing ID")

  (gw:wrap-function
   ws
   'gnc:invoice-get-notes
   '(<gw:mchars> callee-owned const)
   "gncInvoiceGetNotes"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's Notes")

  (gw:wrap-function
   ws
   'gnc:invoice-get-common-commodity
   '<gnc:commodity*>
   "gncInvoiceGetCommonCommodity"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's commodity")

  (gw:wrap-function
   ws
   'gnc:invoice-get-entries
   '(gw:glist-of <gnc:GncEntry*> callee-owned)
   "gncInvoiceGetEntries"
   '((<gnc:GncInvoice*> invoice))
   "Return the invoice's list of Entries")

  (gw:wrap-function
   ws
   'gnc:invoice-get-posted-lot
   '<gnc:Lot*>
   "gncInvoiceGetPostedLot"
   '((<gnc:GncInvoice*> invoice))
   "Return the posted Lot for this Invoice.")

  (gw:wrap-function
   ws
   'gnc:invoice-get-posted-account
   '<gnc:Account*>
   "gncInvoiceGetPostedAcc"
   '((<gnc:GncInvoice*> invoice))
   "Return the posted Account for this Invoice.")

  (gw:wrap-function
   ws
   'gnc:invoice-get-posted-txn
   '<gnc:Transaction*>
   "gncInvoiceGetPostedTxn"
   '((<gnc:GncInvoice*> invoice))
   "Return the posted Transaction for this Invoice.")

  (gw:wrap-function
   ws
   'gnc:invoice-get-invoice-from-lot
   '<gnc:GncInvoice*>
   "gncInvoiceGetInvoiceFromLot"
   '((<gnc:Lot*> lot))
   "Return the Invoice attached to a Lot.")

  (gw:wrap-function
   ws
   'gnc:invoice-get-invoice-from-txn
   '<gnc:GncInvoice*>
   "gncInvoiceGetInvoiceFromTxn"
   '((<gnc:Transaction*> txn))
   "Return the Invoice attached to a Txn.")

  ;;
  ;; gncJob.h
  ;;

  ; Set Functions

  (gw:wrap-function
   ws
   'gnc:job-create
   '<gnc:GncJob*>
   "gncJobCreate"
   '((<gnc:Book*> book))
   "Create a new Job")

  (gw:wrap-function
   ws
   'gnc:job-set-id
   '<gw:void>
   "gncJobSetID"
   '((<gnc:GncJob*> job) ((<gw:mchars> callee-owned const) id))
   "Set the job ID")

  (gw:wrap-function
   ws
   'gnc:job-set-name
   '<gw:void>
   "gncJobSetName"
   '((<gnc:GncJob*> job) ((<gw:mchars> callee-owned const) name))
   "Set the job Name")

  (gw:wrap-function
   ws
   'gnc:job-set-reference
   '<gw:void>
   "gncJobSetReference"
   '((<gnc:GncJob*> job) ((<gw:mchars> callee-owned const) reference))
   "Set the job Reference")

  (gw:wrap-function
   ws
   'gnc:job-set-owner
   '<gw:void>
   "gncJobSetOwner"
   '((<gnc:GncJob*> job) (<gnc:GncOwner*> owner))
   "Set the job Owner")

  ; Get Functions

  (gw:wrap-function
   ws
   'gnc:job-get-id
   '(<gw:mchars> callee-owned const)
   "gncJobGetID"
   '((<gnc:GncJob*> job))
   "Return the Job's ID")

  (gw:wrap-function
   ws
   'gnc:job-get-name
   '(<gw:mchars> callee-owned const)
   "gncJobGetName"
   '((<gnc:GncJob*> job))
   "Return the Job's Name")

  (gw:wrap-function
   ws
   'gnc:job-get-reference
   '(<gw:mchars> callee-owned const)
   "gncJobGetReference"
   '((<gnc:GncJob*> job))
   "Return the Job's Reference")

  (gw:wrap-function
   ws
   'gnc:job-get-owner
   '<gnc:GncOwner*>
   "gncJobGetOwner"
   '((<gnc:GncJob*> job))
   "Return the Job's Owner")

  (gw:wrap-function
   ws
   'gnc:job-get-guid
   '<gnc:guid-scm>
   "gncJobRetGUID"
   '((<gnc:GncJob*> job))
   "Return the guid of the job")

  (gw:wrap-function
   ws
   'gnc:job-lookup
   '<gnc:GncJob*>
   "gncJobLookupDirect"
   '((<gnc:guid-scm> guid) (<gnc:Book*> book))
   "Lookup the job with GUID guid.")

  ;;
  ;; gncOrder.h
  ;;

  ; Set Functions

  (gw:wrap-function
   ws
   'gnc:order-create
   '<gnc:GncOrder*>
   "gncOrderCreate"
   '((<gnc:Book*> book))
   "Create a new order")

  (gw:wrap-function
   ws
   'gnc:order-set-id
   '<gw:void>
   "gncOrderSetID"
   '((<gnc:GncOrder*> order) ((<gw:mchars> callee-owned const) id))
   "Set the Order ID")

  (gw:wrap-function
   ws
   'gnc:order-set-owner
   '<gw:void>
   "gncOrderSetOwner"
   '((<gnc:GncOrder*> order) (<gnc:GncOwner*> owner))
   "Set the Order Owner")

  (gw:wrap-function
   ws
   'gnc:order-set-date-opened
   '<gw:void>
   "gncOrderSetDateOpened"
   '((<gnc:GncOrder*> order) (<gnc:time-pair> date))
   "Set the Order's Opened Date")

  (gw:wrap-function
   ws
   'gnc:order-set-reference
   '<gw:void>
   "gncOrderSetReference"
   '((<gnc:GncOrder*> order) ((<gw:mchars> callee-owned const) id))
   "Set the Order Reference")

  ; Get Functions

  (gw:wrap-function
   ws
   'gnc:order-get-id
   '(<gw:mchars> callee-owned const)
   "gncOrderGetID"
   '((<gnc:GncOrder*> order))
   "Return the Order's ID")

  (gw:wrap-function
   ws
   'gnc:order-get-owner
   '<gnc:GncOwner*>
   "gncOrderGetOwner"
   '((<gnc:GncOrder*> order))
   "Return the Order's Owner")

  (gw:wrap-function
   ws
   'gnc:order-get-date-opened
   '<gnc:time-pair>
   "gncOrderGetDateOpened"
   '((<gnc:GncOrder*> order))
   "Return the Date the order was opened")

  (gw:wrap-function
   ws
   'gnc:order-get-date-closed
   '<gnc:time-pair>
   "gncOrderGetDateClosed"
   '((<gnc:GncOrder*> order))
   "Return the Date the order was closed")

  (gw:wrap-function
   ws
   'gnc:order-get-notes
   '(<gw:mchars> callee-owned const)
   "gncOrderGetNotes"
   '((<gnc:GncOrder*> order))
   "Return the Order's Notes")

  (gw:wrap-function
   ws
   'gnc:order-get-reference
   '(<gw:mchars> callee-owned const)
   "gncOrderGetReference"
   '((<gnc:GncOrder*> order))
   "Return the Order's Reference")

  (gw:wrap-function
   ws
   'gnc:order-get-entries
   '(gw:glist-of <gnc:GncEntry*> callee-owned)
   "gncOrderGetEntries"
   '((<gnc:GncOrder*> order))
   "Return the Order's list of Entries")

  ;;
  ;; gncOwner.h
  ;;

  (gw:wrap-function
   ws
   'gnc:owner-create
   '<gnc:GncOwner*>
   "gncOwnerCreate"
   '()
   "Create a GncOwner object")

  (gw:wrap-function
   ws
   'gnc:owner-destroy
   '<gw:void>
   "gncOwnerDestroy"
   '((<gnc:GncOwner*> owner))
   "Destroy a GncOwner object")

  (gw:wrap-function
   ws
   'gnc:owner-init-customer
   '<gw:void>
   "gncOwnerInitCustomer"
   '((<gnc:GncOwner*> owner) (<gnc:GncCustomer*> customer))
   "Initialize an owner to hold a Customer.  The Customer may be NULL.")

  (gw:wrap-function
   ws
   'gnc:owner-init-job
   '<gw:void>
   "gncOwnerInitJob"
   '((<gnc:GncOwner*> owner) (<gnc:GncJob*> job))
   "Initialize an owner to hold a Job.  The Job may be NULL.")

  (gw:wrap-function
   ws
   'gnc:owner-init-vendor
   '<gw:void>
   "gncOwnerInitVendor"
   '((<gnc:GncOwner*> owner) (<gnc:GncVendor*> vendor))
   "Initialize an owner to hold a Vendor.  The Vendor may be NULL.")

  (gw:wrap-function
   ws
   'gnc:owner-get-type
   '<gnc:GncOwnerType>
   "gncOwnerGetType"
   '((<gnc:GncOwner*> owner))
   "Return the type of this owner.")

  (gw:wrap-function
   ws
   'gnc:owner-get-customer
   '<gnc:GncCustomer*>
   "gncOwnerGetCustomer"
   '((<gnc:GncOwner*> owner))
   "Return the customer of this owner.")

  (gw:wrap-function
   ws
   'gnc:owner-get-job
   '<gnc:GncJob*>
   "gncOwnerGetJob"
   '((<gnc:GncOwner*> owner))
   "Return the job of this owner.")

  (gw:wrap-function
   ws
   'gnc:owner-get-vendor
   '<gnc:GncVendor*>
   "gncOwnerGetVendor"
   '((<gnc:GncOwner*> owner))
   "Return the vendor of this owner.")

  (gw:wrap-function
   ws
   'gnc:owner-equal
   '<gw:bool>
   "gncOwnerEqual"
   '((<gnc:GncOwner*> owner1) (<gnc:GncOwner*> owner2))
   "Compare owner1 and owner2 and return if they are equal")

  (gw:wrap-function
   ws
   'gnc:owner-get-end-owner
   '<gnc:GncOwner*>
   "gncOwnerGetEndOwner"
   '((<gnc:GncOwner*> owner))
   "Returns the End Owner of this owner")

  (gw:wrap-function
   ws
   'gnc:owner-get-owner-from-lot
   '<gw:bool>
   "gncOwnerGetOwnerFromLot"
   '((<gnc:Lot*> lot) (<gnc:GncOwner*> owner))
   "Compute the owner from the Lot, and fills in owner.  Returns TRUE if successful.")

  (gw:wrap-function
   ws
   'gnc:owner-get-guid
   '<gnc:guid-scm>
   "gncOwnerRetGUID"
   '((<gnc:GncOwner*> owner))
   "Return the GUID of this owner")

  (gw:wrap-function
   ws
   'gnc:owner-copy-into-owner
   '<gw:void>
   "gncOwnerCopy"
   '((<gnc:GncOwner*> src-owner) (<gnc:GncOwner*> dest-owner))
   "Copy the src-owner to the dest-owner")

  (gw:wrap-function
   ws
   'gnc:owner-is-valid?
   '<gw:bool>
   "gncOwnerIsValid"
   '((<gnc:GncOwner*> owner))
   "Is this a real owner?  Return TRUE IFF there is an actual owner.")

  ;;
  ;; gncVendor.h
  ;;

  (gw:wrap-function
   ws
   'gnc:vendor-get-guid
   '<gnc:guid-scm>
   "gncVendorRetGUID"
   '((<gnc:GncVendor*> vendor))
   "Return the GUID of the vendor")

  (gw:wrap-function
   ws
   'gnc:vendor-lookup
   '<gnc:GncVendor*>
   "gncVendorLookupDirect"
   '((<gnc:guid-scm> guid) (<gnc:Book*> book))
   "Lookup the vendor with GUID guid.")

  (gw:wrap-function
   ws
   'gnc:vendor-get-id
   '(<gw:mchars> callee-owned const)
   "gncVendorGetID"
   '((<gnc:GncVendor*> vendor))
   "Return the Vendor's ID")

  (gw:wrap-function
   ws
   'gnc:vendor-get-name
   '(<gw:mchars> callee-owned const)
   "gncVendorGetName"
   '((<gnc:GncVendor*> vendor))
   "Return the Vendor's Name")

  (gw:wrap-function
   ws
   'gnc:vendor-get-addr
   '<gnc:GncAddress*>
   "gncVendorGetAddr"
   '((<gnc:GncVendor*> vendor))
   "Return the Vendor's Billing Address")

  (gw:wrap-function
   ws
   'gnc:vendor-get-notes
   '(<gw:mchars> callee-owned const)
   "gncVendorGetNotes"
   '((<gnc:GncVendor*> vendor))
   "Return the Vendor's Notes")

)
