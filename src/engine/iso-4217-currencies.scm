;; currency descriptions for ISO4217 currencies. 
;; Format is:
;; ( (N_ fullname) unitname partname namespace mnemonic exchange-code 
;;  parts-per-unit smallest-fraction)
;;
;; Note: The first string must be enclosed by the (N_ ... ) function
;; so that this string will appear in the gnucash.pot translation
;; template.
;;
;; This file is not currently used at runtime.  It's used to generate
;; the contents of iso-4217-currencies.c.
;;
;; You can find Currency Information from the following sites: 
;;   http://www.evertype.com/standards/iso4217/iso4217-en.html
;;   http://www.xe.com/iso4217.htm
;;   http://www.thefinancials.com/vortex/CurrencyFormats.html
;;
;; Learned from some bugs (543061, 564450), please keep in mind: 
;; If there are no coins for subunits, subunits might still be in use on the paper

( (N_ "Afghanistan Afghani (old)") "afghani" "pul" "ISO4217" "AFA" "004" 100 100 ) ;; through 2003-01-02
( (N_ "Afghanistan Afghani") "afghani" "afghani" "ISO4217" "AFN" "971" 1 1 ) ;; from 2002-10-07
( (N_ "Albanian Lek") "lek" "qindarka" "ISO4217" "ALL" "008" 100 100 )
( (N_ "Algerian Dinar") "dinar" "centime"  "ISO4217" "DZD" "012" 100 100 )
( (N_ "Andorran Franc") "franc" "centime" "ISO4217" "ADF" "950" 100 100 ) ;; 1/1 French Franc
( (N_ "Andorran Peseta") "peseta" "centimo"  "ISO4217" "ADP" "724" 100 100 ) ;; 1/1 Spanish Peseta
( (N_ "Angolan New Kwanza") "new kwanza" "lwei" "ISO4217" "AON" "024" 100 100 )
( (N_ "Argentine Austral") "austral" "centavo" "ISO4217" "ARA" "XXX" 100 100 ) ;; replaced 1991
( (N_ "Argentine Peso") "peso" "centavo"  "ISO4217" "ARS" "032" 100 100 )
( (N_ "Armenian Dram") "dram" "Luma" "ISO4217" "AMD" "051" 100 100 )
( (N_ "Aruban Florin") "florin" "cent" "ISO4217" "AWG" "533" 100 100 )
( (N_ "Australian Dollar") "dollar" "cent" "ISO4217" "AUD" "036" 100 100 )
( (N_ "Austrian Shilling") "shilling" "groschen"  "ISO4217" "ATS" "040" 100 100 ) ;; through 1998
( (N_ "Azerbaijani Manat") "manat" "gopik" "ISO4217" "AZM" "031" 100 100 )
( (N_ "Azerbaijani Manat") "manat" "gopik" "ISO4217" "AZN" "944" 100 100 )
( (N_ "Bahamian Dollar") "dollar" "cent"  "ISO4217" "BSD" "044" 100 100 )
( (N_ "Bahraini Dinar") "dinar" "fil"  "ISO4217" "BHD" "048" 1000 1000 )
( (N_ "Bangladeshi Taka") "taka" "paisa"  "ISO4217" "BDT" "050" 100 100 )
( (N_ "Barbados Dollar") "dollar" "cent"  "ISO4217" "BBD" "052" 100 100 )
( (N_ "Belarussian Ruble (old)") "ruble" "ruble" "ISO4217" "BYB" "974" 1 1 ) ;; through 1999
( (N_ "Belarussian Ruble") "ruble" "ruble" "ISO4217" "BYR" "974" 1 1 ) ;; 2000 on
( (N_ "Belgian Franc") "franc" "centime" "ISO4217" "BEF" "056" 100 100 ) ;; through 1998
( (N_ "Belize Dollar") "dollar" "cent" "ISO4217" "BZD" "084" 100 100 )
( (N_ "Bermudian Dollar") "dollar" "cent" "ISO4217" "BMD" "060" 100 100 )
( (N_ "Bhutan Ngultrum") "ngultrum" "chetrum" "ISO4217" "BTN" "064" 100 100 )
( (N_ "Bolivian Boliviano") "boliviano"  "centavo" "ISO4217" "BOB" "068" 100 100 )
( (N_ "Bosnian B.H. Dinar") "B.H. dinar" "para" "ISO4217" "BAD" "070" 100 100 ) ;; into 1999
( (N_ "Bosnian Convertible Mark") "B.H. mark" "fennig" "ISO4217" "BAM" "977" 100 100 ) ;; 1999 on
( (N_ "Botswana Pula") "pula" "thebe" "ISO4217" "BWP" "072" 100 100 )
( (N_ "Brazilian Cruzeiro (old)") "cruzeiro" "centavo" "ISO4217" "BRE" "076" 100 100 ) ;; pre 1993
( (N_ "Brazilian Cruzeiro (new)") "cruzeiro" "centavo" "ISO4217" "BRR" "076" 100 100 ) ;; 1993-1994
( (N_ "Brazilian Real") "real" "centavo"  "ISO4217" "BRL" "986" 100 100 ) ;; 1994 on
( (N_ "British Pound") "pound" "pence" "ISO4217" "GBP" "826" 100 100 )
( (N_ "Brunei Dollar") "dollar" "cent" "ISO4217" "BND" "096" 100 100 )
( (N_ "Bulgarian Lev") "lev" "stotinki" "ISO4217" "BGL" "100" 100 100 ) ;; replaced 1999
( (N_ "Bulgarian Lev") "lev" "stotinki" "ISO4217" "BGN" "975" 100 100 )
( (N_ "Burundi Franc") "franc" "centime" "ISO4217" "BIF" "108" 1 1 )
( (N_ "CFA Franc BEAC") "franc" "centime" "ISO4217" "XAF" "950" 1 1 )
( (N_ "CFA Franc BCEAO") "franc" "centime" "ISO4217" "XOF" "952" 1 1 )
( (N_ "CFP Franc Pacifique") "franc" "centime" "ISO4217" "XPF" "953" 1 1 )
( (N_ "Cambodia Riel") "riel" "sen" "ISO4217" "KHR" "116" 100 100 )
( (N_ "Canadian Dollar") "dollar" "cent" "ISO4217" "CAD" "124" 100 100 )
( (N_ "Cape Verde Escudo") "escudo" "centavo" "ISO4217" "CVE" "132" 100 100 )
( (N_ "Cayman Islands Dollar") "dollar" "cent"  "ISO4217" "KYD" "136" 100 100 )
( (N_ "Chilean Peso") "peso" "centavo" "ISO4217" "CLP" "152" 1 1 )
( (N_ "Chinese Yuan Renminbi") "renminbi" "fen" "ISO4217" "CNY" "156" 100 100 )
( (N_ "Colombian Peso") "peso" "centavo" "ISO4217" "COP" "170" 100 100 )
;; ( (N_ "Colombian Unidad de Valor Real") "???" "???" "ISO4217" "COU" "970" 100 100)
( (N_ "Comoros Franc") "franc" "centime" "ISO4217" "KMF" "174" 1 1  )
( (N_ "Costa Rican Colon") "colon" "centimo" "ISO4217" "CRC" "188" 100 100 )
( (N_ "Croatian Kuna") "kuna" "lipa" "ISO4217" "HRK" "191" 100 100 )
( (N_ "Cuban Peso") "peso" "centavo" "ISO4217" "CUP" "192" 100 100 )
( (N_ "Cuban Convertible Peso") "peso" "centavo" "ISO4217" "CUC" "XXX" 100 100 )
( (N_ "Cyprus Pound") "pound" "pence"  "ISO4217" "CYP" "196" 100 100 )
( (N_ "Czech Koruna") "koruna" "haleru" "ISO4217" "CZK" "203" 100 100  )
( (N_ "Danish Krone") "krone" "ore" "ISO4217" "DKK" "208" 100 100 )
( (N_ "Djibouti Franc") "franc" "centime" "ISO4217" "DJF" "262" 1 1 )
( (N_ "Dominican Peso") "peso" "centavo"  "ISO4217" "DOP" "214" 100 100 )
( (N_ "Dutch Guilder") "guilder" "cent" "ISO4217" "NLG" "528" 100 100 )
( (N_ "East Caribbean Dollar") "dollar" "cent" "ISO4217" "XCD" "951" 100 100 )
( (N_ "Ecuador Sucre") "sucre" "centavo" "ISO4217" "ECS" "218" 100 100 ) ;; through 2000-09-15
( (N_ "Egyptian Pound") "pound" "pence"  "ISO4217" "EGP" "818" 100 100 )
( (N_ "El Salvador Colon") "colon" "centavo" "ISO4217" "SVC" "222" 100 100 )
( (N_ "Eritrean Nakfa") "nakfa" "cent" "ISO4217" "ERN" "232" 100 100 )
( (N_ "Estonian Kroon") "kroon" "senti" "ISO4217" "EEK" "233" 100 100 )
( (N_ "Ethiopian Birr") "birr" "cent" "ISO4217" "ETB" "231" 100 100 )
( (N_ "Euro") "euro" "euro-cent" "ISO4217" "EUR" "978" 100 100)
( (N_ "Falkland Islands Pound") "pound" "pence" "ISO4217" "FKP" "238" 100 100 )
( (N_ "Fiji Dollar") "dollar" "cent" "ISO4217" "FJD" "242" 100 100 )
( (N_ "Finnish Markka") "markka" "penni"  "ISO4217" "FIM" "246" 100 100) ;; through 1998
( (N_ "French Franc") "franc" "centime" "ISO4217" "FRF" "250" 100 100 ) ;; though 1998
( (N_ "Gambian Dalasi") "dalasi" "butut" "ISO4217" "GMD" "270" 100 100 )
( (N_ "Georgian Lari") "lari" "tetri" "ISO4217" "GEL" "268" 100 100 )
( (N_ "German Mark") "deutschemark" "pfennig" "ISO4217" "DEM" "280" 100 100 ) ;; through 1998
( (N_ "Ghanaian Cedi") "cedi" "psewa" "ISO4217" "GHC" "288" 100 100 )
( (N_ "Gibraltar Pound") "pound" "pence"  "ISO4217" "GIP" "292" 100 100 )
( (N_ "Greek Drachma") "drachma" "lepta" "ISO4217" "GRD" "200" 100 100 ) ;; through 2000
( (N_ "Guatemalan Quetzal") "quetzal" "centavo" "ISO4217" "GTQ" "320" 100 100 )
( (N_ "Guinea Franc") "franc" "centime" "ISO4217" "GNF" "324" 100 100 ) ;; through 1997-04
( (N_ "Guinea-Bissau Peso") "peso" "centavo" "ISO4217" "GWP" "624" 100 100)
( (N_ "Guyanan Dollar") "dollar" "cent" "ISO4217" "GYD" "328" 100 100 )
( (N_ "Haitian Gourde") "gourde" "centime"  "ISO4217" "HTG" "332" 100 100 )
( (N_ "Honduran Lempira") "lempira" "centavo"  "ISO4217" "HNL" "340" 100 100 )
( (N_ "Hong Kong Dollar") "dollar" "cent"  "ISO4217" "HKD" "344" 100 100 )
( (N_ "Hungarian Forint") "forint" "forint" "ISO4217" "HUF" "348" 100 100)
( (N_ "Iceland Krona") "krona" "aur" "ISO4217" "ISK" "352" 100 100 )
( (N_ "Indian Rupee") "rupee" "paise" "ISO4217" "INR" "356" 100 100 )
( (N_ "Indonesian Rupiah") "rupiah" "sen" "ISO4217" "IDR" "360" 100 100 )
( (N_ "Iranian Rial") "rial" "rial" "ISO4217" "IRR" "364" 1 1)
( (N_ "Iraqi Dinar") "dinar" "fil"  "ISO4217" "IQD" "368" 1000 1000)
( (N_ "Irish Punt") "punt" "pingin" "ISO4217" "IEP" "372" 100 100 ) ;; through 1998
( (N_ "Israeli New Shekel") "new shekel" "agorot"  "ISO4217" "ILS" "376" 100 100)
( (N_ "Italian Lira") "lira" "lira" "ISO4217" "ITL" "380" 1 1 ) ;; through 1998
( (N_ "Jamaican Dollar") "dollar" "cent" "ISO4217" "JMD" "388" 100 100 )
( (N_ "Japanese Yen") "yen" "sen"  "ISO4217" "JPY" "392" 100 1 )
( (N_ "Jordanian Dinar") "dinar" "fil"  "ISO4217" "JOD" "400" 1000 1000 )
( (N_ "Kazakhstan Tenge") "tenge" "tiyn" "ISO4217" "KZT" "398" 100 100 )
( (N_ "Kenyan Shilling") "shilling" "cent" "ISO4217" "KES" "404" 100 100 )
( (N_ "Kuwaiti Dinar") "dinar" "fils"  "ISO4217" "KWD" "414" 1000 1000 )
( (N_ "Kyrgyzstan Som") "som" "tyyn" "ISO4217" "KGS" "417" 100 100 )
( (N_ "Laos Kip") "kip" "at" "ISO4217" "LAK" "418" 100 100 )
( (N_ "Latvian Lats") "lats" "santim" "ISO4217" "LVL" "428" 100 100 )
( (N_ "Lebanese Pound") "pound" "pence"  "ISO4217" "LBP" "422" 100 100 )
( (N_ "Lesotho Loti") "loti" "lisente" "ISO4217" "LSL" "426" 100 100 )
( (N_ "Liberian Dollar") "dollar" "cent" "ISO4217" "LRD" "430" 100 100 )
( (N_ "Libyan Dinar") "dinar" "dirham" "ISO4217" "LYD" "434" 1000 1000 )
( (N_ "Lithuanian Litas") "litas" "centu" "ISO4217" "LTL" "440" 100 100 )
( (N_ "Luxembourg Franc") "franc" "centime" "ISO4217" "LUF" "442" 100 100  ) ;; through 1998
( (N_ "Macau Pataca") "pataca" "avo"  "ISO4217" "MOP" "446" 100 100 )
( (N_ "Macedonian Denar") "denar" "deni" "ISO4217" "MKD" "807" 100 100 )
( (N_ "Malagasy Ariary") "ariary" "iraimbilanja" "ISO4217" "MGA" "969" 200 200 )
( (N_ "Malagasy Franc") "franc" "centime" "ISO4217" "MGF" "450" 500 500 ) ;; replaced 2003-07-31
( (N_ "Malawi Kwacha") "kwacha" "tambala"  "ISO4217" "MWK" "454" 100 100 )
( (N_ "Malaysian Ringgit") "ringgit" "sen"  "ISO4217" "MYR" "458" 100 100 )
( (N_ "Maldive Rufiyaa") "rufiyaa" "lari" "ISO4217" "MVR" "462" 100 100 )
( (N_ "Mali Republic Franc") "franc" "centime" "ISO4217" "MLF" "466" 100 100 ) ;; ???
( (N_ "Maltese Lira") "lira" "cent"  "ISO4217" "MTL" "470" 100 100 )
( (N_ "Mauritanian Ouguiya") "ouguiya" "khoum"  "ISO4217" "MRO" "478" 5 5)
( (N_ "Mauritius Rupee") "rupee" "cent"  "ISO4217" "MUR" "480" 100 100 )
( (N_ "Mexican Peso") "peso" "centavo" "ISO4217" "MXN" "484" 100 100 ) ;;sicne Jan 1993
( (N_ "Moldovan Leu") "leu" "ban" "ISO4217" "MDL" "498" 100 100 )
( (N_ "Mongolian Tugrik") "tugrik" "mongo" "ISO4217" "MNT" "496" 100 100 )
( (N_ "Moroccan Dirham") "dirham" "centime"  "ISO4217" "MAD" "504" 100 100 )
( (N_ "Mozambique Metical") "metical" "centavo" "ISO4217" "MZM" "508" 100 100 )
( (N_ "Mozambique Metical") "metical" "centavo" "ISO4217" "MZN" "943" 100 100 ) ;; 2006-07-01; <http://en.wikipedia.org/wiki/Mozambican_metical>
( (N_ "Myanmar Kyat") "kyat" "pya" "ISO4217" "MMK" "104" 100 100 )
( (N_ "Namibian Dollar") "dollar" "cent" "ISO4217" "NAD" "516" 100 100 )
( (N_ "Nepalese Rupee") "rupee" "paise" "ISO4217" "NPR" "524" 100 100 )
( (N_ "Netherlands Antillian Guilder") "guilder" "cent" "ISO4217" "ANG" "532" 100 100 ) ;; through 1998
( (N_ "New Zealand Dollar") "dollar" "cent" "ISO4217" "NZD" "554" 100 100)
( (N_ "Nicaraguan Cordoba Oro") "cordoba" "centavo" "ISO4217" "NIC" "558" 100 100 )
( (N_ "Nigerian Naira") "naira" "kobo"  "ISO4217" "NGN" "566" 100 100)
( (N_ "North Korean Won") "won" "chon" "ISO4217" "KPW" "408" 100 100 )
( (N_ "Norwegian Kroner") "kroner" "ore"  "ISO4217" "NOK" "578" 100 100 )
( (N_ "Omani Rial") "rial" "baiza" "ISO4217" "OMR" "512" 1000 1000 )
( (N_ "Pakistan Rupee") "rupee" "paiza" "ISO4217" "PKR" "586" 100 100 )
( (N_ "Panamanian Balboa") "balboa" "centisimo" "ISO4217" "PAB" "590" 100 100 )
( (N_ "Papua New Guinea Kina") "kina" "toea" "ISO4217" "PGK" "598" 100 100 )
( (N_ "Paraguay Guarani") "guarani" "centimo" "ISO4217" "PYG" "600" 100 100 )
( (N_ "Peruvian Nuevo Sol") "nuevo sol" "centimo"  "ISO4217" "PEN" "604" 100 100 )
( (N_ "Philippine Peso") "peso" "centavo" "ISO4217" "PHP" "608" 100 100 )
( (N_ "Polish Zloty") "zloty" "groszy" "ISO4217" "PLN" "985" 100 100 )
( (N_ "Portuguese Escudo") "escudo" "centavo" "ISO4217" "PTE" "620" 100 100 ) ;; through 1998
( (N_ "Qatari Rial") "rial" "dirham" "ISO4217" "QAR" "634" 100 100 )
( (N_ "Romanian Leu") "leu" "bani"  "ISO4217" "ROL" "642" 100 100 ) ;; through 2005-06
( (N_ "Romanian Leu") "leu" "bani"  "ISO4217" "RON" "946" 100 100 ) ;; from 2005-07
( (N_ "Russian Rouble") "rouble" "kopek" "ISO4217" "RUB" "643" 100 100 ) ;; RUR through 1997-12, RUB from 1998-01 onwards; see bug #393185
( (N_ "Rwanda Franc") "franc" "centime" "ISO4217" "RWF" "646" 100 100 )
( (N_ "Samoan Tala") "tala" "sene" "ISO4217" "WST" "882" 100 100 )
( (N_ "Sao Tome and Principe Dobra") "Dobra" "centimo" "ISO4217" "STD" "678" 100 100 )
( (N_ "Saudi Riyal") "riyal" "halalat"  "ISO4217" "SAR" "682" 100 100 )
( (N_ "Serbian Dinar") "dinar" "para"  "ISO4217" "RSD"  "941" 100 100) ;; continuation of YUM
( (N_ "Seychelles Rupee") "rupee" "cents" "ISO4217" "SCR" "690" 100 100 )
( (N_ "Sierra Leone Leone") "leone" "cent"  "ISO4217" "SLL" "694" 100 100 )
( (N_ "Singapore Dollar") "dollar" "cent" "ISO4217" "SGD" "702" 100 100 )
( (N_ "Slovak Koruna") "koruna" "halier"  "ISO4217" "SKK" "703" 100 100 )
( (N_ "Slovenian Tolar") "tolar" "stotinov"  "ISO4217" "SIT" "705" 100 100 )
( (N_ "Solomon Islands Dollar") "dollar" "cent"  "ISO4217" "SBD" "090" 100 100 )
( (N_ "Somali Shilling") "shilling" "centisimi" "ISO4217" "SOS" "706" 100 100 )
( (N_ "South African Rand") "rand" "cent" "ISO4217" "ZAR" "710" 100 100 )
( (N_ "South Korean Won") "won" "chon"  "ISO4217" "KRW" "410" 1 1 )
( (N_ "Spanish Peseta") "peseta" "centimo"  "ISO4217" "ESP" "724" 100 100 ) ;; through 1998
( (N_ "Sri Lanka Rupee") "rupee" "cent"  "ISO4217" "LKR" "144" 100 100 )
( (N_ "St. Helena Pound") "pound" "pence"  "ISO4217" "SHP"  "654" 100 100 )
( (N_ "Sudanese Dinar") "dinar" "piastre"  "ISO4217" "SDD" "736" 100 100 ) ;; 1992 on
( (N_ "Sudanese Pound") "pound" "piastre"  "ISO4217" "SDP" "736" 100 100 ) ;; into 1992
( (N_ "Suriname Guilder") "guilder" "cent"  "ISO4217" "SRG" "740" 100 100 ) ;; only until 2003-12-31
( (N_ "Suriname Dollar") "dollar" "cent"  "ISO4217" "SRD" "968" 100 100) ;; Since 2004-01-01
( (N_ "Swaziland Lilangeni") "lilangeni" "cent"  "ISO4217" "SZL" "748" 100 100 )
( (N_ "Swedish Krona") "krona" "ore"  "ISO4217" "SEK" "752" 100 100 )
( (N_ "Swiss Franc") "franc" "centime" "ISO4217" "CHF" "756" 100 100 )
( (N_ "Syrian Pound") "pound" "pence"  "ISO4217" "SYP" "760" 100 100 )
( (N_ "Taiwan Dollar") "dollar" "cent" "ISO4217" "TWD" "901" 100 100 )
( (N_ "Tajikistani Somoni") "somoni" "dirams" "ISO4217" "TJS" "972" 100 100 ) ;; 2002-11-06 on
( (N_ "Tajikistan Ruble") "ruble" "ruble" "ISO4217" "TJR" "762" 1 1 ) ;; until 2002-11-05
( (N_ "Tanzanian Shilling") "shilling" "cent"  "ISO4217" "TZS" "834" 100 100 )
( (N_ "Thai Baht") "baht" "stang" "ISO4217" "THB" "764" 100 100 )
( (N_ "Tongan Pa'anga") "Pa'anga" "seniti" "ISO4217" "TOP" "776" 100 100 )
( (N_ "Trinidad and Tobago Dollar") "dollar" "cent" "ISO4217" "TTD" "780" 100 100 )
( (N_ "Tunisian Dinar") "dinar" "milleme" "ISO4217" "TND" "788" 1000 1000 )
( (N_ "Turkish New Lira") "lira" "kuru" "ISO4217" "TRY" "949" 100 100)
( (N_ "Turkmenistan Manat") "manat" "tenga" "ISO4217" "TMM" "795" 100 100 )
( (N_ "US Dollar") "dollar" "cent" "ISO4217" "USD" "840" 100 100 )
( (N_ "Uganda Shilling") "shilling" "cent"  "ISO4217" "UGX" "800" 100 100  )
( (N_ "Ukraine Hryvnia") "hryvnia" "kopiyka"  "ISO4217" "UAH" "804" 100 100 )
( (N_ "United Arab Emirates Dirham") "dirham" "fil" "ISO4217" "AED" "784" 100 100 )
( (N_ "Uruguayan Peso") "peso" "centesimo" "ISO4217" "UYU" "858" 100 100 )
( (N_ "Uzbekistani Sum") "som" "tiyin" "ISO4217" "UZS" "860" 100 100 )
( (N_ "Vanuatu Vatu") "vatu" "centime" "ISO4217" "VUV" "548" 1 1 )
( (N_ "Venezuelan Bolivar (old)") "bolivar" "centimo" "ISO4217" "VEB" "862" 100 100 ) ;; 1000 replaced by 1 VEF in 2008
( (N_ "Venezuelan Bolivar Fuerte") "bolivar" "centimo" "ISO4217" "VEF" "937" 100 100 ) ;; since 2008-01-01
( (N_ "Vietnamese Dong") "dong" "hao" "ISO4217" "VND" "704" 100 100 )
( (N_ "Yemeni Riyal") "riyal" "fils" "ISO4217" "YER" "886" 100 100 )
( (N_ "Yugoslav Dinar") "dinar" "para"  "ISO4217" "YUM"  "890" 100 100) ;; 2003 replaced by RSD
( (N_ "Zambian Kwacha") "kwacha" "ngwee"  "ISO4217" "ZMK" "894" 100 100 )
( (N_ "Zimbabwe Dollar") "dollar" "cent" "ISO4217" "ZWD" "716" 100 100 )

( (N_  "Special Drawing Rights") "SDR" "SDR" "ISO4217" "XDR" "960" 1 1 ) ;; International Monetary Fund
( (N_ "No currency") "" "" "ISO4217" "XXX" "999" 1 1000000 )

( (N_ "Gold") "ounce" "ounce" "ISO4217" "XAU" "959" 1 1000000 )
( (N_ "Palladium") "ounce" "ounce" "ISO4217" "XPD" "964" 1 1000000 )
( (N_ "Platinum") "ounce" "ounce" "ISO4217" "XPT" "962" 1 1000000 )
( (N_ "Silver") "ounce" "ounce" "ISO4217" "XAG" "961" 1 1000000 )
