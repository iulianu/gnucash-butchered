#!/usr/bin/env python

# simple_invoice_insert.py -- Add an invoice to a set of books
#
# Copyright (C) 2010 ParIT Worker Co-operative <transparency@parit.ca>
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, contact:
# Free Software Foundation           Voice:  +1-617-542-5942
# 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
# Boston, MA  02110-1301,  USA       gnu@gnu.org
#
# @author Mark Jenkins, ParIT Worker Co-operative <mark@parit.ca>

# Opens a GnuCash book file and adds an invoice to it for a particular
# customer (by GUID) with a specific ID and value 
#
# The account tree and tax tables are assumed to be the same as the ones
# created in simple_business_create.py, but you can edit that to adapt
# this to become an invoice imported for your own books
#
# Syntax:
# gnucash-env python simple_invoice_insert.py \
#          sqlite3:///home/blah/blah.gnucash
#          dda2ec8e3e63c7715097f852851d6b22 1001 'The Goods' 201.43 

from gnucash import Session, GUID, GncNumeric
from gnucash.gnucash_business import Customer, Invoice, Entry
from gnucash.gnucash_core_c import string_to_guid
from os.path import abspath
from sys import argv
from decimal import Decimal
import datetime

def gnc_numeric_from_decimal(decimal_value):
    sign, digits, exponent = decimal_value.as_tuple()

    # convert decimal digits to a fractional numerator
    # equivlent to
    # numerator = int(''.join(digits))
    # but without the wated conversion to string and back,
    # this is probably the same algorithm int() uses
    numerator = 0
    TEN = int(Decimal(0).radix()) # this is always 10
    numerator_place_value = 1
    # add each digit to the final value multiplied by the place value
    # from least significant to most sigificant
    for i in xrange(len(digits)-1,-1,-1):
        numerator += digits[i] * numerator_place_value
        numerator_place_value *= TEN

    if decimal_value.is_signed():
        numerator = -numerator

    # if the exponent is negative, we use it to set the denominator
    if exponent < 0 :
        denominator = TEN ** (-exponent)
    # if the exponent isn't negative, we bump up the numerator
    # and set the denominator to 1
    else:
        numerator *= TEN ** exponent
        denominator = 1

    return GncNumeric(numerator, denominator)


s = Session(argv[1], False)
# this seems to make a difference in more complex cases
s.save()

book = s.book
root = book.get_root_account()
commod_table = book.get_table()
CAD = commod_table.lookup('CURRENCY', 'CAD')

my_customer_guid = GUID()
result = string_to_guid(argv[2], my_customer_guid.get_instance())
assert( result )
my_customer = book.CustomerLookup(my_customer_guid)
assert( my_customer != None )
assert( isinstance(my_customer, Customer) )

assets = root.lookup_by_name("Assets")
recievables = assets.lookup_by_name("Recievables")
income = root.lookup_by_name("Income")

invoice = Invoice(book, argv[3], CAD, my_customer )
description = argv[4]
invoice_value = gnc_numeric_from_decimal(Decimal(argv[5]))
tax_table = book.TaxTableLookupByName('good tax')
invoice_entry = Entry(book, invoice)
invoice_entry.SetInvTaxTable(tax_table)
invoice_entry.SetInvTaxIncluded(False)
invoice_entry.SetDescription(description)
invoice_entry.SetQuantity( GncNumeric(1) )
invoice_entry.SetInvAccount(income)
invoice_entry.SetInvPrice(invoice_value)

invoice.PostToAccount(recievables, datetime.date.today(), datetime.date.today(),
                      "", True)

s.save()
s.end()
