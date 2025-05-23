# gnucash_core.py -- High level python wrapper classes for the core parts
#                    of GnuCash
#
# Copyright (C) 2008 ParIT Worker Co-operative <paritinfo@parit.ca>
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
# @author Jeff Green,   ParIT Worker Co-operative <jeff@parit.ca>

# The following is for doxygen

## @defgroup python_bindings Python Bindings Module
#  Also have a look at the page @ref python_bindings_page.

## @defgroup python_bindings_examples Python Bindings Examples Module
#  @ingroup python_bindings
#  The python-bindings come with quite a lot of example scripts.

##  @page python_bindings_page Python bindings
#   Also have a look at group @ref python_bindings.
#
#   In the source tree they are located at bindings/python.
#
#   To enable them in the compilation process you have to add  -DWITH_PYTHON=ON
#   to the call of cmake.
#
#   As a starting point have a look at the \link python_bindings_examples example-scripts\endlink.
#
#   @section possibilities What can Python Bindings be used for ?
#
#   The python bindings supply the ability to access a wide range of the core functions of GnuCash. You
#   can read and write Transactions, Commodities, Lots, access the business stuff... You gain the ability
#   to manipulate your financial data with a flexible scripting language.
#
#   Not everything GnuCash can is possible to access though. The bindings focus on basic accounting functions.
#   Have a look at the examples to get an impression.
#
#   Some functions are broken because they have not been wrapped properly. They may crash the program or return unaccessible values.
#   Please file a bug report if you find one to help support the development process.
#
#   @section python_bindings_section Principles
#   The python-bindings are generated using SWIG from parts of the source-files of GnuCash.
#
#   @note Python-scripts should not be executed while GnuCash runs. GnuCash is designed as
#   a single user application with only one program accessing the data at one time. You can force your
#   access but that may corrupt data. Maybe one day that may change but for the moment there is no active development on that.
#
#   @subsection swigworks What SWIG does
#
#   SWIG extracts information from the c-sources and provides access to the structures
#   to python. It's work is controlled by interface files :
#
#   @li gnucash_core.i
#   @li timespec.i
#   @li glib.i
#   @li @link base-typemaps.i src/base-typemaps.i @endlink This file is shared with Guile.
#
#   it outputs:
#
#   @li gnucash_core.c
#   @li gnucash_core_c.py
#
#   If you have generated your own local doxygen documentation (by "make doc") after having compiled the python-bindings, doxygen
#   will include SWIGs output-files.
#   It's actually quite interesting to have a look at them through doxygen, because they contain all that you can
#   access from python.
#
#   This c-style-api is the bottom layer. It is a quite raw extract and close to the original source. Some more details are described further down.
#
#   For some parts there is a second layer of a nice pythonic interface. It is declared
#   in
#   @li gnucash_core.py and
#   @li gnucash_business.py.
#   @li function_class.py contains helper functions for that.
#
#   @section howto How to use the Python bindings
#   @subsection highlevel High level python wrapper classes
#   If you
#
#   @code >> import gnucash @endcode
#
#   You can access the structures of the high level api. For Example you get a Session object by
#
#   @code >> session=gnucash.Session() @endcode
#
#   Here you will find easy to use things. But sometimes - and at the current level rather sooner than
#   later - you may be forced to search for solutions at the :
#
#   @subsection c_style_api C-style-api
#
#   If you
#
#   @code >> import gnucash @endcode
#
#   The c-style-api can be accessed via gnucash.gnucash_core_c. You can have a look at all the possibilities
#   at gnucash_core_c.py.
#
#   You will find a lot of pointers here which you can just ignore if input and output of the function have the
#   same type.
#
#   For example you could start a session by gnucash.gnucash_core_c.qof_session_begin(). But if you just try
#
#   @code session=gnucash.gnucash_core_c.qof_session_begin() @endcode
#
#   you will get an error message and realize the lack of convenience for you have to add the correct function parameters.
#
#   Not all of the available structures will work. SWIG just takes everything from the sources that it is fed with and translates it. Not everything
#   is a working translation, because not everything has been worked through. At this point you are getting closer to the developers who you can
#   contact at the mailing-list gnucash-devel@gnucash.org. There may be a workaround. Maybe the problem can only be fixed by changing SWIGs input
#   files to correctly translate the c-source. Feel free to post a question at the developers list. It may awaken the interest of someone who creates
#   some more beautiful python-interfaces.
#
#   @section Thisorthat When to use which api ?
#
#   The start would surely be the high-level api for you can be quite sure to have something working and you will maybe find
#   explanations in the example-scripts. If you search for something that is not yet implemented in that way you will have to
#   take your way to the c-style-api.
#
#   @section pydoc (Further) documentation
#
#   @li The documentation you just read uses doxygen. It collects documentation in GnuCash's sources. Besides that there is
#   @li the classic python-documentation using help() and docstrings. Have a look at both.
#   @li There is a page in the GnuCash wiki at https://wiki.gnucash.org/wiki/Python
#   @li You may also have a look into the archives of gnucash-devel@gnucash.org.
#   @li On Bugzilla there is also some interesting talk regarding the development process.
#   @li Then you can use the abilities of git to see the history of the code by @code git log @endcode done in the directory of the python-bindings.
#
#   @section pytodo To-Do
#   @li Work out the relation of scheme/guile and python-bindings
#   @li maybe join python_bindings_page and group
#   @li work on the structure of the documentation to make it more clear
#   @li try to make SWIG include the documentation of the c-source
#   @li make function-links in SWIG-generated files work.
#   @li some words to the tests
#
# @author Christoph Holtermann
# @date December 2010


## @file
#  @brief High level python wrapper classes for the core parts of GnuCash
#  @author Mark Jenkins, ParIT Worker Co-operative <mark@parit.ca>
#  @author Jeff Green,   ParIT Worker Co-operative <jeff@parit.ca>
#  @ingroup python_bindings

import operator

from enum import IntEnum
from urllib.parse import urlparse

from gnucash import gnucash_core_c
from gnucash import _sw_core_utils

from gnucash.function_class import \
     ClassFromFunctions, extract_attributes_with_prefix, \
     default_arguments_decorator, method_function_returns_instance, \
     methods_return_instance, process_list_convert_to_instance, \
     method_function_returns_instance_list, methods_return_instance_lists

from gnucash.gnucash_core_c import gncInvoiceLookup, gncInvoiceGetInvoiceFromTxn, \
    gncInvoiceGetInvoiceFromLot, gncEntryLookup, gncInvoiceLookup, \
    gncCustomerLookup, gncVendorLookup, gncJobLookup, gncEmployeeLookup, \
    gncTaxTableLookup, gncTaxTableLookupByName, gnc_search_invoice_on_id, \
    gnc_search_customer_on_id, gnc_search_bill_on_id , \
    gnc_search_vendor_on_id, gncInvoiceNextID, gncCustomerNextID, \
    gncVendorNextID, gncTaxTableGetTables, gnc_numeric_zero, \
    gnc_numeric_create, double_to_gnc_numeric, gnc_numeric_from_string, \
    gnc_numeric_to_string, gnc_numeric_check

from gnucash.deprecation import (
    deprecated_args_session,
    deprecated_args_session_init,
    deprecated_args_session_begin,
    deprecated
)

try:
    import gettext

    _localedir = _sw_core_utils.gnc_path_get_localedir()
    gettext.install(_sw_core_utils.GETTEXT_PACKAGE, _localedir)
except:
    print()
    print("Problem importing gettext!")
    import traceback
    import sys
    exc_type, exc_value, exc_traceback = sys.exc_info()
    traceback.print_exception(exc_type, exc_value, exc_traceback)
    print()

    def _(s):
        """Null translator function, gettext not available"""
        return s

    import builtins
    builtins.__dict__['_'] = _

class GnuCashCoreClass(ClassFromFunctions):
    _module = gnucash_core_c

    def do_lookup_create_oo_instance(self, lookup_function, cls, *args):
        thing = lookup_function(self.get_instance(), *args)
        if thing != None:
            thing = cls(instance=thing)
        return thing


class GnuCashBackendException(Exception):
    def __init__(self, msg, errors):
        Exception.__init__(self, msg)
        self.errors = errors


class SessionOpenMode(IntEnum):
    """Mode for opening sessions.

    This replaces three booleans that were passed in order: ignore_lock, create,
    and force. It's structured so that one can use it as a bit field with the
    values in the same order, i.e. ignore_lock = 1 << 2, create_new = 1 << 1, and
    force_new = 1.

    enumeration members
    -------------------

    SESSION_NORMAL_OPEN = 0     (All False)
    Open will fail if the URI doesn't exist or is locked.

    SESSION_NEW_STORE = 2       (False, True, False (create))
    Create a new store at the URI. It will fail if the store already exists and is found to contain data that would be overwritten.

    SESSION_NEW_OVERWRITE = 3   (False, True, True (create | force))
    Create a new store at the URI even if a store already exists there.

    SESSION_READ_ONLY = 4,      (True, False, False (ignore_lock))
    Open the session read-only, ignoring any existing lock and not creating one if the URI isn't locked.

    SESSION_BREAK_LOCK = 5     (True, False, True (ignore_lock | force))
    Open the session, taking over any existing lock.

    source: lignucash/engine/qofsession.h
    """

    SESSION_NORMAL_OPEN = gnucash_core_c.SESSION_NORMAL_OPEN
    """All False
    Open will fail if the URI doesn't exist or is locked."""

    SESSION_NEW_STORE = gnucash_core_c.SESSION_NEW_STORE
    """False, True, False (create)
    Create a new store at the URI. It will fail if the store already exists and is found to contain data that would be overwritten."""

    SESSION_NEW_OVERWRITE = gnucash_core_c.SESSION_NEW_OVERWRITE
    """False, True, True (create | force)
    Create a new store at the URI even if a store already exists there."""

    SESSION_READ_ONLY = gnucash_core_c.SESSION_READ_ONLY
    """True, False, False (ignore_lock)
    Open the session read-only, ignoring any existing lock and not creating one if the URI isn't locked."""

    SESSION_BREAK_LOCK = gnucash_core_c.SESSION_BREAK_LOCK
    """True, False, True (ignore_lock | force)
    Open the session, taking over any existing lock."""


class Session(GnuCashCoreClass):
    """A GnuCash book editing session

    To commit changes to the session you may need to call save,
    (this is always the case with the file backend).

    When you're down with a session you may need to call end()

    Every Session has a Book in the book attribute, which you'll definitely
    be interested in, as every GnuCash entity (Transaction, Split, Vendor,
    Invoice..) is associated with a particular book where it is stored.
    """

    @deprecated_args_session_init
    def __init__(self, book_uri=None, mode=None, instance=None, book=None):
        """!
        A convenient constructor that allows you to specify a book URI,
        begin the session, and load the book.

        This can give you the power of calling
        qof_session_new, qof_session_begin, and qof_session_load all in one!

        qof_session_load is only called if url scheme is "xml" and
        mode is SESSION_NEW_STORE or SESSION_NEW_OVERWRITE

        @param book_uri must be a string in the form of a URI/URL. The access
        method specified depends on the loaded backends. Paths may be relative
        or absolute.  If the path is relative, that is if the argument is
        "file://somefile.xml", then the current working directory is
        assumed. Customized backends can choose to search other
        application-specific directories or URI schemes as well.
        It be None to skip the calls to qof_session_begin and
        qof_session_load.

        @param instance argument can be passed if new Session is used as a
        wrapper for an existing session instance

        @param mode The SessionOpenMode.
        @note SessionOpenMode replaces deprecated ignore_lock, is_new and force_new.

        @par SessionOpenMode
        `SESSION_NORMAL_OPEN`: Find an existing file or database at the provided uri and
        open it if it is unlocked. If it is locked post a QOF_BACKEND_LOCKED error.
        @par
        `SESSION_NEW_STORE`: Check for an existing file or database at the provided
        uri and if none is found, create it. If the file or database exists post a
        QOF_BACKED_STORE_EXISTS and return.
        @par
        `SESSION_NEW_OVERWRITE`: Create a new file or database at the provided uri,
        deleting any existing file or database.
        @par
        `SESSION_READ_ONLY`: Find an existing file or database and open it without
        disturbing the lock if it exists or setting one if not. This will also set a
        flag on the book that will prevent many elements from being edited and will
        prevent the backend from saving any edits.
        @par
        `SESSION_BREAK_LOCK`: Find an existing file or database, lock it, and open
        it. If there is already a lock replace it with a new one for this session.

        @par Errors
        qof_session_begin() signals failure by queuing errors. After it completes use
        qof_session_get_error() and test that the value is `ERROR_BACKEND_NONE` to
        determine that the session began successfully.

        @exception as begin() and load() are wrapped with raise_backend_errors_after_call()
        this function can raise a GnuCashBackendException. If it does,
        you don't need to cleanup and call end() and destroy(), that is handled
        for you, and the exception is raised.
        """
        if instance is not None:
            GnuCashCoreClass.__init__(self, instance=instance)
        else:
            if book is None:
                book = Book()
            GnuCashCoreClass.__init__(self, book)

        if book_uri is not None:
            try:
                if mode is None:
                    mode = SessionOpenMode.SESSION_NORMAL_OPEN
                self.begin(book_uri, mode)
                is_new = mode in (SessionOpenMode.SESSION_NEW_STORE, SessionOpenMode.SESSION_NEW_OVERWRITE)
                if not is_new:
                    self.load()
            except GnuCashBackendException as backend_exception:
                self.end()
                self.destroy()
                raise

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # Roll back changes on exception by not calling save. Only works for XMl backend.
        if not exc_type:
            self.save()
        self.end()

    def raise_backend_errors(self, called_function="qof_session function"):
        """Raises a GnuCashBackendException if there are outstanding
        QOF_BACKEND errors.

        set called_function to name the function that was last called
        """
        errors = self.pop_all_errors()
        if errors != ():
            raise GnuCashBackendException(
                "call to %s resulted in the "
                "following errors, %s" % (called_function, backend_error_dict[errors[0]]),
                errors )

    def generate_errors(self):
        """A generator that yields any outstanding QofBackend errors
        """
        while self.get_error() is not ERR_BACKEND_NO_ERR:
            error = self.pop_error()
            yield error

    def pop_all_errors(self):
        """Returns any accumulated qof backend errors as a tuple
        """
        return tuple( self.generate_errors() )

    # STATIC METHODS
    @staticmethod
    def raise_backend_errors_after_call(function, *args, **kwargs):
        """A function decorator that results in a call to
        raise_backend_errors after execution.
        """
        def new_function(self, *args, **kwargs):
            return_value = function(self, *args, **kwargs)
            self.raise_backend_errors(function.__name__)
            return return_value
        return new_function

class Book(GnuCashCoreClass):
    """A Book encapsulates all of the GnuCash data, it is the place where
    all GnuCash entities (Transaction, Split, Vendor, Invoice...), are
    stored. You'll notice that all of the constructors for those entities
    need a book to be associated with.

    The most common way to get a book is through the book property in the
    Session class, that is, create a session that connects to some storage,
    such as through 'my_session = Session('file:my_books.xac')', and access
    the book via the book property, 'my_session.book'

    If you would like to create a Book without any backing storage, call the
    Book constructor without any parameters, 'Book()'. You can later merge
    such a book into a book with actual store by using merge_init.

    Methods of interest
    get_root_account -- Returns the root level Account
    get_table -- Returns a commodity lookup table, of type GncCommodityTable
    """
    def InvoiceLookup(self, guid):
        from gnucash.gnucash_business import Invoice
        return self.do_lookup_create_oo_instance(
            gncInvoiceLookup, Invoice, guid.get_instance() )

    def EntryLookup(self, guid):
        from gnucash.gnucash_business import Entry
        return self.do_lookup_create_oo_instance(
            gncEntryLookup, Entry, guid.get_instance() )

    def CustomerLookup(self, guid):
        from gnucash.gnucash_business import Customer
        return self.do_lookup_create_oo_instance(
            gncCustomerLookup, Customer, guid.get_instance())

    def JobLookup(self, guid):
        from gnucash.gnucash_business import Job
        return self.do_lookup_create_oo_instance(
            gncJobLookup, Job, guid.get_instance() )

    def VendorLookup(self, guid):
        from gnucash.gnucash_business import Vendor
        return self.do_lookup_create_oo_instance(
            gncVendorLookup, Vendor, guid.get_instance() )

    def EmployeeLookup(self, guid):
        from gnucash.gnucash_business import Employee
        return self.do_lookup_create_oo_instance(
            gncEmployeeLookup, Employee, guid.get_instance() )

    def TaxTableLookup(self, guid):
        from gnucash.gnucash_business import TaxTable
        return self.do_lookup_create_oo_instance(
            gncTaxTableLookup, TaxTable, guid.get_instance() )

    def TaxTableLookupByName(self, name):
        from gnucash.gnucash_business import TaxTable
        return self.do_lookup_create_oo_instance(
            gncTaxTableLookupByName, TaxTable, name)

    def TaxTableGetTables(self):
        from gnucash.gnucash_business import TaxTable
        return [ TaxTable(instance=item) for item in gncTaxTableGetTables(self.instance) ]

    def BillLookupByID(self, id):
        from gnucash.gnucash_business import Bill
        return self.do_lookup_create_oo_instance(
            gnc_search_bill_on_id, Bill, id)

    def InvoiceLookupByID(self, id):
        from gnucash.gnucash_business import Invoice
        return self.do_lookup_create_oo_instance(
            gnc_search_invoice_on_id, Invoice, id)

    def CustomerLookupByID(self, id):
        from gnucash.gnucash_business import Customer
        return self.do_lookup_create_oo_instance(
            gnc_search_customer_on_id, Customer, id)

    def VendorLookupByID(self, id):
        from gnucash.gnucash_business import Vendor
        return self.do_lookup_create_oo_instance(
            gnc_search_vendor_on_id, Vendor, id)

    def InvoiceNextID(self, customer):
      ''' Return the next invoice ID.
      '''
      from gnucash.gnucash_core_c import gncInvoiceNextID
      return gncInvoiceNextID(self.get_instance(),customer.GetEndOwner().get_instance()[1])

    def BillNextID(self, vendor):
      ''' Return the next Bill ID. '''
      from gnucash.gnucash_core_c import gncInvoiceNextID
      return gncInvoiceNextID(self.get_instance(),vendor.GetEndOwner().get_instance()[1])

    def CustomerNextID(self):
      ''' Return the next Customer ID. '''
      from gnucash.gnucash_core_c import gncCustomerNextID
      return gncCustomerNextID(self.get_instance())

    def VendorNextID(self):
      ''' Return the next Vendor ID. '''
      from gnucash.gnucash_core_c import gncVendorNextID
      return gncVendorNextID(self.get_instance())

class GncNumeric(GnuCashCoreClass):
    """Object used by GnuCash to store all numbers. Always consists of a
    numerator and denominator.

    The constants GNC_DENOM_AUTO,
    GNC_HOW_RND_FLOOR, GNC_HOW_RND_CEIL, GNC_HOW_RND_TRUNC,
    GNC_HOW_RND_PROMOTE, GNC_HOW_RND_ROUND_HALF_DOWN,
    GNC_HOW_RND_ROUND_HALF_UP, GNC_HOW_RND_ROUND, GNC_HOW_RND_NEVER,
    GNC_HOW_DENOM_EXACT, GNC_HOW_DENOM_REDUCE, GNC_HOW_DENOM_LCD,
    and GNC_HOW_DENOM_FIXED are available for arithmetic
    functions like GncNumeric.add

    Look at gnc-numeric.h to see how to use these
    """

    def __init__(self, *args, **kargs):
        """Constructor that supports the following formats:
        * No arguments defaulting to zero: eg. GncNumeric() == 0/1
        * A integer: e.g. GncNumeric(1) == 1/1
        * Numerator and denominator intager pair: eg. GncNumeric(1, 2) == 1/2
        * A floating point number: e.g. GncNumeric(0.5) == 1/2
        * A floating point number with defined conversion: e.g.
          GncNumeric(0.5, GNC_DENOM_AUTO,
                    GNC_HOW_DENOM_FIXED | GNC_HOW_RND_NEVER) == 1/2
        * A string: e.g. GncNumeric("1/2") == 1/2
        """
        if 'instance' not in kargs:
            kargs['instance'] = GncNumeric.__args_to_instance(args)
        GnuCashCoreClass.__init__(self, [], **kargs)

    @staticmethod
    def __args_to_instance(args):
        if len(args) == 0:
            return gnc_numeric_zero()
        elif len(args) == 1:
            arg = args[0]
            if isinstance(arg, int):
                return gnc_numeric_create(arg, 1)
            elif isinstance(arg, float):
                return double_to_gnc_numeric(arg, GNC_DENOM_AUTO, GNC_HOW_DENOM_FIXED | GNC_HOW_RND_NEVER)
            elif isinstance(arg, str):
                instance = gnc_numeric_from_string(arg)
                if gnc_numeric_check(instance):
                    raise TypeError('Failed to convert to GncNumeric: ' + str(args))
                return instance
            elif isinstance(arg, GncNumeric):
                return arg.instance
            else:
                raise TypeError('Only single int/float/str/GncNumeric allowed: ' + str(args))
        elif len(args) == 2:
            if isinstance(args[0], int) and isinstance(args[1], int):
                return gnc_numeric_create(*args)
            else:
                raise TypeError('Only two ints allowed: ' + str(args))
        elif len(args) == 3:
            if isinstance(args[0], float) \
                and isinstance(args[1], int) \
                and type(args[2]) == type(GNC_HOW_DENOM_FIXED):
                return double_to_gnc_numeric(*args)
            else:
                raise TypeError('Only (float, int, GNC_HOW_RND_*) allowed: ' + str(args))
        else:
            raise TypeError('Required single int/float/str or two ints: ' + str(args))

    # from https://docs.python.org/3/library/numbers.html#numbers.Integral
    # and https://github.com/python/cpython/blob/3.7/Lib/fractions.py

    def _operator_fallbacks(monomorphic_operator, fallback_operator):
        """fallbacks are not needed except for method name,
        keep for possible later use"""
        def forward(a, b):
            if isinstance(b, GncNumeric):
                return monomorphic_operator(a, b)
            if isinstance(b, (int, float)):
                return monomorphic_operator(a, GncNumeric(b))
            else:
                return NotImplemented
        forward.__name__ = '__' + fallback_operator.__name__ + '__'
        forward.__doc__ = monomorphic_operator.__doc__

        def reverse(b, a):
            if isinstance(a, (GncNumeric, int, float)):
                return forward(b, a)
            else:
                return NotImplemented
        reverse.__name__ = '__r' + fallback_operator.__name__ + '__'
        reverse.__doc__ = monomorphic_operator.__doc__

        return forward, reverse

    def _add(a, b):
        return a.add(b, GNC_DENOM_AUTO, GNC_HOW_RND_ROUND)

    def _sub(a, b):
        return a.sub(b, GNC_DENOM_AUTO, GNC_HOW_RND_ROUND)

    def _mul(a, b):
        return a.mul(b, GNC_DENOM_AUTO, GNC_HOW_RND_ROUND)

    def _div(a, b):
        return a.div(b, GNC_DENOM_AUTO, GNC_HOW_RND_ROUND)

    def _floordiv(a, b):
        return a.div(b, 1, GNC_HOW_RND_TRUNC)

    __add__, __radd__ = _operator_fallbacks(_add, operator.add)
    __iadd__ = __add__
    __sub__, __rsub__ = _operator_fallbacks(_sub, operator.sub)
    __isub__ = __sub__
    __mul__, __rmul__ = _operator_fallbacks(_mul, operator.mul)
    __imul__ = __mul__
    __truediv__, __rtruediv__ = _operator_fallbacks(_div, operator.truediv)
    __itruediv__ = __truediv__
    __floordiv__, __rfloordiv__ = _operator_fallbacks(_floordiv, operator.floordiv)
    __ifloordiv__ = __floordiv__

    # Comparisons derived from https://github.com/python/cpython/blob/3.7/Lib/fractions.py
    def _lt(a, b):
        return a.compare(b) == -1

    def _gt(a, b):
        return a.compare(b) == 1

    def _le(a, b):
        return a.compare(b) in (0,-1)

    def _ge(a, b):
        return a.compare(b) in (0,1)

    def _eq(a, b):
        return a.compare(b) == 0

    def _richcmp(self, other, op):
        """Helper for comparison operators, for internal use only.
        Implement comparison between a GncNumeric instance `self`,
        and either another GncNumeric instance, an int or a float
        `other`.  If `other` is not an instance of that kind, return
        NotImplemented. `op` should be one of the six standard
        comparison operators. The comparisons are based on
        GncNumeric.compare().
        """
        import math
        if isinstance(other, GncNumeric):
            return op(other)
        elif isinstance(other, (int, float)):
            return op(GncNumeric(other))
        else:
            return NotImplemented

    def __lt__(a, b):
        """a < b"""
        return a._richcmp(b, a._lt)

    def __gt__(a, b):
        """a > b"""
        return a._richcmp(b, a._gt)

    def __le__(a, b):
        """a <= b"""
        return a._richcmp(b, a._le)

    def __ge__(a, b):
        """a >= b"""
        return a._richcmp(b, a._ge)

    def __eq__(a, b):
        """a == b"""
        return a._richcmp(b, a._eq)

    def __bool__(a):
        """a != 0"""
        return bool(a.num())

    def __float__(self):
        return self.to_double()

    def __int__(self):
        return int(self.to_double())

    def __pos__(a):
        """+a"""
        return GncNumeric(a.num(), a.denom())

    def __neg__(a):
        """-a"""
        return a.neg()

    def __abs__(a):
        """abs(a)"""
        return a.abs()

    def to_fraction(self):
        from fractions import Fraction
        return Fraction(self.num(), self.denom())

    def __str__(self):
        """Returns a human readable numeric value string as UTF8."""
        return gnc_numeric_to_string(self.instance)

class GncPrice(GnuCashCoreClass):
    '''
    Each priceEach price in the database represents an "instantaneous"
    quote for a given commodity with respect to another commodity.
    For example, a given price might represent the value of LNUX in USD on 2001-02-03.

    Fields:
      * commodity: the item being priced.
      * currency: the denomination of the value of the item being priced.
      * value: the value of the item being priced.
      * time: the time the price was valid.
      * source: a string describing the source of the quote. These strings will be something like this:
      "Finance::Quote", "user:misc", "user:foo", etc. If the quote came from a user, as a matter of policy,
      you *must* prefix the string you give with "user:". For now, the only other reserved values are
      "Finance::Quote" and "old-file-import". Any string used must be added to the source_list array in
      dialog-price-edit-db.c so that it can be properly translated. (There are unfortunately many strings
      in users' databases, so this string must be translated on output instead of always being used in untranslated form).
      * type: the type of quote - types possible right now are bid, ask, last, nav, and
      unknown.Each price in the database represents an "instantaneous" quote for a given
      commodity with respect to another commodity.
      For example, a given price might represent the value of LNUX in USD on 2001-02-03.

      See also https://code.gnucash.org/docs/head/group__Price.html
    '''
    _new_instance = 'gnc_price_create'
GncPrice.add_methods_with_prefix('gnc_price_')


class GncPriceDB(GnuCashCoreClass):
    '''
    a simple price database for gnucash.
    The PriceDB is intended to be a database of price quotes, or more specifically,
    a database of GNCPrices. For the time being, it is still a fairly simple
    database supporting only fairly simple queries. It is expected that new
    queries will be added as needed, and that there is some advantage to delaying
    complex queries for now in the hope that we get a real DB implementation
    before they're really needed.

    Every QofBook contains a GNCPriceDB, accessible via gnc_pricedb_get_db.

    Definition in file gnc-pricedb.h.
    See also https://code.gnucash.org/docs/head/gnc-pricedb_8h.html
    '''

@deprecated("Use gnc_pricedb_latest_before_t64")
def gnc_pricedb_lookup_latest_before_t64(self, commodity, currency, date):
    return self.lookup_nearest_before_t64(commodity, currency, date)

GncPriceDB.add_method('gnc_pricedb_lookup_latest_before_t64', 'lookup_latest_before_t64')

GncPriceDB.lookup_latest_before_t64 = method_function_returns_instance(GncPriceDB.lookup_latest_before_t64, GncPrice)

GncPriceDB.add_methods_with_prefix('gnc_pricedb_')
PriceDB_dict =  {
                'lookup_latest' : GncPrice,
                'lookup_nearest_in_time64' : GncPrice,
                'lookup_nearest_before_t64' : GncPrice,
                'convert_balance_latest_price' : GncNumeric,
                'convert_balance_nearest_price_t64' : GncNumeric,
                }
methods_return_instance(GncPriceDB,PriceDB_dict)
GncPriceDB.get_prices = method_function_returns_instance_list(
    GncPriceDB.get_prices, GncPrice )

class GncCommodity(GnuCashCoreClass): pass

class GncCommodityTable(GnuCashCoreClass):
    """A CommodityTable provides a way to store and lookup commodities.
    Commodities are primarily currencies, but other tradable things such as
    stocks, mutual funds, and material substances are possible.

    Users of this library should not create their own CommodityTable, instead
    the get_table method from the Book class should be used.

    This table is automatically populated with the GnuCash default commodity's
    which includes most of the world's currencies.
    """

    def _get_namespaces_py(self):
        return [ns.get_name() for ns in self.get_namespaces_list()]

class GncCommodityNamespace(GnuCashCoreClass):
    pass

class GncLot(GnuCashCoreClass):
    def GetInvoiceFromLot(self):
        from gnucash.gnucash_business import Invoice
        return self.do_lookup_create_oo_instance(
            gncInvoiceGetInvoiceFromLot, Invoice )

class Transaction(GnuCashCoreClass):
    """A GnuCash Transaction

    Consists of at least one (generally two) splits to represent a transaction
    between two accounts.


    Has a GetImbalance() method that returns a list of all the imbalanced
    currencies. Each list item is a two element tuple, the first element is
    the imbalanced commodity, the second element is the value.

    Warning, the commodity.get_instance() value can be None when there
    is no currency set for the transaction.
    """
    _new_instance = 'xaccMallocTransaction'
    def GetNthSplit(self, n):
        return self.GetSplitList().pop(n)

    def GetInvoiceFromTxn(self):
        from gnucash.gnucash_business import Invoice
        return self.do_lookup_create_oo_instance(
            gncInvoiceGetInvoiceFromTxn, Invoice )

    def __eq__(self, other):
        return self.Equal(other, True, False, False, False)

def decorate_monetary_list_returning_function(orig_function):
    def new_function(self, *args):
        """decorate function that returns list of gnc_monetary to return tuples of GncCommodity and GncNumeric

        Args:
            *args: Variable length argument list. Will get passed to orig_function

        Returns:
            array of tuples: (GncCommodity, GncNumeric)

        ToDo:
            Maybe this function should better reside in module function_class (?)"""
        # warning, item.commodity has been shown to be None
        # when the transaction doesn't have a currency
        return [(GncCommodity(instance=item.commodity),
                 GncNumeric(instance=item.value))
                for item in orig_function(self, *args) ]
    return new_function

class Split(GnuCashCoreClass):
    """A GnuCash Split

    The most basic representation of a movement of currency from one account to
    another.
    """
    _new_instance = 'xaccMallocSplit'

    def __eq__(self, other):
        return self.Equal(other, True, False, False)

class Account(GnuCashCoreClass):
    """A GnuCash Account.

    A fundamental entity in accounting, an Account provides representation
    for a financial object, such as a ACCT_TYPE_BANK account, an
    ACCT_TYPE_ASSET (like a building),
    a ACCT_TYPE_LIABILITY (such as a bank loan), a summary of some type of
    ACCT_TYPE_EXPENSE, or a summary of some source of ACCT_TYPE_INCOME .

    The words in upper case are the constants that GnuCash and this library uses
    to describe account type. Here is the full list:
    ACCT_TYPE_ASSET, ACCT_TYPE_BANK, ACCT_TYPE_CASH, ACCT_TYPE_CHECKING, \
    ACCT_TYPE_CREDIT, ACCT_TYPE_EQUITY, ACCT_TYPE_EXPENSE, ACCT_TYPE_INCOME, \
    ACCT_TYPE_LIABILITY, ACCT_TYPE_MUTUAL, ACCT_TYPE_PAYABLE, \
    ACCT_TYPE_RECEIVABLE, ACCT_TYPE_STOCK, ACCT_TYPE_ROOT, ACCT_TYPE_TRADING

    These are not strings, they are attributes you can import from this
    module
    """
    _new_instance = 'xaccMallocAccount'

class GUID(GnuCashCoreClass):
    _new_instance = 'guid_new_return'

# Session
Session.add_constructor_and_methods_with_prefix('qof_session_', 'new')

def one_arg_default_none(function):
    return default_arguments_decorator(function, None, None)
Session.decorate_functions(one_arg_default_none, "load", "save")

Session.decorate_functions( Session.raise_backend_errors_after_call,
                            "begin", "load", "save", "end")
Session.decorate_method(default_arguments_decorator, "begin", None, mode=SessionOpenMode.SESSION_NORMAL_OPEN)
Session.decorate_functions(deprecated_args_session_begin, "begin")

Session.get_book = method_function_returns_instance(
    Session.get_book, Book )

Session.book = property( Session.get_book )

# import all of the session backend error codes into this module
this_module_dict = globals()
for error_name, error_value, error_name_after_prefix in \
    extract_attributes_with_prefix(gnucash_core_c, 'ERR_'):
    this_module_dict[ error_name ] = error_value

#backend error codes used for reverse lookup
backend_error_dict = {}
for error_name, error_value, error_name_after_prefix in \
    extract_attributes_with_prefix(gnucash_core_c, 'ERR_'):
    backend_error_dict[ error_value ] = error_name

# GncNumeric denominator computation schemes
# Used for the denom argument in arithmetic functions like GncNumeric.add
from gnucash.gnucash_core_c import GNC_DENOM_AUTO

# GncNumeric rounding instructions
# used for the how argument in arithmetic functions like GncNumeric.add
from gnucash.gnucash_core_c import \
    GNC_HOW_RND_FLOOR, GNC_HOW_RND_CEIL, GNC_HOW_RND_TRUNC, \
    GNC_HOW_RND_PROMOTE, GNC_HOW_RND_ROUND_HALF_DOWN, \
    GNC_HOW_RND_ROUND_HALF_UP, GNC_HOW_RND_ROUND, GNC_HOW_RND_NEVER

# GncNumeric denominator types
# used for the how argument in arithmetic functions like GncNumeric.add
from gnucash.gnucash_core_c import \
    GNC_HOW_DENOM_EXACT, GNC_HOW_DENOM_REDUCE, GNC_HOW_DENOM_LCD, \
    GNC_HOW_DENOM_FIXED, GNC_HOW_DENOM_SIGFIG

# import account types
from gnucash.gnucash_core_c import \
    ACCT_TYPE_ASSET, ACCT_TYPE_BANK, ACCT_TYPE_CASH, ACCT_TYPE_CHECKING, \
    ACCT_TYPE_CREDIT, ACCT_TYPE_EQUITY, ACCT_TYPE_EXPENSE, ACCT_TYPE_INCOME, \
    ACCT_TYPE_LIABILITY, ACCT_TYPE_MUTUAL, ACCT_TYPE_PAYABLE, \
    ACCT_TYPE_RECEIVABLE, ACCT_TYPE_STOCK, ACCT_TYPE_ROOT, ACCT_TYPE_TRADING

#Book
Book.add_constructor_and_methods_with_prefix('qof_book_', 'new')
Book.add_method('gnc_book_get_root_account', 'get_root_account')
Book.add_method('gnc_book_set_root_account', 'set_root_account')
Book.add_method('gnc_commodity_table_get_table', 'get_table')
Book.add_method('gnc_pricedb_get_db', 'get_price_db')
Book.add_method('qof_book_increment_and_format_counter', 'increment_and_format_counter')

#Functions that return Account
Book.get_root_account = method_function_returns_instance(
    Book.get_root_account, Account )
#Functions that return GncCommodityTable
Book.get_table = method_function_returns_instance(
    Book.get_table, GncCommodityTable )
#Functions that return GNCPriceDB
Book.get_price_db = method_function_returns_instance(
    Book.get_price_db, GncPriceDB)

# GncNumeric
GncNumeric.add_constructor_and_methods_with_prefix('gnc_numeric_', 'create')

gncnumeric_dict =   {
                        'same' : GncNumeric,
                        'add' : GncNumeric,
                        'sub' : GncNumeric,
                        'mul' : GncNumeric,
                        'div' : GncNumeric,
                        'neg' : GncNumeric,
                        'abs' : GncNumeric,
                        'add_fixed' : GncNumeric,
                        'sub_fixed' : GncNumeric,
                        'convert' : GncNumeric,
                        'reduce' : GncNumeric,
                        'invert' : GncNumeric
                    }
methods_return_instance(GncNumeric, gncnumeric_dict)

# GncCommodity
GncCommodity.add_constructor_and_methods_with_prefix('gnc_commodity_', 'new')
#Functions that return GncCommodity
GncCommodity.clone = method_function_returns_instance(
    GncCommodity.clone, GncCommodity )

# GncCommodityTable
GncCommodityTable.add_methods_with_prefix('gnc_commodity_table_')
commoditytable_dict =   {
                            'lookup' : GncCommodity,
                            'lookup_unique' : GncCommodity,
                            'find_full' : GncCommodity,
                            'insert' : GncCommodity,
                            'add_namespace': GncCommodityNamespace,
                            'find_namespace': GncCommodityNamespace,
                        }
methods_return_instance(GncCommodityTable, commoditytable_dict)

methods_return_instance_lists(
    GncCommodityTable, { 'get_namespaces_list': GncCommodityNamespace,
                         'get_commodities': GncCommodity,
                         'get_quotable_commodities': GncCommodity,

                       } )
setattr(GncCommodityTable, 'get_namespaces', getattr(GncCommodityTable, '_get_namespaces_py'))

# GncCommodityNamespace
GncCommodityNamespace.add_methods_with_prefix('gnc_commodity_namespace_')
GncCommodityNamespace.get_commodity_list = \
    method_function_returns_instance_list(
    GncCommodityNamespace.get_commodity_list, GncCommodity )

# GncLot
GncLot.add_constructor_and_methods_with_prefix('gnc_lot_', 'new')

gnclot_dict =   {
                    'get_account' : Account,
                    'get_book' : Book,
                    'get_earliest_split' : Split,
                    'get_latest_split' : Split,
                    'get_balance' : GncNumeric,
                    'lookup' : GncLot,
                    'make_default' : GncLot
                }
methods_return_instance(GncLot, gnclot_dict)

# Transaction
Transaction.add_methods_with_prefix('xaccTrans')
Transaction.add_method('gncTransGetGUID', 'GetGUID')

Transaction.add_method('xaccTransGetDescription', 'GetDescription')
Transaction.add_method('xaccTransDestroy', 'Destroy')

trans_dict =    {
                    'GetSplit': Split,
                    'FindSplitByAccount': Split,
                    'Clone': Transaction,
                    'Reverse': Transaction,
                    'GetReversedBy': Transaction,
                    'GetImbalanceValue': GncNumeric,
                    'GetAccountValue': GncNumeric,
                    'GetAccountAmount': GncNumeric,
                    'GetAccountConvRate': GncNumeric,
                    'GetAccountBalance': GncNumeric,
                    'GetCurrency': GncCommodity,
                    'GetGUID': GUID
                }

methods_return_instance(Transaction, trans_dict)
methods_return_instance_lists(
    Transaction, { 'GetSplitList': Split,
                       })
Transaction.decorate_functions(
    decorate_monetary_list_returning_function, 'GetImbalance')

# Split
Split.add_methods_with_prefix('xaccSplit')
Split.add_method('gncSplitGetGUID', 'GetGUID')
Split.add_method('xaccSplitDestroy', 'Destroy')

split_dict =    {
                    'GetBook': Book,
                    'GetAccount': Account,
                    'GetParent': Transaction,
                    'Lookup': Split,
                    'GetOtherSplit': Split,
                    'GetAmount': GncNumeric,
                    'GetValue': GncNumeric,
                    'GetSharePrice': GncNumeric,
                    'ConvertAmount': GncNumeric,
                    'GetBaseValue': GncNumeric,
                    'GetBalance': GncNumeric,
                    'GetClearedBalance': GncNumeric,
                    'GetReconciledBalance': GncNumeric,
                    'VoidFormerAmount': GncNumeric,
                    'VoidFormerValue': GncNumeric,
                    'GetGUID': GUID
                }
methods_return_instance(Split, split_dict)

Split.account = property( Split.GetAccount, Split.SetAccount )
Split.parent = property( Split.GetParent, Split.SetParent )

# Account
Account.add_methods_with_prefix('xaccAccount')
Account.add_methods_with_prefix('gnc_account_')
Account.add_method('gncAccountGetGUID', 'GetGUID')
Account.add_method('xaccAccountGetPlaceholder', 'GetPlaceholder')

account_dict =  {
                    'get_book' : Book,
                    'Lookup' : Account,
                    'get_parent' : Account,
                    'get_root' : Account,
                    'nth_child' : Account,
                    'lookup_by_code' : Account,
                    'lookup_by_name' : Account,
                    'lookup_by_full_name' : Account,
                    'FindTransByDesc' : Transaction,
                    'FindSplitByDesc' : Split,
                    'GetBalance' : GncNumeric,
                    'GetClearedBalance' : GncNumeric,
                    'GetReconciledBalance' : GncNumeric,
                    'GetPresentBalance' : GncNumeric,
                    'GetProjectedMinimumBalance' : GncNumeric,
                    'GetBalanceAsOfDate' : GncNumeric,
                    'ConvertBalanceToCurrency' : GncNumeric,
                    'ConvertBalanceToCurrencyAsOfDate' : GncNumeric,
                    'GetBalanceInCurrency' : GncNumeric,
                    'GetClearedBalanceInCurrency' : GncNumeric,
                    'GetReconciledBalanceInCurrency' : GncNumeric,
                    'GetPresentBalanceInCurrency' : GncNumeric,
                    'GetProjectedMinimumBalanceInCurrency' : GncNumeric,
                    'GetBalanceAsOfDateInCurrency' : GncNumeric,
                    'GetBalanceChangeForPeriod' : GncNumeric,
                    'GetCommodity' : GncCommodity,
                    'GetGUID': GUID
                }
methods_return_instance(Account, account_dict)
methods_return_instance_lists(
    Account, { 'GetSplitList': Split,
               'get_children': Account,
               'get_children_sorted': Account,
               'get_descendants': Account,
               'get_descendants_sorted': Account
                       })
Account.name = property( Account.GetName, Account.SetName )

#GUID
GUID.add_methods_with_prefix('guid_')
GUID.add_method('xaccAccountLookup', 'AccountLookup')
GUID.add_method('xaccTransLookup', 'TransLookup')
GUID.add_method('xaccSplitLookup', 'SplitLookup')

## define addition methods for GUID object - do we need these
GUID.add_method('guid_to_string', 'to_string')
#GUID.add_method('string_to_guid', 'string_to_guid')

guid_dict = {
                'copy' : GUID,
                'TransLookup': Transaction,
                'AccountLookup': Account,
                'SplitLookup': Split
            }
methods_return_instance(GUID, guid_dict)

#GUIDString
class GUIDString(GnuCashCoreClass):
    pass

GUIDString.add_constructor_and_methods_with_prefix('string_', 'to_guid')

#Query
from gnucash.gnucash_core_c import \
    QOF_QUERY_AND, \
    QOF_QUERY_OR, \
    QOF_QUERY_NAND, \
    QOF_QUERY_NOR, \
    QOF_QUERY_XOR

from gnucash.gnucash_core_c import \
    QOF_STRING_MATCH_NORMAL, \
    QOF_STRING_MATCH_CASEINSENSITIVE

from gnucash.gnucash_core_c import \
    QOF_COMPARE_LT, \
    QOF_COMPARE_LTE, \
    QOF_COMPARE_EQUAL, \
    QOF_COMPARE_GT, \
    QOF_COMPARE_GTE, \
    QOF_COMPARE_NEQ, \
    QOF_COMPARE_CONTAINS, \
    QOF_COMPARE_NCONTAINS

from gnucash.gnucash_core_c import \
    QOF_DATE_MATCH_NORMAL, \
    QOF_DATE_MATCH_DAY

from gnucash.gnucash_core_c import \
    QOF_NUMERIC_MATCH_DEBIT, \
    QOF_NUMERIC_MATCH_CREDIT, \
    QOF_NUMERIC_MATCH_ANY

from gnucash.gnucash_core_c import \
    QOF_GUID_MATCH_ANY, \
    QOF_GUID_MATCH_NONE, \
    QOF_GUID_MATCH_NULL, \
    QOF_GUID_MATCH_ALL, \
    QOF_GUID_MATCH_LIST_ANY

from gnucash.gnucash_core_c import \
    QOF_CHAR_MATCH_ANY, \
    QOF_CHAR_MATCH_NONE

from gnucash.gnucash_core_c import \
    INVOICE_TYPE

from gnucash.gnucash_core_c import \
    INVOICE_IS_PAID

class Query(GnuCashCoreClass):

    def search_for(self, obj_type):
        """Set search_for to obj_type

        calls qof_query_search_for. Buffers search string for queries lifetime.
        @see https://bugs.gnucash.org/show_bug.cgi?id=796137"""
        self.__search_for_buf = obj_type
        self._search_for(self.__search_for_buf)

Query.add_constructor_and_methods_with_prefix('qof_query_', 'create', exclude=["qof_query_search_for"])

Query.add_method('qof_query_set_book', 'set_book')
Query.add_method('qof_query_search_for', '_search_for')
Query.add_method('qof_query_run', 'run')
Query.add_method('qof_query_add_term', 'add_term')
Query.add_method('qof_query_add_boolean_match', 'add_boolean_match')
Query.add_method('qof_query_add_guid_list_match', 'add_guid_list_match')
Query.add_method('qof_query_add_guid_match', 'add_guid_match')
Query.add_method('qof_query_destroy', 'destroy')

class QueryStringPredicate(GnuCashCoreClass):
    pass

QueryStringPredicate.add_constructor_and_methods_with_prefix(
    'qof_query_','string_predicate')

class QueryBooleanPredicate(GnuCashCoreClass):
    pass

QueryBooleanPredicate.add_constructor_and_methods_with_prefix(
    'qof_query_', 'boolean_predicate')

class QueryInt32Predicate(GnuCashCoreClass):
    pass

QueryInt32Predicate.add_constructor_and_methods_with_prefix(
    'qof_query_', 'int32_predicate')

class QueryDatePredicate(GnuCashCoreClass):
    pass

QueryDatePredicate.add_constructor_and_methods_with_prefix(
    'qof_query_', 'date_predicate', exclude=["qof_query_date_predicate_get_date"])
QueryDatePredicate.add_method('qof_query_date_predicate_get_date', 'get_date')

class QueryGuidPredicate(GnuCashCoreClass):
    pass

QueryGuidPredicate.add_constructor_and_methods_with_prefix(
    'qof_query_', 'guid_predicate')

class QueryNumericPredicate(GnuCashCoreClass):
    pass

QueryNumericPredicate.add_constructor_and_methods_with_prefix(
    'qof_query_', 'numeric_predicate')
