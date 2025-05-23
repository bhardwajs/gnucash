/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

%module sw_engine
%{
/* Includes the header in the wrapper code */
#include <glib.h>

#include <config.h>
#include "qof.h"
#include "qoflog.h"
#include "Query.h"
#include "gnc-budget.h"
#include "gnc-commodity.h"
#include "gnc-engine.h"
#include "gnc-filepath-utils.h"
#include "gnc-pricedb.h"
#include "gnc-lot.h"
#include "gnc-session.h"
#include "engine-helpers.h"
#include "gnc-engine-guile.h"
#include "policy.h"
#include "SX-book.h"
#include "gnc-kvp-guile.h"
#include "glib-guile.h"

#include "Account.hpp"
#include "gncAddress.h"
#include "gncBillTerm.h"
#include "gncCustomer.h"
#include "gncEmployee.h"
#include "gncEntry.h"
#include "gncInvoice.h"
#include "gncJob.h"
#include "gncOrder.h"
#include "gncOwner.h"
#include "gncTaxTable.h"
#include "gncVendor.h"

#include <numeric>
#include <unordered_set>
%}
#if defined(SWIGGUILE) //Always C++
%{

using SplitsVec = std::vector<Split*>;
using AccountVec = std::vector<Account*>;

SplitsVec gnc_get_match_commodity_splits (AccountVec accounts, bool use_end_date,
                                          time64 end_date, gnc_commodity *comm, bool sort);

/* scans account splits, in posted date order, calling split_fn(split)
   at each split and accumulate the result at each date in the SCM
   list dates into a new SCM list. Parameters are:

   acc            the account
   dates          the SCM list of posted dates, assumed to be in chronological order
   init           the result to be pushed into the result for dates prior to first split date
   split_fn       the split->elt procedure whose result of (split_fn split) will be pushed
                  into the returned SCM list */
SCM gnc_account_accumulate_to_dates (const Account *acc, SCM dates,
                                     SCM split_fn, SCM init);

/* as above, but the split_to_date function to use a different date order. */
SCM gnc_account_accumulate_to_dates (const Account *acc, SCM dates,
                                     SCM split_fn, SCM init, SCM split_to_date);

AccountVec gnc_accounts_and_all_descendants (AccountVec accounts);

extern "C"
{
SCM scm_init_sw_engine_module (void);
}
%}
%include "gnc-optiondb.i"
#endif

%import "base-typemaps.i"
%apply struct tm* { tm* };
GLIST_HELPER_INOUT(SplitList, SWIGTYPE_p_Split);
GLIST_HELPER_INOUT(TransList, SWIGTYPE_p_Transaction);
GLIST_HELPER_INOUT(LotList, SWIGTYPE_p_GNCLot);
GLIST_HELPER_INOUT(AccountList, SWIGTYPE_p_Account);
GLIST_HELPER_INOUT(PriceList, SWIGTYPE_p_GNCPrice);
// TODO: free PriceList?
GLIST_HELPER_INOUT(CommodityList, SWIGTYPE_p_gnc_commodity);
VECTOR_HELPER_INOUT(SplitsVec, SWIGTYPE_p_Split, Split);
VECTORREF_HELPER_INOUT(SplitsVec&, SWIGTYPE_p_Split, Split);
VECTOR_HELPER_INOUT(AccountVec, SWIGTYPE_p_Account, Account);

%typemap(newfree) char * "g_free($1);"

/* These need to be here so that they are *before* the function
declarations in the header files, some of which are included by
engine-common.i */

%newobject gnc_account_get_full_name;
%newobject xaccTransGetAPARAcctSplitList;
%newobject xaccTransGetPaymentAcctSplitList;
%newobject xaccAccountGetSplitList;

%include "engine-common.i"

#if defined(SWIGGUILE)
%ignore QofLogModule;
%typemap(in) QofLogModule {
     $1 = (const char *)SWIG_scm2str($input);
 }

%typemap(freearg) QofLogModule {
    SWIG_free((char*)$1);
 }

#endif

%include "qoflog.h"

%inline %{
static void gnc_log_warn(const char *msg)
{ g_log("gnc.scm", G_LOG_LEVEL_WARNING, "%s", msg); }
static void gnc_log_error(const char *msg)
{ g_log("gnc.scm", G_LOG_LEVEL_CRITICAL, "%s", msg); }
static void gnc_log_msg(const char *msg)
{ g_log("gnc.scm", G_LOG_LEVEL_MESSAGE, "%s", msg); }
static void gnc_log_debug(const char *msg)
{ g_log("gnc.scm", G_LOG_LEVEL_DEBUG, "%s", msg); }

static const GncGUID * gncPriceGetGUID(GNCPrice *x)
{ return qof_instance_get_guid(QOF_INSTANCE(x)); }
static const GncGUID * gncBudgetGetGUID(GncBudget *x)
{ return qof_instance_get_guid(QOF_INSTANCE(x)); }

SplitsVec gnc_get_match_commodity_splits (AccountVec accounts, bool use_end_date,
                                          time64 end_date, gnc_commodity *comm, bool sort)
{
    SplitsVec rv;

    auto maybe_accumulate = [&rv, comm](auto s)
    {
        auto txn_comm{xaccTransGetCurrency (xaccSplitGetParent (s))};
        auto acc_comm{xaccAccountGetCommodity (xaccSplitGetAccount (s))};
        if ((xaccSplitGetReconcile (s) != VREC) &&
            (txn_comm != acc_comm) &&
            (!comm || comm == txn_comm || comm == acc_comm))
            rv.push_back (s);
    };

    std::function<void(Account*)> scan_account;
    if (use_end_date)
        scan_account = [end_date, maybe_accumulate](auto acc)
            { gnc_account_foreach_split_until_date (acc, end_date, maybe_accumulate); };
    else
        scan_account = [maybe_accumulate](auto acc)
            { gnc_account_foreach_split (acc, maybe_accumulate, false); };

    std::for_each (accounts.begin(), accounts.end(), scan_account);
    if (sort)
        std::sort (rv.begin(), rv.end(), [](auto a, auto b){ return xaccSplitOrder (a, b) < 0; });
    return rv;
}

static SCM
accumulate_splits_by_dates (const SplitsVec& splits, SCM dates, SCM result,
                            std::function<bool(Split*,SCM)> no_later_than_date,
                            std::function<SCM(Split*)> get_result)
{
    SCM rv = SCM_EOL;
    auto splits_it = splits.begin();
    for (; !scm_is_null(dates); dates = scm_cdr (dates))
    {
        while (splits_it != splits.end() && no_later_than_date (*splits_it, scm_car (dates)))
            result = get_result (*splits_it++);

        rv = scm_cons (result, rv);
    }
    return scm_reverse_x (rv, SCM_EOL);
}

SCM
gnc_account_accumulate_to_dates (const Account *acc, SCM dates,
                                 SCM split_fn, SCM init)
{
    const auto& splits = xaccAccountGetSplits(acc);
    auto get_result = [&](Split* s) -> SCM { return scm_call_1(split_fn, gnc_split_to_scm(s)); };
    auto no_later_than_date = [&](Split* s, SCM date) -> bool
        { return xaccTransGetDate (xaccSplitGetParent (s)) <= scm_to_int64 (date); };

    return accumulate_splits_by_dates (splits, dates, init, no_later_than_date, get_result);
}

SCM
gnc_account_accumulate_to_dates (const Account *acc, SCM dates,
                                 SCM split_fn, SCM init, SCM split_to_date)
{
    auto splits = xaccAccountGetSplits(acc);
    auto less_scm = [](SCM a, SCM b) -> bool { return scm_is_true(scm_less_p(a, b)); };
    auto get_date = [&](Split* s) -> SCM { return scm_call_1(split_to_date, gnc_split_to_scm(s)); };
    auto get_result = [&](Split* s) -> SCM { return scm_call_1(split_fn, gnc_split_to_scm(s)); };
    auto no_later_than_date = [&](auto s, SCM date) -> bool { return !less_scm(date, get_date(s)); };
    std::sort(splits.begin(), splits.end(), [&](auto a, auto b) -> bool
              { return less_scm(get_date(a), get_date(b)); });

    return accumulate_splits_by_dates (splits, dates, init, no_later_than_date, get_result);
}


using AccountSet = std::unordered_set<Account*>;
static void maybe_add_descendants (Account* acc, AccountSet* accset)
{
    if (accset->insert (acc).second)
        gnc_account_foreach_child (acc, (AccountCb)maybe_add_descendants, accset);
};

AccountVec
gnc_accounts_and_all_descendants (AccountVec accounts)
{
    AccountSet accset;
    for (auto a : accounts)
        maybe_add_descendants (a, &accset);
    return AccountVec (accset.begin(), accset.end());
}

%}

/* NB: The object ownership annotations should already cover all the
functions currently used in guile, but not all the functions that are
wrapped.  So, we should contract the interface to wrap only the used
functions. */

%delobject gnc_price_list_destroy;
%newobject gnc_pricedb_lookup_latest_any_currency;

%newobject gnc_pricedb_lookup_nearest_in_time_any_currency;
%newobject gnc_pricedb_lookup_nearest_in_time_any_currency_t64;
%newobject gnc_pricedb_lookup_latest_before_any_currency;
%newobject gnc_pricedb_lookup_latest_before_any_currency_t64;
%newobject gnc_pricedb_get_prices;
%newobject gnc_pricedb_lookup_at_time;
%newobject gnc_pricedb_lookup_day;
%newobject gnc_pricedb_lookup_day_t64;

%newobject xaccQueryGetSplitsUniqueTrans;
%newobject xaccQueryGetTransactions;
%newobject xaccQueryGetLots;

%newobject xaccSplitGetCorrAccountFullName;
%newobject gnc_numeric_to_string;

%newobject gnc_localtime;
%newobject gnc_gmtime;

/* Parse the header file to generate wrappers */
%inline {
  static QofIdType QOF_ID_BOOK_SCM (void) { return QOF_ID_BOOK; }
}

/* Allow '#f' in guile to be used to represent NULL in 'C' for functions *
 * 'gnc_set_num_action', 'gnc_get_num_action' and 'gnc_get_action_num' in *
 * 'engine-helpers.c' */
%typemap(in) Transaction *trans {
  if ($input == SCM_BOOL_F)
    $1 = NULL;
  else
    $1 = (Transaction *)SWIG_MustGetPtr($input, SWIGTYPE_p_Transaction, 1, 0);
}

%typemap(in) Split *split {
  if ($input == SCM_BOOL_F)
    $1 = NULL;
  else
    $1 = (Split *)SWIG_MustGetPtr($input, SWIGTYPE_p_Split, 2, 0);
}

%typemap(in) char * num (int must_free = 0) {
  if ($input == SCM_BOOL_F)
    $1 = NULL;
  else
  {
    $1 = (char *)SWIG_scm2str($input);
    must_free3 = 1;
  }
}

%typemap(in) char * action (int must_free = 0) {
  if ($input == SCM_BOOL_F)
    $1 = NULL;
  else
  {
    $1 = (char *)SWIG_scm2str($input);
    must_free4 = 1;
  }
}

%include <engine-helpers.h>
%include <gnc-engine-guile.h>
%typemap(in) Transaction *trans;
%typemap(in) Split *split;
%typemap(in) char * num;
%typemap(in) char * action;

%include <policy.h>
%include <gnc-pricedb.h>

QofSession * qof_session_new (QofBook* book);
QofBook * qof_session_get_book (QofSession *session);
// TODO: Unroll/remove
const char *qof_session_get_url (QofSession *session);

%ignore qof_print_date_time_buff;
%ignore gnc_tm_free;
%newobject qof_print_date;
%newobject gnc_ctime;
%newobject gnc_print_time64;
%include <gnc-date.h>
extern const char *gnc_default_strftime_date_format;

GncGUID guid_new_return(void);

%inline {
static QofQuery * qof_query_create_for_splits(void) {
  return qof_query_create_for(GNC_ID_SPLIT);
}
}

SplitList * qof_query_run (QofQuery *q);
SplitList * qof_query_last_run (QofQuery *q);
SplitList * qof_query_run_subquery (QofQuery *q, const QofQuery *q);

%typemap(in) QofQueryParamList * "$1 = gnc_query_scm2path($input);"

%include <gnc-session.h>
%include <Query.h>
%ignore qof_query_run;
%ignore qof_query_last_run;
%ignore qof_query_run_subquery;
%include <qofquery.h>
%include <qofquerycore.h>
%include <qofbookslots.h>
%include <qofbook.h>

%ignore GNC_DENOM_AUTO;
%ignore GNCNumericErrorCodes;
%ignore GNC_ERROR_OK;
%ignore GNC_ERROR_ARG;
%ignore GNC_ERROR_OVERFLOW;
%ignore GNC_ERROR_DENOM_DIFF;
%ignore GNC_ERROR_REMAINDER;
%include <gnc-numeric.h>

time64 time64CanonicalDayTime(time64 t);

%include <gnc-budget.h>
%typemap (freearg) GList * "g_list_free_full ($1, g_free);"

%include <gnc-commodity.h>

void gnc_hook_add_scm_dangler (const gchar *name, SCM proc);
void gnc_hook_run (const gchar *name, gpointer data);
%include <gnc-hooks.h>

Account * gnc_book_get_template_root(QofBook *book);

// KVP stuff
%typemap(in) KvpValue * " $1 = gnc_scm_to_kvp_value_ptr($input); "
%typemap(out) KvpValue * " $result = gnc_kvp_value_ptr_to_scm($1); "
%typemap(in) GSList *key_path " $1 = gnc_scm_to_gslist_string($input);"
%typemap(freearg) GSList *key_path "g_slist_free_full ($1, g_free);"

const SplitsVec& xaccAccountGetSplits (const Account*);

QofBook* qof_book_new (void);
void qof_book_options_delete (QofBook *book, GSList *key_path);
void qof_book_set_option (QofBook *book, KvpValue *new_value, GSList *key_path);
KvpValue* qof_book_get_option (QofBook *book, GSList *key_path);

%clear GSList *key_path;

const char* qof_book_get_string_option(const QofBook* book, const char* opt_name);
void qof_book_set_string_option(QofBook* book, const char* opt_name, const char* opt_val);

#if defined(SWIGGUILE)
%init {
  {
    char tmp[100];
    char * no_args[1] = { NULL };

#define SET_ENUM(e) snprintf(tmp, 100, "(set! %s (%s))", (e), (e));  \
    scm_c_eval_string(tmp);

    SET_ENUM("TXN-TYPE-NONE");
    SET_ENUM("TXN-TYPE-INVOICE");
    SET_ENUM("TXN-TYPE-PAYMENT");
    SET_ENUM("TXN-TYPE-LINK");

    SET_ENUM("ACCT-TYPE-INVALID");
    SET_ENUM("ACCT-TYPE-NONE");
    SET_ENUM("ACCT-TYPE-BANK");
    SET_ENUM("ACCT-TYPE-CASH");
    SET_ENUM("ACCT-TYPE-CREDIT");
    SET_ENUM("ACCT-TYPE-ASSET");
    SET_ENUM("ACCT-TYPE-LIABILITY");
    SET_ENUM("ACCT-TYPE-STOCK");
    SET_ENUM("ACCT-TYPE-MUTUAL");
    SET_ENUM("ACCT-TYPE-CURRENCY");
    SET_ENUM("ACCT-TYPE-INCOME");
    SET_ENUM("ACCT-TYPE-EXPENSE");
    SET_ENUM("ACCT-TYPE-EQUITY");
    SET_ENUM("ACCT-TYPE-RECEIVABLE");
    SET_ENUM("ACCT-TYPE-PAYABLE");
    SET_ENUM("ACCT-TYPE-ROOT");
    SET_ENUM("ACCT-TYPE-TRADING");
    SET_ENUM("NUM-ACCOUNT-TYPES");
    SET_ENUM("ACCT-TYPE-CHECKING");
    SET_ENUM("ACCT-TYPE-SAVINGS");
    SET_ENUM("ACCT-TYPE-MONEYMRKT");
    SET_ENUM("ACCT-TYPE-CREDITLINE");

    SET_ENUM("QOF-QUERY-AND");
    SET_ENUM("QOF-QUERY-OR");

    SET_ENUM("QUERY-TXN-MATCH-ALL");
    SET_ENUM("QUERY-TXN-MATCH-ANY");

    SET_ENUM("QOF-GUID-MATCH-ALL");
    SET_ENUM("QOF-GUID-MATCH-ANY");
    SET_ENUM("QOF-GUID-MATCH-NULL");
    SET_ENUM("QOF-GUID-MATCH-NONE");
    SET_ENUM("QOF-GUID-MATCH-LIST-ANY");

    SET_ENUM("QOF-COMPARE-LT");
    SET_ENUM("QOF-COMPARE-LTE");
    SET_ENUM("QOF-COMPARE-EQUAL");
    SET_ENUM("QOF-COMPARE-GT");
    SET_ENUM("QOF-COMPARE-GTE");
    SET_ENUM("QOF-COMPARE-NEQ");
    SET_ENUM("QOF-COMPARE-CONTAINS");
    SET_ENUM("QOF-COMPARE-NCONTAINS");

    SET_ENUM("QOF-LOG-DEBUG");
    SET_ENUM("QOF-LOG-FATAL");
    SET_ENUM("QOF-LOG-ERROR");
    SET_ENUM("QOF-LOG-WARNING");
    SET_ENUM("QOF-LOG-MESSAGE");
    SET_ENUM("QOF-LOG-INFO");

    SET_ENUM("QOF-NUMERIC-MATCH-ANY");
    SET_ENUM("QOF-NUMERIC-MATCH-CREDIT");
    SET_ENUM("QOF-NUMERIC-MATCH-DEBIT");

    SET_ENUM("QOF-PARAM-BOOK");
    SET_ENUM("QOF-PARAM-GUID");
    SET_ENUM("QOF-PARAM-KVP");
    SET_ENUM("QOF-PARAM-ACTIVE");
    SET_ENUM("QOF-PARAM-VERSION");

    SET_ENUM("CLEARED-NO");
    SET_ENUM("CLEARED-CLEARED");
    SET_ENUM("CLEARED-FROZEN");
    SET_ENUM("CLEARED-RECONCILED");
    SET_ENUM("CLEARED-VOIDED");
    SET_ENUM("CLEARED-ALL");

    SET_ENUM("HOOK-REPORT");
    SET_ENUM("HOOK-SAVE-OPTIONS");

    //SET_ENUM("GNC-ID-ACCOUNT");
    SET_ENUM("QOF-ID-BOOK-SCM");
    //SET_ENUM("GNC-ID-BUDGET");
    //SET_ENUM("GNC-ID-LOT");
    //SET_ENUM("GNC-ID-PRICE");
    //SET_ENUM("GNC-ID-SPLIT");
    //SET_ENUM("GNC-ID-SCHEDXACTION");
    //SET_ENUM("QOF-ID-SESSION");
    //SET_ENUM("GNC-ID-TRANS");

    SET_ENUM("QUERY-DEFAULT-SORT");
    SET_ENUM("SPLIT-LOT");
    SET_ENUM("SPLIT-TRANS");
    SET_ENUM("SPLIT-ACCOUNT");
    SET_ENUM("SPLIT-VALUE");
    SET_ENUM("SPLIT-MEMO");
    SET_ENUM("SPLIT-ACTION");
    SET_ENUM("SPLIT-DATE-RECONCILED");
    SET_ENUM("SPLIT-ACCT-FULLNAME");
    SET_ENUM("SPLIT-CORR-ACCT-NAME");
    SET_ENUM("SPLIT-CORR-ACCT-CODE");

    SET_ENUM("TRANS-DATE-POSTED");
    SET_ENUM("TRANS-DESCRIPTION");
    SET_ENUM("TRANS-NUM");

    SET_ENUM("KVP-OPTION-PATH");

    SET_ENUM("OPTION-SECTION-ACCOUNTS");
    SET_ENUM("OPTION-NAME-TRADING-ACCOUNTS");
    SET_ENUM("OPTION-NAME-AUTO-READONLY-DAYS");
    SET_ENUM("OPTION-NAME-NUM-FIELD-SOURCE");

    SET_ENUM("OPTION-SECTION-BUDGETING");
    SET_ENUM("OPTION-NAME-DEFAULT-BUDGET");

    SET_ENUM("ACCOUNT-CODE-");  /* sic */

    SET_ENUM("GNC-HOW-RND-CEIL");
    SET_ENUM("GNC-HOW-RND-TRUNC");
    SET_ENUM("GNC-HOW-RND-PROMOTE");
    SET_ENUM("GNC-HOW-RND-ROUND-HALF-DOWN");
    SET_ENUM("GNC-HOW-RND-ROUND-HALF-UP");
    SET_ENUM("GNC-HOW-RND-ROUND");
    SET_ENUM("GNC-HOW-RND-NEVER");

    SET_ENUM("PRICE-SOURCE-EDIT-DLG");
    SET_ENUM("PRICE-SOURCE-FQ");
    SET_ENUM("PRICE-SOURCE-USER-PRICE");
    SET_ENUM("PRICE-SOURCE-XFER-DLG-VAL");
    SET_ENUM("PRICE-SOURCE-SPLIT-REG");
    SET_ENUM("PRICE-SOURCE-SPLIT-IMPORT");
    SET_ENUM("PRICE-SOURCE-STOCK-SPLIT");
    SET_ENUM("PRICE-SOURCE-TEMP");
    SET_ENUM("PRICE-SOURCE-INVALID");

    SET_ENUM("QOF-DATE-FORMAT-US");
    SET_ENUM("QOF-DATE-FORMAT-UK");
    SET_ENUM("QOF-DATE-FORMAT-CE");
    SET_ENUM("QOF-DATE-FORMAT-ISO");
    SET_ENUM("QOF-DATE-FORMAT-LOCALE");
    SET_ENUM("QOF-DATE-FORMAT-UTC");
    SET_ENUM("QOF-DATE-FORMAT-CUSTOM");


#undef SET_ENUM

    gnc_engine_init(0, no_args);
  }

}
#endif

%include business-core.i

%typemap(in) GList * {
    SCM path_scm = $input;
    GList *path = NULL;
    while (!scm_is_null (path_scm))
    {
        SCM key_scm = SCM_CAR (path_scm);
        char *key;
        if (!scm_is_string (key_scm))
            break;
        key = scm_to_locale_string (key_scm);
        path = g_list_prepend (path, key);
        path_scm = SCM_CDR (path_scm);
    }
    $1 = g_list_reverse (path);
}
