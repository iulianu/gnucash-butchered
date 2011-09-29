import unittest

from test import test_support

from test_book import TestBook
from test_account import TestAccount
from test_split import TestSplit
from test_transaction import TestTransaction
from test_business import TestBusiness

def test_main():
    test_support.run_unittest(TestBook, TestAccount, TestSplit, TestTransaction, TestBusiness)

if __name__ == '__main__':
    test_main()
