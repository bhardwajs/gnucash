/**********************************************************************
 * gnc-plugin-page-register.c -- register page functions              *
 *                                                                    *
 * Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>       *
 * Copyright (C) 2003,2005,2006 David Hampton <hampton@employees.org> *
 * Copyright (C) 2011, Robert Fewell                                  *
 *                                                                    *
 * This program is free software; you can redistribute it and/or      *
 * modify it under the terms of the GNU General Public License as     *
 * published by the Free Software Foundation; either version 2 of     *
 * the License, or (at your option) any later version.                *
 *                                                                    *
 * This program is distributed in the hope that it will be useful,    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 * GNU General Public License for more details.                       *
 *                                                                    *
 * You should have received a copy of the GNU General Public License  *
 * along with this program; if not, contact:                          *
 *                                                                    *
 * Free Software Foundation           Voice:  +1-617-542-5942         *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652         *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                     *
 **********************************************************************/

/** @addtogroup ContentPlugins
    @{ */
/** @addtogroup RegisterPlugin Register Page
    @{ */
/** @file gnc-plugin-page-register.c
    @brief  Functions providing a register page for the GnuCash UI
    @author Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
    @author Copyright (C) 2003,2005 David Hampton <hampton@employees.org>
*/

#include <config.h>

#include <optional>

#include <libguile.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "swig-runtime.h"
#include "guile-mappings.h"

#include "gnc-plugin-page-register.h"
#include "gnc-plugin-register.h"
#include "gnc-plugin-menu-additions.h"
#include "gnc-plugin-page-report.h"
#include "gnc-plugin-business.h"

#include "dialog-account.h"
#include "dialog-dup-trans.h"
#include "dialog-find-account.h"
#include "dialog-find-transactions.h"
#include "dialog-print-check.h"
#include "dialog-invoice.h"
#include "dialog-transfer.h"
#include "dialog-utils.h"
#include "assistant-stock-split.h"
#include "assistant-stock-transaction.h"
#include "gnc-component-manager.h"
#include "gnc-date.h"
#include "gnc-date-edit.h"
#include "gnc-engine.h"
#include "gnc-event.h"
#include "gnc-features.h"
#include "gnc-glib-utils.h"
#include "gnc-gnome-utils.h"
#include "gnc-gobject-utils.h"
#include "gnc-gui-query.h"
#include "gnc-icons.h"
#include "gnc-split-reg.h"
#include "gnc-state.h"
#include "gnc-prefs.h"
#include "gnc-ui-util.h"
#include "gnc-window.h"
#include "gnc-main-window.h"
#include "gnc-session.h"
#include "gnc-ui.h"
#include "gnc-warnings.h"
#include "gnucash-sheet.h"
#include "dialog-lot-viewer.h"
#include "Scrub.h"
#include "ScrubBusiness.h"
#include "qof.h"
#include "window-reconcile.h"
#include "window-autoclear.h"
#include "window-report.h"
#include "engine-helpers.h"
#include "qofbookslots.h"
#include "gnc-gtk-utils.h"

/* gschema: org.gnucash.GnuCash.general.register.JumpMultipleSplits */
typedef enum : gint
{
    JUMP_DEFAULT = 0, /* Do nothing */
    JUMP_LARGEST_VALUE_FIRST_SPLIT = 1,
    JUMP_SMALLEST_VALUE_FIRST_SPLIT = 2,
} GncPrefJumpMultSplits;

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

#define DEFAULT_LINES_AMOUNT         50
#define DEFAULT_FILTER_NUM_DAYS_GL  "30"

static void gnc_plugin_page_register_finalize (GObject* object);

/* static Account *gnc_plugin_page_register_get_current_account (GncPluginPageRegister *page); */

static GtkWidget* gnc_plugin_page_register_create_widget (
    GncPluginPage* plugin_page);
static void gnc_plugin_page_register_destroy_widget (GncPluginPage*
                                                     plugin_page);
static void gnc_plugin_page_register_window_changed (GncPluginPage*
                                                     plugin_page, GtkWidget* window);
static gboolean gnc_plugin_page_register_focus_widget (GncPluginPage*
                                                       plugin_page);
static void gnc_plugin_page_register_focus (GncPluginPage* plugin_page,
                                            gboolean current_page);
static void gnc_plugin_page_register_save_page (GncPluginPage* plugin_page,
                                                GKeyFile* file, const gchar* group);
static GncPluginPage* gnc_plugin_page_register_recreate_page (
    GtkWidget* window, GKeyFile* file, const gchar* group);
static void gnc_plugin_page_register_update_edit_menu (GncPluginPage* page,
                                                       gboolean hide);
static gboolean gnc_plugin_page_register_finish_pending (GncPluginPage* page);

static gchar* gnc_plugin_page_register_get_tab_name (GncPluginPage*
                                                     plugin_page);
static gchar* gnc_plugin_page_register_get_tab_color (GncPluginPage*
                                                      plugin_page);
static gchar* gnc_plugin_page_register_get_long_name (GncPluginPage*
                                                      plugin_page);

static void gnc_plugin_page_register_summarybar_position_changed (
    gpointer prefs, gchar* pref, gpointer user_data);

extern "C"
{
/* Callbacks for the "Sort By" dialog */
void gnc_plugin_page_register_sort_button_cb (GtkToggleButton* button,
                                              GncPluginPageRegister* page);
void gnc_plugin_page_register_sort_response_cb (GtkDialog* dialog,
                                                gint response, GncPluginPageRegister* plugin_page);
void gnc_plugin_page_register_sort_order_save_cb (GtkToggleButton* button,
                                                  GncPluginPageRegister* page);
void gnc_plugin_page_register_sort_order_reverse_cb (GtkToggleButton* button,
                                                     GncPluginPageRegister* page);
}

static gchar* gnc_plugin_page_register_get_sort_order (GncPluginPage*
                                                       plugin_page);
void gnc_plugin_page_register_set_sort_order (GncPluginPage* plugin_page,
                                              const gchar* sort_order);
static gboolean gnc_plugin_page_register_get_sort_reversed (
    GncPluginPage* plugin_page);
void gnc_plugin_page_register_set_sort_reversed (GncPluginPage* plugin_page,
                                                 gboolean reverse_order);

extern "C"
{
/* Callbacks for the "Filter By" dialog */
void gnc_plugin_page_register_filter_select_range_cb (GtkRadioButton* button,
                                                      GncPluginPageRegister* page);
void gnc_plugin_page_register_filter_start_cb (GtkWidget* radio,
                                               GncPluginPageRegister* page);
void gnc_plugin_page_register_filter_end_cb (GtkWidget* radio,
                                             GncPluginPageRegister* page);
void gnc_plugin_page_register_filter_response_cb (GtkDialog* dialog,
                                                  gint response, GncPluginPageRegister* plugin_page);
void gnc_plugin_page_register_filter_status_select_all_cb (GtkButton* button,
                                                           GncPluginPageRegister* plugin_page);
void gnc_plugin_page_register_filter_status_clear_all_cb (GtkButton* button,
                                                          GncPluginPageRegister* plugin_page);
void gnc_plugin_page_register_filter_status_one_cb (GtkToggleButton* button,
                                                    GncPluginPageRegister* page);
void gnc_plugin_page_register_filter_save_cb (GtkToggleButton* button,
                                              GncPluginPageRegister* page);
void gnc_plugin_page_register_filter_days_changed_cb (GtkSpinButton* button,
                                                      GncPluginPageRegister* page);
}

static time64 gnc_plugin_page_register_filter_dmy2time (char* date_string);
static gchar* gnc_plugin_page_register_filter_time2dmy (time64 raw_time);
static gchar* gnc_plugin_page_register_get_filter (GncPluginPage* plugin_page);
void gnc_plugin_page_register_set_filter (GncPluginPage* plugin_page,
                                          const gchar* filter);
static void gnc_plugin_page_register_set_filter_tooltip (GncPluginPageRegister* page);

static void gnc_ppr_update_status_query (GncPluginPageRegister* page);
static void gnc_ppr_update_date_query (GncPluginPageRegister* page);

/* Command callbacks */
static void gnc_plugin_page_register_cmd_print_check (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_cut (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_copy (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_paste (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_edit_account (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_find_account (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_find_transactions (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_edit_tax_options (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_cut_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_copy_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_paste_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_void_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_unvoid_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_reverse_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_view_sort_by (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_view_filter_by (GSimpleAction *simple, GVariant *paramter, gpointer user_data);

static void gnc_plugin_page_register_cmd_style_changed (GSimpleAction *simple, GVariant *parameter, gpointer user_data);
static void gnc_plugin_page_register_cmd_style_double_line (GSimpleAction *simple, GVariant *parameter, gpointer user_data);
static void gnc_plugin_page_register_cmd_expand_transaction (GSimpleAction *simple, GVariant *parameter, gpointer user_data);

static void gnc_plugin_page_register_cmd_reconcile (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_stock_assistant (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_autoclear (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_transfer (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_stock_split (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_lots (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_enter_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_cancel_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_delete_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_blank_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_goto_date (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_duplicate_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_reinitialize_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_exchange_rate (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_jump (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_reload (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_schedule (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_scrub_all (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_scrub_current (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_account_report (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_transaction_report (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_linked_transaction (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_linked_transaction_open (GSimpleAction *simple, GVariant *paramter, gpointer user_data);
static void gnc_plugin_page_register_cmd_jump_linked_invoice (GSimpleAction *simple, GVariant *paramter, gpointer user_data);

static void gnc_plugin_page_help_changed_cb (GNCSplitReg* gsr,
                                             GncPluginPageRegister* register_page);
static void gnc_plugin_page_popup_menu_cb (GNCSplitReg* gsr,
                                           GncPluginPageRegister* register_page);
static void gnc_plugin_page_register_refresh_cb (GHashTable* changes,
                                                 gpointer user_data);
static void gnc_plugin_page_register_close_cb (gpointer user_data);

static void gnc_plugin_page_register_ui_update (gpointer various,
                                                GncPluginPageRegister* page);
static void gppr_account_destroy_cb (Account* account);
static void gnc_plugin_page_register_event_handler (QofInstance* entity,
                                                    QofEventId event_type,
                                                    GncPluginPageRegister* page,
                                                    GncEventData* ed);

static GncInvoice* invoice_from_split (Split* split);

/************************************************************/
/*                          Actions                         */
/************************************************************/

#define CUT_TRANSACTION_LABEL            N_("Cu_t Transaction")
#define COPY_TRANSACTION_LABEL           N_("_Copy Transaction")
#define PASTE_TRANSACTION_LABEL          N_("_Paste Transaction")
#define DUPLICATE_TRANSACTION_LABEL      N_("Dup_licate Transaction")
#define DELETE_TRANSACTION_LABEL         N_("_Delete Transaction")
/* Translators: This is a menu item that opens a dialog for linking an
   external file or URL with the bill, invoice, transaction, or voucher or
   removing such an link. */
#define LINK_TRANSACTION_LABEL           N_("_Manage Document Link…")
/* Translators: This is a menu item that opens an external file or URI that may
   be linked to the current bill, invoice, transaction, or voucher using
   the operating system's default application for the file or URI mime type. */
#define LINK_TRANSACTION_OPEN_LABEL      N_("_Open Linked Document")
/* Translators: This is a menu item that will open the bill, invoice, or voucher
   that is posted to the current transaction if there is one. */
#define JUMP_LINKED_INVOICE_LABEL        N_("Jump to Invoice")
#define CUT_SPLIT_LABEL                  N_("Cu_t Split")
#define COPY_SPLIT_LABEL                 N_("_Copy Split")
#define PASTE_SPLIT_LABEL                N_("_Paste Split")
#define DUPLICATE_SPLIT_LABEL            N_("Dup_licate Split")
#define DELETE_SPLIT_LABEL               N_("_Delete Split")
#define CUT_TRANSACTION_TIP              N_("Cut the selected transaction into clipboard")
#define COPY_TRANSACTION_TIP             N_("Copy the selected transaction into clipboard")
#define PASTE_TRANSACTION_TIP            N_("Paste the transaction from the clipboard")
#define DUPLICATE_TRANSACTION_TIP        N_("Make a copy of the current transaction")
#define DELETE_TRANSACTION_TIP           N_("Delete the current transaction")
#define LINK_TRANSACTION_TIP             N_("Add, change, or unlink the document linked with the current transaction")
#define LINK_TRANSACTION_OPEN_TIP        N_("Open the linked document for the current transaction")
#define JUMP_LINKED_INVOICE_TIP          N_("Jump to the linked bill, invoice, or voucher")
#define CUT_SPLIT_TIP                    N_("Cut the selected split into clipboard")
#define COPY_SPLIT_TIP                   N_("Copy the selected split into clipboard")
#define PASTE_SPLIT_TIP                  N_("Paste the split from the clipboard")
#define DUPLICATE_SPLIT_TIP              N_("Make a copy of the current split")
#define DELETE_SPLIT_TIP                 N_("Delete the current split")

static GActionEntry gnc_plugin_page_register_actions [] =
{
    { "FilePrintAction", gnc_plugin_page_register_cmd_print_check, NULL, NULL, NULL },
    { "EditCutAction", gnc_plugin_page_register_cmd_cut, NULL, NULL, NULL },
    { "EditCopyAction", gnc_plugin_page_register_cmd_copy, NULL, NULL, NULL },
    { "EditPasteAction", gnc_plugin_page_register_cmd_paste, NULL, NULL, NULL },
    { "EditEditAccountAction", gnc_plugin_page_register_cmd_edit_account, NULL, NULL, NULL },
    { "EditFindAccountAction", gnc_plugin_page_register_cmd_find_account, NULL, NULL, NULL },
    { "EditFindTransactionsAction", gnc_plugin_page_register_cmd_find_transactions, NULL, NULL, NULL },
    { "EditTaxOptionsAction", gnc_plugin_page_register_cmd_edit_tax_options, NULL, NULL, NULL },
    { "CutTransactionAction", gnc_plugin_page_register_cmd_cut_transaction, NULL, NULL, NULL },
    { "CopyTransactionAction", gnc_plugin_page_register_cmd_copy_transaction, NULL, NULL, NULL },
    { "PasteTransactionAction", gnc_plugin_page_register_cmd_paste_transaction, NULL, NULL, NULL },
    { "DuplicateTransactionAction", gnc_plugin_page_register_cmd_duplicate_transaction, NULL, NULL, NULL },
    { "DeleteTransactionAction", gnc_plugin_page_register_cmd_delete_transaction, NULL, NULL, NULL },
    { "RemoveTransactionSplitsAction", gnc_plugin_page_register_cmd_reinitialize_transaction, NULL, NULL, NULL },
    { "RecordTransactionAction", gnc_plugin_page_register_cmd_enter_transaction, NULL, NULL, NULL },
    { "CancelTransactionAction", gnc_plugin_page_register_cmd_cancel_transaction, NULL, NULL, NULL },
    { "VoidTransactionAction", gnc_plugin_page_register_cmd_void_transaction, NULL, NULL, NULL },
    { "UnvoidTransactionAction", gnc_plugin_page_register_cmd_unvoid_transaction, NULL, NULL, NULL },
    { "ReverseTransactionAction", gnc_plugin_page_register_cmd_reverse_transaction, NULL, NULL, NULL },
    { "LinkTransactionAction", gnc_plugin_page_register_cmd_linked_transaction, NULL, NULL, NULL },
    { "LinkedTransactionOpenAction", gnc_plugin_page_register_cmd_linked_transaction_open, NULL, NULL, NULL },
    { "JumpLinkedInvoiceAction", gnc_plugin_page_register_cmd_jump_linked_invoice, NULL, NULL, NULL },
    { "ViewSortByAction", gnc_plugin_page_register_cmd_view_sort_by, NULL, NULL, NULL },
    { "ViewFilterByAction", gnc_plugin_page_register_cmd_view_filter_by, NULL, NULL, NULL },
    { "ViewRefreshAction", gnc_plugin_page_register_cmd_reload, NULL, NULL, NULL },
    { "ActionsTransferAction", gnc_plugin_page_register_cmd_transfer, NULL, NULL, NULL },
    { "ActionsReconcileAction", gnc_plugin_page_register_cmd_reconcile, NULL, NULL, NULL },
    { "ActionsAutoClearAction", gnc_plugin_page_register_cmd_autoclear, NULL, NULL, NULL },
    { "ActionsStockAssistantAction", gnc_plugin_page_register_cmd_stock_assistant, NULL, NULL, NULL },
    { "ActionsStockSplitAction", gnc_plugin_page_register_cmd_stock_split, NULL, NULL, NULL },
    { "ActionsLotsAction", gnc_plugin_page_register_cmd_lots, NULL, NULL, NULL },
    { "BlankTransactionAction", gnc_plugin_page_register_cmd_blank_transaction, NULL, NULL, NULL },
    { "GotoDateAction", gnc_plugin_page_register_cmd_goto_date, NULL, NULL, NULL },
    { "EditExchangeRateAction", gnc_plugin_page_register_cmd_exchange_rate, NULL, NULL, NULL },
    { "JumpTransactionAction", gnc_plugin_page_register_cmd_jump, NULL, NULL, NULL },
    { "ScheduleTransactionAction", gnc_plugin_page_register_cmd_schedule, NULL, NULL, NULL },
    { "ScrubAllAction", gnc_plugin_page_register_cmd_scrub_all, NULL, NULL, NULL },
    { "ScrubCurrentAction", gnc_plugin_page_register_cmd_scrub_current, NULL, NULL, NULL },
    { "ReportsAccountReportAction", gnc_plugin_page_register_cmd_account_report, NULL, NULL, NULL },
    { "ReportsAcctTransReportAction", gnc_plugin_page_register_cmd_transaction_report, NULL, NULL, NULL },

    { "ViewStyleDoubleLineAction", gnc_plugin_page_register_cmd_style_double_line, NULL, "false", NULL },
    { "SplitTransactionAction", gnc_plugin_page_register_cmd_expand_transaction, NULL, "false", NULL },
    { "ViewStyleRadioAction", gnc_plugin_page_register_cmd_style_changed, "i", "@i 0", NULL },
};
static guint gnc_plugin_page_register_n_actions = G_N_ELEMENTS(gnc_plugin_page_register_actions);

/** The default menu items that need to be add to the menu */
static const gchar *gnc_plugin_load_ui_items [] =
{
    "FilePlaceholder3",
    "EditPlaceholder1",
    "EditPlaceholder2",
    "EditPlaceholder3",
    "EditPlaceholder5",
    "ViewPlaceholder1",
    "ViewPlaceholder2",
    "ViewPlaceholder3",
    "ViewPlaceholder4",
    "TransPlaceholder0",
    "TransPlaceholder1",
    "TransPlaceholder2",
    "TransPlaceholder3",
    "TransPlaceholder4",
    "ActionsPlaceholder4",
    "ActionsPlaceholder5",
    "ActionsPlaceholder6",
    "ReportsPlaceholder1",
    NULL,
};

/** Actions that require an account to be selected before they are
 *  enabled. */
static const gchar* actions_requiring_account[] =
{
    "EditEditAccountAction",
    "ActionsReconcileAction",
    "ActionsAutoClearAction",
    "ActionsLotsAction",
    NULL
};

static const gchar* actions_requiring_priced_account[] =
{
    "ActionsStockAssistantAction",
    NULL
};

/** Short labels for use on the toolbar buttons. */
static GncToolBarShortNames toolbar_labels[] =
{
    { "ActionsTransferAction",              N_ ("Transfer") },
    { "RecordTransactionAction",            N_ ("Enter") },
    { "CancelTransactionAction",            N_ ("Cancel") },
    { "DeleteTransactionAction",            N_ ("Delete") },
    { "DuplicateTransactionAction",         N_ ("Duplicate") },
    { "SplitTransactionAction",
      /* Translators: This is the label of a toolbar button. So keep it short. */
      N_ ("Show Splits") },
    { "JumpTransactionAction",              N_ ("Jump") },
    { "ScheduleTransactionAction",          N_ ("Schedule") },
    { "BlankTransactionAction",             N_ ("Blank") },
    { "ActionsReconcileAction",             N_ ("Reconcile") },
    { "ActionsAutoClearAction",             N_ ("Auto-clear") },
    { "LinkTransactionAction",              N_ ("Manage Document Link") },
    { "LinkedTransactionOpenAction",        N_ ("Open Linked Document") },
    { "JumpLinkedInvoiceAction",            N_ ("Invoice") },
    { "ActionsStockAssistantAction",        N_ ("Stock Assistant") },
    { NULL, NULL },
};

struct status_action
{
    const char* action_name;
    int value;
    GtkWidget* widget;
};

static struct status_action status_actions[] =
{
    { "filter_status_reconciled",   CLEARED_RECONCILED, NULL },
    { "filter_status_cleared",      CLEARED_CLEARED, NULL },
    { "filter_status_voided",       CLEARED_VOIDED, NULL },
    { "filter_status_frozen",       CLEARED_FROZEN, NULL },
    { "filter_status_unreconciled", CLEARED_NO, NULL },
    { NULL, 0, NULL },
};

#define CLEARED_VALUE "cleared_value"
#define DEFAULT_FILTER "0x001f"
#define DEFAULT_SORT_ORDER "BY_STANDARD"

/************************************************************/
/*                      Data Structures                     */
/************************************************************/

typedef struct GncPluginPageRegisterPrivate
{
    GNCLedgerDisplay* ledger;
    GNCSplitReg* gsr;

    GtkWidget* widget;

    gint event_handler_id;
    gint component_manager_id;
    GncGUID key;  /* The guid of the Account we're watching */

    gint lines_default;
    gboolean read_only;
    gboolean page_focus;
    gboolean enable_refresh; // used to reduce ledger display refreshes
    Query* search_query;     // saved search query for comparison
    Query* filter_query;     // saved filter query for comparison

    struct
    {
        GtkWidget* dialog;
        GtkWidget* num_radio;
        GtkWidget* act_radio;
        SortType original_sort_type;
        gboolean original_save_order;
        gboolean save_order;
        gboolean reverse_order;
        gboolean original_reverse_order;
    } sd;

    struct
    {
        GtkWidget* dialog;
        GtkWidget* table;
        GtkWidget* start_date_choose;
        GtkWidget* start_date_today;
        GtkWidget* start_date;
        GtkWidget* end_date_choose;
        GtkWidget* end_date_today;
        GtkWidget* end_date;
        GtkWidget* num_days;
        cleared_match_t original_cleared_match;
        cleared_match_t cleared_match;
        time64 original_start_time;
        time64 original_end_time;
        time64 start_time;
        time64 end_time;
        gint days;
        gint original_days;
        gboolean original_save_filter;
        gboolean save_filter;
    } fd;
} GncPluginPageRegisterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GncPluginPageRegister, gnc_plugin_page_register,
                            GNC_TYPE_PLUGIN_PAGE)

#define GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE(o)  \
   ((GncPluginPageRegisterPrivate*)gnc_plugin_page_register_get_instance_private((GncPluginPageRegister*)o))

/************************************************************/
/*                      Implementation                      */
/************************************************************/

static GncPluginPage*
gnc_plugin_page_register_new_common (GNCLedgerDisplay* ledger)
{
    GncPluginPageRegister* register_page;
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* plugin_page;
    GNCSplitReg* gsr;
    const GList* item;
    GList* book_list;
    gchar* label;
    gchar* label_color;
    QofQuery* q;

    // added for version 4.0 onwards
    if (!gnc_features_check_used (gnc_get_current_book(), GNC_FEATURE_REG_SORT_FILTER))
        gnc_features_set_used (gnc_get_current_book(), GNC_FEATURE_REG_SORT_FILTER);

    // added for version 4.14 onwards
    if (!gnc_using_equity_type_opening_balance_account (gnc_get_current_book()))
        gnc_set_use_equity_type_opening_balance_account (gnc_get_current_book());

    /* Is there an existing page? */
    gsr = GNC_SPLIT_REG(gnc_ledger_display_get_user_data (ledger));
    if (gsr)
    {
        item = gnc_gobject_tracking_get_list (GNC_PLUGIN_PAGE_REGISTER_NAME);
        for (; item; item = g_list_next (item))
        {
            register_page = (GncPluginPageRegister*)item->data;
            priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (register_page);
            if (priv->gsr == gsr)
                return GNC_PLUGIN_PAGE (register_page);
        }
    }

    register_page = GNC_PLUGIN_PAGE_REGISTER(g_object_new (GNC_TYPE_PLUGIN_PAGE_REGISTER, nullptr));
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (register_page);
    priv->ledger = ledger;
    priv->key = *guid_null();

    plugin_page = GNC_PLUGIN_PAGE (register_page);
    label = gnc_plugin_page_register_get_tab_name (plugin_page);
    gnc_plugin_page_set_page_name (plugin_page, label);
    g_free (label);

    label_color = gnc_plugin_page_register_get_tab_color (plugin_page);
    gnc_plugin_page_set_page_color (plugin_page, label_color);
    g_free (label_color);

    label = gnc_plugin_page_register_get_long_name (plugin_page);
    gnc_plugin_page_set_page_long_name (plugin_page, label);
    g_free (label);

    q = gnc_ledger_display_get_query (ledger);
    book_list = qof_query_get_books (q);
    for (item = book_list; item; item = g_list_next (item))
        gnc_plugin_page_add_book (plugin_page, (QofBook*)item->data);
    // Do not free the list. It is owned by the query.

    priv->component_manager_id = 0;
    return plugin_page;
}

static gpointer
gnc_plug_page_register_check_commodity (Account* account, void* usr_data)
{
    // Check that account's commodity matches the commodity in usr_data
    gnc_commodity* com0 = (gnc_commodity*) usr_data;
    gnc_commodity* com1 = xaccAccountGetCommodity (account);
    return gnc_commodity_equal (com1, com0) ? NULL : com1;
}

GncPluginPage*
gnc_plugin_page_register_new (Account* account, gboolean subaccounts)
{
    GNCLedgerDisplay* ledger;
    GncPluginPage* page;
    GncPluginPageRegisterPrivate* priv;
    gnc_commodity* com0;
    gnc_commodity* com1;

    ENTER ("account=%p, subaccounts=%s", account,
           subaccounts ? "TRUE" : "FALSE");

    com0 = gnc_account_get_currency_or_parent (account);
    com1 = GNC_COMMODITY(gnc_account_foreach_descendant_until (account,
                                                               gnc_plug_page_register_check_commodity,
                                                               static_cast<gpointer>(com0)));

    if (subaccounts)
        ledger = gnc_ledger_display_subaccounts (account, com1 != NULL);
    else
        ledger = gnc_ledger_display_simple (account);

    page = gnc_plugin_page_register_new_common (ledger);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->key = *xaccAccountGetGUID (account);

    LEAVE ("%p", page);
    return page;
}

GncPluginPage*
gnc_plugin_page_register_new_gl (void)
{
    GNCLedgerDisplay* ledger;

    ledger = gnc_ledger_display_gl();
    return gnc_plugin_page_register_new_common (ledger);
}

GncPluginPage*
gnc_plugin_page_register_new_ledger (GNCLedgerDisplay* ledger)
{
    return gnc_plugin_page_register_new_common (ledger);
}

static void
gnc_plugin_page_register_class_init (GncPluginPageRegisterClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GncPluginPageClass* gnc_plugin_class = GNC_PLUGIN_PAGE_CLASS (klass);

    object_class->finalize = gnc_plugin_page_register_finalize;

    gnc_plugin_class->tab_icon        = GNC_ICON_ACCOUNT;
    gnc_plugin_class->plugin_name     = GNC_PLUGIN_PAGE_REGISTER_NAME;
    gnc_plugin_class->create_widget   = gnc_plugin_page_register_create_widget;
    gnc_plugin_class->destroy_widget  = gnc_plugin_page_register_destroy_widget;
    gnc_plugin_class->window_changed  = gnc_plugin_page_register_window_changed;
    gnc_plugin_class->focus_page      = gnc_plugin_page_register_focus;
    gnc_plugin_class->save_page       = gnc_plugin_page_register_save_page;
    gnc_plugin_class->recreate_page   = gnc_plugin_page_register_recreate_page;
    gnc_plugin_class->update_edit_menu_actions =
        gnc_plugin_page_register_update_edit_menu;
    gnc_plugin_class->finish_pending  = gnc_plugin_page_register_finish_pending;
    gnc_plugin_class->focus_page_function = gnc_plugin_page_register_focus_widget;

    gnc_ui_register_account_destroy_callback (gppr_account_destroy_cb);
}

static void
gnc_plugin_page_register_init (GncPluginPageRegister* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* parent;
    GSimpleActionGroup *simple_action_group;
    gboolean use_new;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    /* Init parent declared variables */
    parent = GNC_PLUGIN_PAGE (plugin_page);
    use_new = gnc_prefs_get_bool (GNC_PREFS_GROUP_GENERAL_REGISTER,
                                  GNC_PREF_USE_NEW);
    g_object_set (G_OBJECT (plugin_page),
                  "page-name",      _ ("General Journal"),
                  "ui-description", "gnc-plugin-page-register.ui",
                  "use-new-window", use_new,
                  NULL);

    /* Create menu and toolbar information */
    simple_action_group = gnc_plugin_page_create_action_group (parent, "GncPluginPageRegisterActions");
    g_action_map_add_action_entries (G_ACTION_MAP(simple_action_group),
                                     gnc_plugin_page_register_actions,
                                     gnc_plugin_page_register_n_actions,
                                     plugin_page);

    priv->lines_default     = DEFAULT_LINES_AMOUNT;
    priv->read_only         = FALSE;
    priv->fd.cleared_match  = CLEARED_ALL;
    priv->fd.days           = 0;
    priv->enable_refresh    = TRUE;
    priv->search_query      = NULL;
    priv->filter_query      = NULL;
}

static void
gnc_plugin_page_register_finalize (GObject* object)
{
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (object));

    ENTER ("object %p", object);

    G_OBJECT_CLASS (gnc_plugin_page_register_parent_class)->finalize (object);
    LEAVE (" ");
}

Account*
gnc_plugin_page_register_get_account (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    Account* leader;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    ledger_type = gnc_ledger_display_type (priv->ledger);
    leader = gnc_ledger_display_leader (priv->ledger);

    if ((ledger_type == LD_SINGLE) || (ledger_type == LD_SUBACCOUNT))
        return leader;
    return NULL;
}

Transaction*
gnc_plugin_page_register_get_current_txn (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    return gnc_split_register_get_current_trans (reg);
}

/**
 * Whenever the current page is changed, if a register page is
 * the current page, set focus on the sheet.
 */
static gboolean
gnc_plugin_page_register_focus_widget (GncPluginPage* register_plugin_page)
{
    if (GNC_IS_PLUGIN_PAGE_REGISTER (register_plugin_page))
    {
        GncWindow* gnc_window = GNC_WINDOW(GNC_PLUGIN_PAGE(register_plugin_page)->window);
        GNCSplitReg *gsr = gnc_plugin_page_register_get_gsr (GNC_PLUGIN_PAGE(register_plugin_page));

        if (GNC_IS_MAIN_WINDOW(GNC_PLUGIN_PAGE(register_plugin_page)->window))
        {
            /* Enable the Transaction menu */
            GAction *action = gnc_main_window_find_action (GNC_MAIN_WINDOW(register_plugin_page->window), "TransactionAction");
            g_simple_action_set_enabled (G_SIMPLE_ACTION(action), TRUE);
            /* Disable the Schedule menu */
            action = gnc_main_window_find_action (GNC_MAIN_WINDOW(register_plugin_page->window), "ScheduledAction");
            g_simple_action_set_enabled (G_SIMPLE_ACTION(action), FALSE);

            gnc_main_window_update_menu_and_toolbar (GNC_MAIN_WINDOW(register_plugin_page->window),
                                                     register_plugin_page,
                                                     gnc_plugin_load_ui_items);
        }
        else
        {
            GtkWidget *toolbar = gnc_window_get_toolbar (gnc_window);
            GtkWidget *menubar = gnc_window_get_menubar (gnc_window);
            GMenuModel *menubar_model = gnc_window_get_menubar_model (gnc_window);
            GtkWidget *statusbar = gnc_window_get_statusbar (gnc_window);

            // add tooltip redirect call backs
            gnc_plugin_add_toolbar_tooltip_callbacks (toolbar, statusbar);
            gnc_plugin_add_menu_tooltip_callbacks (menubar, menubar_model, statusbar);
        }

        // setup any short toolbar names
        gnc_plugin_init_short_names (gnc_window_get_toolbar (gnc_window), toolbar_labels);

        gnc_plugin_page_register_ui_update (NULL, GNC_PLUGIN_PAGE_REGISTER(register_plugin_page));

        gnc_split_reg_focus_on_sheet (gsr);
    }
    return FALSE;
}

/* This is the list of actions which are switched inactive in a read-only book. */
static const char* readonly_inactive_actions[] =
{
    "EditCutAction",
    "EditPasteAction",
    "CutTransactionAction",
    "PasteTransactionAction",
    "DuplicateTransactionAction",
    "DeleteTransactionAction",
    "RemoveTransactionSplitsAction",
    "RecordTransactionAction",
    "CancelTransactionAction",
    "UnvoidTransactionAction",
    "VoidTransactionAction",
    "ReverseTransactionAction",
    "ActionsTransferAction",
    "ActionsReconcileAction",
    "ActionsStockSplitAction",
    "ScheduleTransactionAction",
    "ScrubAllAction",
    "ScrubCurrentAction",
    "LinkTransactionAction",
    NULL
};

/* This is the list of actions whose text needs to be changed based on whether */
/* the current cursor class is transaction or split. */
static const char* tran_vs_split_actions[] =
{
    "CutTransactionAction",
    "CopyTransactionAction",
    "PasteTransactionAction",
    "DuplicateTransactionAction",
    "DeleteTransactionAction",
    NULL
};

/* This is the list of labels for when the current cursor class is transaction. */
static const char* tran_action_labels[] =
{
    CUT_TRANSACTION_LABEL,
    COPY_TRANSACTION_LABEL,
    PASTE_TRANSACTION_LABEL,
    DUPLICATE_TRANSACTION_LABEL,
    DELETE_TRANSACTION_LABEL,
    LINK_TRANSACTION_LABEL,
    LINK_TRANSACTION_OPEN_LABEL,
    JUMP_LINKED_INVOICE_LABEL,
    NULL
};

/* This is the list of tooltips for when the current cursor class is transaction. */
static const char* tran_action_tips[] =
{
    CUT_TRANSACTION_TIP,
    COPY_TRANSACTION_TIP,
    PASTE_TRANSACTION_TIP,
    DUPLICATE_TRANSACTION_TIP,
    DELETE_TRANSACTION_TIP,
    LINK_TRANSACTION_TIP,
    LINK_TRANSACTION_OPEN_TIP,
    JUMP_LINKED_INVOICE_TIP,
    NULL
};

/* This is the list of labels for when the current cursor class is split. */
static const char* split_action_labels[] =
{
    CUT_SPLIT_LABEL,
    COPY_SPLIT_LABEL,
    PASTE_SPLIT_LABEL,
    DUPLICATE_SPLIT_LABEL,
    DELETE_SPLIT_LABEL,
    NULL
};

/* This is the list of tooltips for when the current cursor class is split. */
static const char* split_action_tips[] =
{
    CUT_SPLIT_TIP,
    COPY_SPLIT_TIP,
    PASTE_SPLIT_TIP,
    DUPLICATE_SPLIT_TIP,
    DELETE_SPLIT_TIP,
    NULL
};

static std::vector<GncInvoice*>
invoices_from_transaction (const Transaction* trans)
{
    std::vector<GncInvoice*> rv;

    g_return_val_if_fail (GNC_IS_TRANSACTION (trans), rv);

    for (auto node = xaccTransGetSplitList (trans); node; node = g_list_next (node))
    {
        auto split = GNC_SPLIT(node->data);
        auto account = xaccSplitGetAccount (split);
        if (!account || !xaccAccountIsAPARType(xaccAccountGetType(account)))
            continue;
        auto inv = invoice_from_split (split);
        if (inv)
            rv.push_back (inv);
    }
    return rv;
}

static void
gnc_plugin_page_register_ui_update (gpointer various,
                                    GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    GAction* action;
    GNCLedgerDisplayType ledger_type;
    gboolean expanded, voided, read_only = FALSE, read_only_reg = FALSE;
    Transaction* trans;
    CursorClass cursor_class;
    const char* uri;
    Account *account;
    GncWindow* gnc_window = GNC_WINDOW(GNC_PLUGIN_PAGE(page)->window);

    /* Set 'Split Transaction' */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    cursor_class = gnc_split_register_get_current_cursor_class (reg);
    expanded = gnc_split_register_current_trans_expanded (reg);

    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "SplitTransactionAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), reg->style == REG_STYLE_LEDGER);

    /* Set "style" radio button */
    ledger_type = gnc_ledger_display_type (priv->ledger);
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "ViewStyleRadioAction");

    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), ledger_type != LD_GL);
    g_action_change_state (G_ACTION(action), g_variant_new_int32 (reg->style));

    /* Set double line */
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "ViewStyleDoubleLineAction");
    g_action_change_state (G_ACTION(action), g_variant_new_boolean (reg->use_double_line));

    /* Split Expand */
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "SplitTransactionAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), reg->style == REG_STYLE_LEDGER);

    g_signal_handlers_block_by_func (action, (gpointer)gnc_plugin_page_register_cmd_expand_transaction, page);
    g_action_change_state (G_ACTION(action), g_variant_new_boolean (expanded));
    g_signal_handlers_unblock_by_func (action, (gpointer)gnc_plugin_page_register_cmd_expand_transaction, page);

    account = gnc_plugin_page_register_get_account (page);

    /* Done like this as the register can be displayed in embedded window */
    if (GNC_IS_MAIN_WINDOW(GNC_PLUGIN_PAGE(page)->window))
    {
        /* Enable the FilePrintAction */
        action = gnc_main_window_find_action (GNC_MAIN_WINDOW(GNC_PLUGIN_PAGE(page)->window), "FilePrintAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), TRUE);

        /* Set the vis of the StockAssistant */
        gnc_main_window_set_vis_of_items_by_action (GNC_MAIN_WINDOW(GNC_PLUGIN_PAGE(page)->window),
                                                    actions_requiring_priced_account,
                                                    account &&
                                                    xaccAccountIsPriced (account));
    }

    /* If we are in a readonly book, or possibly a place holder
     * account register make any modifying action inactive */
    if (qof_book_is_readonly (gnc_get_current_book()) ||
        gnc_split_reg_get_read_only (priv->gsr))
        read_only_reg = TRUE;

    gnc_plugin_set_actions_enabled (G_ACTION_MAP(gnc_plugin_page_get_action_group (GNC_PLUGIN_PAGE(page))),
                                    actions_requiring_account,
                                    !read_only_reg && account != NULL);

    gnc_plugin_set_actions_enabled (G_ACTION_MAP(gnc_plugin_page_get_action_group (GNC_PLUGIN_PAGE(page))),
                                    actions_requiring_priced_account,
                                    account && xaccAccountIsPriced (account));

    /* Set available actions based on read only */
    trans = gnc_split_register_get_current_trans (reg);

    if (cursor_class == CURSOR_CLASS_SPLIT)
    {
        if (GNC_IS_MAIN_WINDOW(GNC_PLUGIN_PAGE(page)->window))
            gnc_plugin_page_set_menu_popup_qualifier (GNC_PLUGIN_PAGE(page), "split");
        else
            gnc_plugin_page_set_menu_popup_qualifier (GNC_PLUGIN_PAGE(page), "split-sx");
    }
    else
    {
        if (GNC_IS_MAIN_WINDOW(GNC_PLUGIN_PAGE(page)->window))
            gnc_plugin_page_set_menu_popup_qualifier (GNC_PLUGIN_PAGE(page), "trans");
        else
            gnc_plugin_page_set_menu_popup_qualifier (GNC_PLUGIN_PAGE(page), "trans-sx");
    }

    /* If the register is not read only, make any modifying action active
     * to start with */
    if (!read_only_reg)
    {
        const char** iter;
        for (iter = readonly_inactive_actions; *iter; ++iter)
        {
            /* Set the action's sensitivity */
            GAction* action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), *iter);
            g_simple_action_set_enabled (G_SIMPLE_ACTION(action), TRUE);
        }
        main_window_update_page_set_read_only_icon (GNC_PLUGIN_PAGE(page), FALSE);

        if (trans)
            read_only = xaccTransIsReadonlyByPostedDate (trans);

        voided = xaccTransHasSplitsInState (trans, VREC);

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "CutTransactionAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !read_only & !voided);

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "PasteTransactionAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !read_only & !voided);

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "DeleteTransactionAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !read_only & !voided);

        if (cursor_class == CURSOR_CLASS_SPLIT)
        {
             action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                                  "DuplicateTransactionAction");
             g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !read_only & !voided);
        }

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "RemoveTransactionSplitsAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !read_only & !voided);

        /* Set 'Void' and 'Unvoid' */
        if (read_only)
            voided = TRUE;

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "VoidTransactionAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !voided);

        if (read_only)
            voided = FALSE;

        action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                             "UnvoidTransactionAction");
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), voided);
    }

    /* Set 'Open and Remove Linked Documents' */
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                         "LinkedTransactionOpenAction");
    if (trans)
    {
        uri = xaccTransGetDocLink (trans);
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), (uri ? TRUE:FALSE));
    }
    /* Set 'ExecAssociatedInvoice'
       We can determine an invoice from a txn if either
       - it is an invoice transaction
       - it has splits with an invoice associated with it
    */
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page),
                                         "JumpLinkedInvoiceAction");
    if (trans)
    {
        auto invoices = invoices_from_transaction (trans);
        g_simple_action_set_enabled (G_SIMPLE_ACTION(action), !invoices.empty());
    }

    gnc_plugin_business_split_reg_ui_update (GNC_PLUGIN_PAGE(page));

    /* If we are read only, make any modifying action inactive */
    if (read_only_reg)
    {
        const char** iter;
        for (iter = readonly_inactive_actions; *iter; ++iter)
        {
            /* Set the action's sensitivity */
            GAction* action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), *iter);
            g_simple_action_set_enabled (G_SIMPLE_ACTION(action), FALSE);
        }
        main_window_update_page_set_read_only_icon (GNC_PLUGIN_PAGE(page), TRUE);
    }

    /* Modifying action descriptions based on cursor class */
    {
        GncMenuModelSearch *gsm = g_new0 (GncMenuModelSearch, 1);
        gboolean found = FALSE;
        const char** iter, **label_iter, **tooltip_iter;
        gboolean curr_label_trans = FALSE;
        iter = tran_vs_split_actions;
        label_iter = tran_action_labels;

        gsm->search_action_label = NULL;
        gsm->search_action_name = *iter;
        gsm->search_action_target = NULL;

        found = gnc_menubar_model_find_item (gnc_window_get_menubar_model (gnc_window), gsm);

        PINFO("Test for action '%s', found is %d, iter label is '%s'", *iter, found, _(*label_iter));

        if (!found)
        {
            g_free (gsm);
            return;
        }

        if (g_strcmp0 (gsm->search_action_label, _(*label_iter)) == 0)
            curr_label_trans = TRUE;

        g_free (gsm);

        if ((cursor_class == CURSOR_CLASS_SPLIT) && curr_label_trans)
        {
            gboolean found = FALSE;
            label_iter = split_action_labels;
            tooltip_iter = split_action_tips;
            for (iter = tran_vs_split_actions; *iter; ++iter)
            {
                /* Adjust the action's label and tooltip */
                found = gnc_menubar_model_update_item (gnc_window_get_menubar_model (gnc_window),
                                                       *iter, NULL, _(*label_iter), NULL, _(*tooltip_iter));

                PINFO("split model_item action '%s', found is %d, iter label is '%s'",
                        *iter, found, _(*label_iter));

                ++label_iter;
                ++tooltip_iter;
            }
        }
        else if ((cursor_class == CURSOR_CLASS_TRANS) && !curr_label_trans)
        {
            gboolean found = FALSE;
            label_iter = tran_action_labels;
            tooltip_iter = tran_action_tips;
            for (iter = tran_vs_split_actions; *iter; ++iter)
            {
                /* Adjust the action's label and tooltip */
                found = gnc_menubar_model_update_item (gnc_window_get_menubar_model (gnc_window),
                                                       *iter, NULL, _(*label_iter), NULL, _(*tooltip_iter));

                PINFO("trans model_item action '%s', found is %d, iter label is '%s'",
                        *iter, found, _(*label_iter));

                ++label_iter;
                ++tooltip_iter;
            }
        }
        // now add the callbacks to the replaced menu items.
        gnc_plugin_add_menu_tooltip_callbacks (gnc_window_get_menubar (gnc_window),
                                               gnc_window_get_menubar_model (gnc_window),
                                               gnc_window_get_statusbar (gnc_window));

        // need to add any accelerator keys, default or user added
        gnc_add_accelerator_keys_for_menu (gnc_window_get_menubar (gnc_window),
                                           gnc_window_get_menubar_model (gnc_window),
                                           gnc_window_get_accel_group (gnc_window));
    }
}

static void
gnc_plugin_page_register_ui_initial_state (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv ;
    GSimpleActionGroup *simple_action_group;
    GAction *action;
    Account* account;
    SplitRegister* reg;
    GNCLedgerDisplayType ledger_type;
    gboolean is_readwrite = !qof_book_is_readonly (gnc_get_current_book());

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    account = gnc_plugin_page_register_get_account (page);

    /* Get the action group */
    simple_action_group = gnc_plugin_page_get_action_group (GNC_PLUGIN_PAGE(page));
    g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP(simple_action_group));

    gnc_plugin_set_actions_enabled (G_ACTION_MAP(simple_action_group), actions_requiring_account,
                                    is_readwrite && account != NULL);

    /* Set "style" radio button */
    ledger_type = gnc_ledger_display_type (priv->ledger);
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "ViewStyleRadioAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), ledger_type == LD_SINGLE);

    reg = gnc_ledger_display_get_split_register (priv->ledger);

    g_signal_handlers_block_by_func (action,
                                     (gpointer)gnc_plugin_page_register_cmd_style_changed, page);
    g_action_change_state (G_ACTION(action), g_variant_new_int32 (reg->style));
    g_signal_handlers_unblock_by_func (action,
                                       (gpointer)gnc_plugin_page_register_cmd_style_changed, page);

    /* Set "double line" toggle button */
    action = gnc_plugin_page_get_action (GNC_PLUGIN_PAGE(page), "ViewStyleDoubleLineAction");
    g_signal_handlers_block_by_func (action,
                                     (gpointer)gnc_plugin_page_register_cmd_style_double_line, page);
    g_action_change_state (G_ACTION(action), g_variant_new_boolean (reg->use_double_line));
    g_signal_handlers_unblock_by_func (action,
                                       (gpointer)gnc_plugin_page_register_cmd_style_double_line, page);
}

/* Virtual Functions */

static const gchar*
get_filter_default_num_of_days (GNCLedgerDisplayType ledger_type)
{
    if (ledger_type == LD_GL)
        return DEFAULT_FILTER_NUM_DAYS_GL;
    else
        return "0";
}

/* For setting the focus on a register page, the default gnc_plugin
 * function for 'focus_page' is overridden so that the page focus
 * can be conditionally set. This is to allow for enabling the setting
 * of the sheet focus only when the page is the current one.
 */
static void
gnc_plugin_page_register_focus (GncPluginPage* plugin_page,
                                gboolean on_current_page)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;
    GNCSplitReg* gsr;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    gsr = gnc_plugin_page_register_get_gsr (GNC_PLUGIN_PAGE (plugin_page));

    if (on_current_page)
    {
        priv->page_focus = TRUE;

        // Chain up to use parent version of 'focus_page' which will
        // use an idle_add as the page changed signal is emitted multiple times.
        GNC_PLUGIN_PAGE_CLASS (gnc_plugin_page_register_parent_class)->focus_page (plugin_page, TRUE);
    }
    else
        priv->page_focus = FALSE;

    // set the sheet focus setting
    gnc_split_reg_set_sheet_focus (gsr, priv->page_focus);

    gnc_ledger_display_set_focus (priv->ledger, priv->page_focus);
}

static GtkWidget*
gnc_plugin_page_register_create_widget (GncPluginPage* plugin_page)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    GncWindow* gnc_window;
    guint numRows;
    GtkWidget* gsr;
    SplitRegister* reg;
    Account* acct;
    gchar* order;
    int filter_changed = 0;
    gboolean create_new_page = FALSE;

    ENTER ("page %p", plugin_page);
    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    if (priv->widget != NULL)
    {
        LEAVE ("existing widget %p", priv->widget);
        return priv->widget;
    }

    priv->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous (GTK_BOX (priv->widget), FALSE);
    gtk_widget_show (priv->widget);

    // Set the name for this widget so it can be easily manipulated with css
    gtk_widget_set_name (GTK_WIDGET(priv->widget), "gnc-id-register-page");

    numRows = priv->lines_default;
    numRows = MIN (numRows, DEFAULT_LINES_AMOUNT);

    gnc_window = GNC_WINDOW(GNC_PLUGIN_PAGE(page)->window);
    gsr = gnc_split_reg_new (priv->ledger,
                             gnc_window_get_gtk_window (gnc_window),
                             numRows, priv->read_only);
    priv->gsr = (GNCSplitReg *)gsr;

    gtk_widget_show (gsr);
    gtk_box_pack_start (GTK_BOX (priv->widget), gsr, TRUE, TRUE, 0);

    g_signal_connect (G_OBJECT (gsr), "help-changed",
                      G_CALLBACK (gnc_plugin_page_help_changed_cb),
                      page);

    g_signal_connect (G_OBJECT (gsr), "show-popup-menu",
                      G_CALLBACK (gnc_plugin_page_popup_menu_cb),
                      page);

    reg = gnc_ledger_display_get_split_register (priv->ledger);
    gnc_split_register_config (reg, reg->type, reg->style,
                               reg->use_double_line);

    gnc_plugin_page_register_ui_initial_state (page);
    gnc_plugin_page_register_ui_update (NULL, page);

    ledger_type = gnc_ledger_display_type (priv->ledger);

    {
        gchar** filter;
        gchar* filter_str;
        guint filtersize = 0;
        /* Set the sort order for the split register and status of save order button */
        priv->sd.save_order = FALSE;
        order = gnc_plugin_page_register_get_sort_order (plugin_page);

        PINFO ("Loaded Sort order is %s", order);

        gnc_split_reg_set_sort_type (priv->gsr, SortTypefromString (order));

        if (order && (g_strcmp0 (order, DEFAULT_SORT_ORDER) != 0))
            priv->sd.save_order = TRUE;

        priv->sd.original_save_order = priv->sd.save_order;
        g_free (order);

        priv->sd.reverse_order = gnc_plugin_page_register_get_sort_reversed (
                                     plugin_page);
        gnc_split_reg_set_sort_reversed (priv->gsr, priv->sd.reverse_order, FALSE);
        if (priv->sd.reverse_order)
            priv->sd.save_order = TRUE;

        priv->sd.original_reverse_order = priv->sd.reverse_order;

        /* Set the filter for the split register and status of save filter button */
        priv->fd.save_filter = FALSE;

        filter_str = gnc_plugin_page_register_get_filter (plugin_page);
        filter = g_strsplit (filter_str, ",", -1);
        filtersize = g_strv_length (filter);
        g_free (filter_str);

        PINFO ("Loaded Filter Status is %s", filter[0]);

        priv->fd.cleared_match = (cleared_match_t)g_ascii_strtoll (filter[0], NULL, 16);

        if (filtersize > 0 && (g_strcmp0 (filter[0], DEFAULT_FILTER) != 0))
            filter_changed = filter_changed + 1;

        if (filtersize > 1 && (g_strcmp0 (filter[1], "0") != 0))
        {
            PINFO ("Loaded Filter Start Date is %s", filter[1]);

            priv->fd.start_time = gnc_plugin_page_register_filter_dmy2time (filter[1]);
            priv->fd.start_time = gnc_time64_get_day_start (priv->fd.start_time);
            filter_changed = filter_changed + 1;
        }

        if (filtersize > 2 && (g_strcmp0 (filter[2], "0") != 0))
        {
            PINFO ("Loaded Filter End Date is %s", filter[2]);

            priv->fd.end_time = gnc_plugin_page_register_filter_dmy2time (filter[2]);
            priv->fd.end_time = gnc_time64_get_day_end (priv->fd.end_time);
            filter_changed = filter_changed + 1;
        }

        // set the default for the number of days
        priv->fd.days = (gint)g_ascii_strtoll (
                            get_filter_default_num_of_days (ledger_type), NULL, 10);

        if (filtersize > 3 &&
            (g_strcmp0 (filter[3], get_filter_default_num_of_days (ledger_type)) != 0))
        {
            PINFO ("Loaded Filter Days is %s", filter[3]);

            priv->fd.days = (gint)g_ascii_strtoll (filter[3], NULL, 10);
            filter_changed = filter_changed + 1;
        }

        if (filter_changed != 0)
            priv->fd.save_filter = TRUE;

        priv->fd.original_save_filter = priv->fd.save_filter;
        g_strfreev (filter);
    }

    if (ledger_type == LD_GL)
    {
        time64 start_time = 0, end_time = 0;

        if (reg->type == GENERAL_JOURNAL)
        {
            start_time = priv->fd.start_time;
            end_time = priv->fd.end_time;
        }
        else // search ledger and the like
        {
            priv->fd.days = 0;
            priv->fd.cleared_match = (cleared_match_t)g_ascii_strtoll (DEFAULT_FILTER, NULL, 16);
            gnc_split_reg_set_sort_type (priv->gsr,
                                         SortTypefromString (DEFAULT_SORT_ORDER));
            priv->sd.reverse_order = FALSE;
            priv->fd.save_filter = FALSE;
            priv->sd.save_order = FALSE;
        }

        priv->fd.original_days = priv->fd.days;

        priv->fd.original_start_time = start_time;
        priv->fd.start_time = start_time;
        priv->fd.original_end_time = end_time;
        priv->fd.end_time = end_time;
    }

    // if enable_refresh is TRUE, default, come from creating
    // new page instead of restoring
    if (priv->enable_refresh == TRUE)
    {
        create_new_page = TRUE;
        priv->enable_refresh = FALSE; // disable refresh
    }

    /* Update Query with Filter Status and Dates */
    gnc_ppr_update_status_query (page);
    gnc_ppr_update_date_query (page);

    /* Now do the refresh if this is a new page instead of restore */
    if (create_new_page)
    {
        priv->enable_refresh = TRUE;
        gnc_ledger_display_refresh (priv->ledger);
    }

    // Set filter tooltip for summary bar
    gnc_plugin_page_register_set_filter_tooltip (page);

    plugin_page->summarybar = gsr_create_summary_bar (priv->gsr);
    if (plugin_page->summarybar)
    {
        gtk_widget_show_all (plugin_page->summarybar);
        gtk_box_pack_start (GTK_BOX (priv->widget), plugin_page->summarybar,
                            FALSE, FALSE, 0);

        gnc_plugin_page_register_summarybar_position_changed (NULL, NULL, page);
        gnc_prefs_register_cb (GNC_PREFS_GROUP_GENERAL,
                               GNC_PREF_SUMMARYBAR_POSITION_TOP,
                               (gpointer)gnc_plugin_page_register_summarybar_position_changed,
                               page);
        gnc_prefs_register_cb (GNC_PREFS_GROUP_GENERAL,
                               GNC_PREF_SUMMARYBAR_POSITION_BOTTOM,
                               (gpointer)gnc_plugin_page_register_summarybar_position_changed,
                               page);
    }

    priv->event_handler_id = qof_event_register_handler
                             ((QofEventHandler)gnc_plugin_page_register_event_handler, page);
    priv->component_manager_id =
        gnc_register_gui_component (GNC_PLUGIN_PAGE_REGISTER_NAME,
                                    gnc_plugin_page_register_refresh_cb,
                                    gnc_plugin_page_register_close_cb,
                                    page);
    gnc_gui_component_set_session (priv->component_manager_id,
                                   gnc_get_current_session());
    acct = gnc_plugin_page_register_get_account (page);
    if (acct)
        gnc_gui_component_watch_entity (
            priv->component_manager_id, xaccAccountGetGUID (acct),
            QOF_EVENT_DESTROY | QOF_EVENT_MODIFY);

    gnc_split_reg_set_moved_cb
    (priv->gsr, (GFunc)gnc_plugin_page_register_ui_update, page);

    g_signal_connect (G_OBJECT (plugin_page), "inserted",
                      G_CALLBACK (gnc_plugin_page_inserted_cb),
                      NULL);

    /* DRH - Probably lots of other stuff from regWindowLedger should end up here. */
    LEAVE (" ");
    return priv->widget;
}

static void
gnc_plugin_page_register_destroy_widget (GncPluginPage* plugin_page)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;

    ENTER ("page %p", plugin_page);
    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    gnc_prefs_remove_cb_by_func (GNC_PREFS_GROUP_GENERAL,
                                 GNC_PREF_SUMMARYBAR_POSITION_TOP,
                                 (gpointer)gnc_plugin_page_register_summarybar_position_changed,
                                 page);
    gnc_prefs_remove_cb_by_func (GNC_PREFS_GROUP_GENERAL,
                                 GNC_PREF_SUMMARYBAR_POSITION_BOTTOM,
                                 (gpointer)gnc_plugin_page_register_summarybar_position_changed,
                                 page);

    // Remove the page_changed signal callback
    gnc_plugin_page_disconnect_page_changed (GNC_PLUGIN_PAGE (plugin_page));

    // Remove the page focus idle function if present
    g_idle_remove_by_data (GNC_PLUGIN_PAGE_REGISTER (plugin_page));

    if (priv->widget == NULL)
        return;

    if (priv->component_manager_id)
    {
        gnc_unregister_gui_component (priv->component_manager_id);
        priv->component_manager_id = 0;
    }

    if (priv->event_handler_id)
    {
        qof_event_unregister_handler (priv->event_handler_id);
        priv->event_handler_id = 0;
    }

    if (priv->sd.dialog)
    {
        gtk_widget_destroy (priv->sd.dialog);
        memset (&priv->sd, 0, sizeof (priv->sd));
    }

    if (priv->fd.dialog)
    {
        gtk_widget_destroy (priv->fd.dialog);
        memset (&priv->fd, 0, sizeof (priv->fd));
    }

    qof_query_destroy (priv->search_query);
    qof_query_destroy (priv->filter_query);

    gtk_widget_hide (priv->widget);
    gnc_ledger_display_close (priv->ledger);
    priv->ledger = NULL;
    LEAVE (" ");
}

static void
gnc_plugin_page_register_window_changed (GncPluginPage* plugin_page,
                                         GtkWidget* window)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->gsr->window =
        GTK_WIDGET (gnc_window_get_gtk_window (GNC_WINDOW (window)));
}

static const gchar* style_names[] =
{
    "Ledger",
    "Auto Ledger",
    "Journal",
    NULL
};

#define KEY_REGISTER_TYPE       "RegisterType"
#define KEY_ACCOUNT_NAME        "AccountName"
#define KEY_ACCOUNT_GUID        "AccountGuid"
#define KEY_REGISTER_STYLE      "RegisterStyle"
#define KEY_DOUBLE_LINE         "DoubleLineMode"

#define LABEL_ACCOUNT       "Account"
#define LABEL_SUBACCOUNT    "SubAccount"
#define LABEL_GL            "GL"
#define LABEL_SEARCH        "Search"


/** Save enough information about this register page that it can be
 *  recreated next time the user starts gnucash.
 *
 *  @param plugin_page The page to save.
 *
 *  @param key_file A pointer to the GKeyFile data structure where the
 *  page information should be written.
 *
 *  @param group_name The group name to use when saving data. */
static void
gnc_plugin_page_register_save_page (GncPluginPage* plugin_page,
                                    GKeyFile* key_file,
                                    const gchar* group_name)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    SplitRegister* reg;
    Account* leader;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));
    g_return_if_fail (key_file != NULL);
    g_return_if_fail (group_name != NULL);

    ENTER ("page %p, key_file %p, group_name %s", plugin_page, key_file,
           group_name);

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    reg = gnc_ledger_display_get_split_register (priv->ledger);
    ledger_type = gnc_ledger_display_type (priv->ledger);
    if (ledger_type > LD_GL)
    {
        LEAVE ("Unsupported ledger type");
        return;
    }
    if ((ledger_type == LD_SINGLE) || (ledger_type == LD_SUBACCOUNT))
    {
        const gchar* label;
        gchar* name;
        gchar acct_guid[GUID_ENCODING_LENGTH + 1];
        label = (ledger_type == LD_SINGLE) ? LABEL_ACCOUNT : LABEL_SUBACCOUNT;
        leader = gnc_ledger_display_leader (priv->ledger);
        g_key_file_set_string (key_file, group_name, KEY_REGISTER_TYPE, label);
        name = gnc_account_get_full_name (leader);
        g_key_file_set_string (key_file, group_name, KEY_ACCOUNT_NAME, name);
        g_free (name);
        guid_to_string_buff (xaccAccountGetGUID (leader), acct_guid);
        g_key_file_set_string (key_file, group_name, KEY_ACCOUNT_GUID, acct_guid);
    }
    else if (reg->type == GENERAL_JOURNAL)
    {
        g_key_file_set_string (key_file, group_name, KEY_REGISTER_TYPE,
                               LABEL_GL);
    }
    else if (reg->type == SEARCH_LEDGER)
    {
        g_key_file_set_string (key_file, group_name, KEY_REGISTER_TYPE,
                               LABEL_SEARCH);
    }
    else
    {
        LEAVE ("Unsupported register type");
        return;
    }

    g_key_file_set_string (key_file, group_name, KEY_REGISTER_STYLE,
                           style_names[reg->style]);
    g_key_file_set_boolean (key_file, group_name, KEY_DOUBLE_LINE,
                            reg->use_double_line);

    LEAVE(" ");
}


/** Read and restore the edit menu settings on the specified register
 *  page.  This function will restore the register style (ledger, auto
 *  ledger, journal) and whether or not the register is in double line
 *  mode.  It should eventually restore the "filter by" and "sort by
 *  settings.
 *
 *  @param page The register being restored.
 *
 *  @param key_file A pointer to the GKeyFile data structure where the
 *  page information should be read.
 *
 *  @param group_name The group name to use when restoring data. */
static void
gnc_plugin_page_register_restore_edit_menu (GncPluginPage* page,
                                            GKeyFile* key_file,
                                            const gchar* group_name)
{
    GAction* action;
    GVariant *state;
    GError* error = NULL;
    gchar* style_name;
    gint i;
    gboolean use_double_line;

    ENTER (" ");

    /* Convert the style name to an index */
    style_name = g_key_file_get_string (key_file, group_name,
                                        KEY_REGISTER_STYLE, &error);
    for (i = 0 ; style_names[i]; i++)
    {
        if (g_ascii_strcasecmp (style_name, style_names[i]) == 0)
        {
            DEBUG ("Found match for style name: %s", style_name);
            break;
        }
    }
    g_free (style_name);

    /* Update the style menu action for this page */
    if (i <= REG_STYLE_JOURNAL)
    {
        DEBUG ("Setting style: %d", i);
        action = gnc_plugin_page_get_action (page, "ViewStyleRadioAction");
        g_action_activate (G_ACTION(action), g_variant_new_int32 (i));
    }

    /* Update the  double line action on this page */
    use_double_line = g_key_file_get_boolean (key_file, group_name,
                                              KEY_DOUBLE_LINE, &error);
    DEBUG ("Setting double_line_mode: %d", use_double_line);
    action = gnc_plugin_page_get_action (page, "ViewStyleDoubleLineAction");

    state = g_action_get_state (G_ACTION(action));

    if (use_double_line != g_variant_get_boolean (state))
        g_action_activate (G_ACTION(action), NULL);

    g_variant_unref (state);

    LEAVE (" ");
}


/** Create a new register page based on the information saved during a
 *  previous instantiation of gnucash.
 *
 *  @param window The window where this page should be installed.
 *
 *  @param key_file A pointer to the GKeyFile data structure where the
 *  page information should be read.
 *
 *  @param group_name The group name to use when restoring data. */
static GncPluginPage*
gnc_plugin_page_register_recreate_page (GtkWidget* window,
                                        GKeyFile* key_file,
                                        const gchar* group_name)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* page;
    GError* error = NULL;
    gchar* reg_type, *acct_guid;
    GncGUID guid;
    Account* account = NULL;
    QofBook* book;
    gboolean include_subs;

    g_return_val_if_fail (key_file, NULL);
    g_return_val_if_fail (group_name, NULL);
    ENTER ("key_file %p, group_name %s", key_file, group_name);

    /* Create the new page. */
    reg_type = g_key_file_get_string (key_file, group_name,
                                      KEY_REGISTER_TYPE, &error);
    DEBUG ("Page type: %s", reg_type);
    if ((g_ascii_strcasecmp (reg_type, LABEL_ACCOUNT) == 0) ||
        (g_ascii_strcasecmp (reg_type, LABEL_SUBACCOUNT) == 0))
    {
        include_subs = (g_ascii_strcasecmp (reg_type, LABEL_SUBACCOUNT) == 0);
        DEBUG ("Include subs: %d", include_subs);
        book = qof_session_get_book (gnc_get_current_session());
        if (!book)
        {
            LEAVE("Session has no book");
            return NULL;
        }
        acct_guid = g_key_file_get_string (key_file, group_name,
                                           KEY_ACCOUNT_GUID, &error);
        if (string_to_guid (acct_guid, &guid)) //find account by guid
        {
            account = xaccAccountLookup (&guid, book);
            g_free (acct_guid);
        }
        if (account == NULL) //find account by full name
        {
            gchar* acct_name = g_key_file_get_string (key_file, group_name,
                                                      KEY_ACCOUNT_NAME, &error);
            account = gnc_account_lookup_by_full_name (gnc_book_get_root_account (book),
                                                       acct_name);
            g_free (acct_name);
        }
        if (account == NULL)
        {
            LEAVE ("Bad account name");
            g_free (reg_type);
            return NULL;
        }
        page = gnc_plugin_page_register_new (account, include_subs);
    }
    else if (g_ascii_strcasecmp (reg_type, LABEL_GL) == 0)
    {
        page = gnc_plugin_page_register_new_gl();
    }
    else
    {
        LEAVE ("Bad ledger type");
        g_free (reg_type);
        return NULL;
    }
    g_free (reg_type);

    /* disable the refresh of the display ledger, this is for
     * sort/filter updates and double line/style changes */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->enable_refresh = FALSE;

    /* Recreate page in given window */
    gnc_plugin_page_set_use_new_window (page, FALSE);

    /* Install it now so we can them manipulate the created widget */
    gnc_main_window_open_page (GNC_MAIN_WINDOW (window), page);

    /* Now update the page to the last state it was in */
    gnc_plugin_page_register_restore_edit_menu (page, key_file, group_name);

    /* enable the refresh */
    priv->enable_refresh = TRUE;
    LEAVE (" ");
    return page;
}


/*
 * Based on code from Epiphany (src/ephy-window.c)
 */
static void
gnc_plugin_page_register_update_edit_menu (GncPluginPage* page, gboolean hide)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPageRegister* reg_page;
    GAction* action;
    gboolean can_copy = FALSE, can_cut = FALSE, can_paste = FALSE;
    gboolean has_selection;
    gboolean is_readwrite = !qof_book_is_readonly (gnc_get_current_book());

    reg_page = GNC_PLUGIN_PAGE_REGISTER (page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (reg_page);
    has_selection = gnucash_register_has_selection (priv->gsr->reg);

    can_copy = has_selection;
    can_cut = is_readwrite && has_selection;
    can_paste = is_readwrite;

    action = gnc_plugin_page_get_action (page, "EditCopyAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), can_copy);
    action = gnc_plugin_page_get_action (page, "EditCutAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), can_cut);
    action = gnc_plugin_page_get_action (page, "EditPasteAction");
    g_simple_action_set_enabled (G_SIMPLE_ACTION(action), can_paste);
}

static gboolean is_scrubbing = FALSE;
static gboolean show_abort_verify = TRUE;

static const char*
check_repair_abort_YN = N_("'Check & Repair' is currently running, do you want to abort it?");

static gboolean
finish_scrub (GncPluginPage* page)
{
    gboolean ret = FALSE;

    if (is_scrubbing)
    {
        ret = gnc_verify_dialog (GTK_WINDOW(gnc_plugin_page_get_window (GNC_PLUGIN_PAGE(page))),
                                 false, "%s", _(check_repair_abort_YN));

        show_abort_verify = FALSE;

        if (ret)
            gnc_set_abort_scrub (TRUE);
    }
    return ret;
}

static gboolean
gnc_plugin_page_register_finish_pending (GncPluginPage* page)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPageRegister* reg_page;
    SplitRegister* reg;
    GtkWidget* dialog, *window;
    gchar* name;
    gint response;

    if (is_scrubbing && show_abort_verify)
    {
        if (!finish_scrub (page))
            return FALSE;
    }

    reg_page = GNC_PLUGIN_PAGE_REGISTER (page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (reg_page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    if (!reg || !gnc_split_register_changed (reg))
        return TRUE;

    name = gnc_plugin_page_register_get_tab_name (page);
    window = gnc_plugin_page_get_window (page);
    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_NONE,
                                     /* Translators: %s is the name
                                        of the tab page */
                                     _ ("Save changes to %s?"), name);
    g_free (name);
    gtk_message_dialog_format_secondary_text
    (GTK_MESSAGE_DIALOG (dialog),
     "%s",
     _ ("This register has pending changes to a transaction. "
        "Would you like to save the changes to this transaction, "
        "discard the transaction, or cancel the operation?"));
    gnc_gtk_dialog_add_button (dialog, _ ("_Discard Transaction"),
                               "edit-delete", GTK_RESPONSE_REJECT);
    gtk_dialog_add_button (GTK_DIALOG (dialog),
                           _ ("_Cancel"), GTK_RESPONSE_CANCEL);
    gnc_gtk_dialog_add_button (dialog, _ ("_Save Transaction"),
                               "document-save", GTK_RESPONSE_ACCEPT);

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    switch (response)
    {
    case GTK_RESPONSE_ACCEPT:
        gnc_split_register_save (reg, TRUE);
        return TRUE;

    case GTK_RESPONSE_REJECT:
        gnc_split_register_cancel_cursor_trans_changes (reg);
        gnc_split_register_save (reg, TRUE);
        return TRUE;

    default:
        return FALSE;
    }
}


static gchar*
gnc_plugin_page_register_get_tab_name (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    GNCLedgerDisplay* ld;
    SplitRegister* reg;
    Account* leader;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page),
                          g_strdup (_("unknown")));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);
    ld = priv->ledger;
    reg = gnc_ledger_display_get_split_register (ld);
    ledger_type = gnc_ledger_display_type (ld);
    leader = gnc_ledger_display_leader (ld);

    switch (ledger_type)
    {
    case LD_SINGLE:
        return g_strdup (xaccAccountGetName (leader));

    case LD_SUBACCOUNT:
        return g_strdup_printf ("%s+", xaccAccountGetName (leader));

    case LD_GL:
        switch (reg->type)
        {
        case GENERAL_JOURNAL:
        case INCOME_LEDGER:
            return g_strdup (_ ("General Journal"));
        case PORTFOLIO_LEDGER:
            return g_strdup (_ ("Portfolio"));
        case SEARCH_LEDGER:
            return g_strdup (_ ("Search Results"));
        default:
            break;
        }
        break;

    default:
        break;
    }

    return g_strdup (_ ("unknown"));
}

static gchar*
gnc_plugin_page_register_get_tab_color (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    GNCLedgerDisplay* ld;
    Account* leader;
    const char* color;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page),
                          g_strdup (_("unknown")));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);
    ld = priv->ledger;
    ledger_type = gnc_ledger_display_type (ld);
    leader = gnc_ledger_display_leader (ld);
    color = NULL;

    if ((ledger_type == LD_SINGLE) || (ledger_type == LD_SUBACCOUNT))
        color = xaccAccountGetColor (leader);

    return g_strdup (color ? color : "Not Set");
}

static void
gnc_plugin_page_register_check_for_empty_group (GKeyFile *state_file,
                                                const gchar *state_section)
{
    gsize num_keys;
    gchar **keys = g_key_file_get_keys (state_file, state_section, &num_keys, NULL);

    if (num_keys == 0)
        gnc_state_drop_sections_for (state_section);

    g_strfreev (keys);
}

static gchar*
gnc_plugin_page_register_get_filter_gcm (GNCSplitReg *gsr)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;
    GError* error = NULL;
    char* filter = NULL;

    // get the filter from the .gcm file
    state_section = gsr_get_register_state_section (gsr);
    filter = g_key_file_get_string (state_file, state_section,
                                    KEY_PAGE_FILTER, &error);

    if (error)
        g_clear_error (&error);
    else
        g_strdelimit (filter, ";", ',');

    g_free (state_section);
    return filter;
}

static gchar*
gnc_plugin_page_register_get_filter (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    char* filter = NULL;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page),
                          g_strdup (_("unknown")));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    ledger_type = gnc_ledger_display_type (priv->ledger);

    // load from gcm file
    filter = gnc_plugin_page_register_get_filter_gcm (priv->gsr);

    if (filter)
        return filter;

    return g_strdup_printf ("%s,%s,%s,%s", DEFAULT_FILTER,
                            "0", "0", get_filter_default_num_of_days (ledger_type));
}

static void
gnc_plugin_page_register_set_filter_gcm (GNCSplitReg *gsr, const gchar* filter,
                                         gchar* default_filter)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;
    gchar* filter_text;

    // save the filter to the .gcm file also
    state_section = gsr_get_register_state_section (gsr);
    if (!filter || (g_strcmp0 (filter, default_filter) == 0))
    {
        if (g_key_file_has_key (state_file, state_section, KEY_PAGE_FILTER, NULL))
            g_key_file_remove_key (state_file, state_section, KEY_PAGE_FILTER, NULL);

        gnc_plugin_page_register_check_for_empty_group (state_file, state_section);
    }
    else
    {
        filter_text = g_strdup (filter);
        g_strdelimit (filter_text, ",", ';'); // make it conform to .gcm file list
        g_key_file_set_string (state_file, state_section, KEY_PAGE_FILTER,
                               filter_text);
        g_free (filter_text);
    }
    g_free (state_section);
}

void
gnc_plugin_page_register_set_filter (GncPluginPage* plugin_page,
                                     const gchar* filter)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    gchar* default_filter;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    ledger_type = gnc_ledger_display_type (priv->ledger);

    default_filter = g_strdup_printf ("%s,%s,%s,%s", DEFAULT_FILTER,
                                      "0", "0", get_filter_default_num_of_days (ledger_type));

    // save to gcm file
    gnc_plugin_page_register_set_filter_gcm (priv->gsr, filter, default_filter);

    g_free (default_filter);
    return;
}

static gchar*
gnc_plugin_page_register_get_sort_order_gcm (GNCSplitReg *gsr)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;
    gchar* sort_text;
    GError* error = NULL;
    char* sort_order = NULL;

    // get the sort_order from the .gcm file
    state_section = gsr_get_register_state_section (gsr);
    sort_text = g_key_file_get_string (state_file, state_section, KEY_PAGE_SORT,
                                       &error);

    if (error)
        g_clear_error (&error);
    else
    {
        sort_order = g_strdup (sort_text);
        g_free (sort_text);
    }
    g_free (state_section);
    return sort_order;
}

static gchar*
gnc_plugin_page_register_get_sort_order (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    char* sort_order = NULL;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page),
                          _ ("unknown"));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    // load from gcm file
    sort_order = gnc_plugin_page_register_get_sort_order_gcm (priv->gsr);

    return sort_order ? sort_order : g_strdup (DEFAULT_SORT_ORDER);
}

static void
gnc_plugin_page_register_set_sort_order_gcm (GNCSplitReg *gsr,
                                             const gchar* sort_order)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;

    // save sort_order to the .gcm file also
    state_section = gsr_get_register_state_section (gsr);
    if (!sort_order || (g_strcmp0 (sort_order, DEFAULT_SORT_ORDER) == 0))
    {
        if (g_key_file_has_key (state_file, state_section, KEY_PAGE_SORT, NULL))
            g_key_file_remove_key (state_file, state_section, KEY_PAGE_SORT, NULL);

        gnc_plugin_page_register_check_for_empty_group (state_file, state_section);
    }
    else
        g_key_file_set_string (state_file, state_section, KEY_PAGE_SORT, sort_order);

    g_free (state_section);
}
void
gnc_plugin_page_register_set_sort_order (GncPluginPage* plugin_page,
                                         const gchar* sort_order)
{
    GncPluginPageRegisterPrivate* priv;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    // save to gcm file
    gnc_plugin_page_register_set_sort_order_gcm (priv->gsr, sort_order);
}

static gboolean
gnc_plugin_page_register_get_sort_reversed_gcm (GNCSplitReg *gsr)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;
    GError* error = NULL;
    gboolean sort_reversed = FALSE;

    // get the sort_reversed from the .gcm file
    state_section = gsr_get_register_state_section (gsr);
    sort_reversed = g_key_file_get_boolean (state_file, state_section,
                                            KEY_PAGE_SORT_REV, &error);

    if (error)
        g_clear_error (&error);

    g_free (state_section);
    return sort_reversed;
}

static gboolean
gnc_plugin_page_register_get_sort_reversed (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    gboolean sort_reversed = FALSE;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page), FALSE);

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    // load from gcm file
    sort_reversed = gnc_plugin_page_register_get_sort_reversed_gcm (priv->gsr);
    return sort_reversed;
}

static void
gnc_plugin_page_register_set_sort_reversed_gcm (GNCSplitReg *gsr,
                                                gboolean reverse_order)
{
    GKeyFile* state_file = gnc_state_get_current();
    gchar* state_section;

    // save reverse_order to the .gcm file also
    state_section = gsr_get_register_state_section (gsr);

    if (!reverse_order)
    {
        if (g_key_file_has_key (state_file, state_section, KEY_PAGE_SORT_REV, NULL))
            g_key_file_remove_key (state_file, state_section, KEY_PAGE_SORT_REV, NULL);

        gnc_plugin_page_register_check_for_empty_group (state_file, state_section);
    }
    else
        g_key_file_set_boolean (state_file, state_section, KEY_PAGE_SORT_REV,
                                reverse_order);

    g_free (state_section);
}

void
gnc_plugin_page_register_set_sort_reversed (GncPluginPage* plugin_page,
                                            gboolean reverse_order)
{
    GncPluginPageRegisterPrivate* priv;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    // save to gcm file
    gnc_plugin_page_register_set_sort_reversed_gcm (priv->gsr, reverse_order);
}

static gchar*
gnc_plugin_page_register_get_long_name (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    GNCLedgerDisplay* ld;
    Account* leader;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page),
                          _ ("unknown"));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);
    ld = priv->ledger;
    ledger_type = gnc_ledger_display_type (ld);
    leader = gnc_ledger_display_leader (ld);

    switch (ledger_type)
    {
    case LD_SINGLE:
        return gnc_account_get_full_name (leader);

    case LD_SUBACCOUNT:
    {
        gchar* account_full_name = gnc_account_get_full_name (leader);
        gchar* return_string = g_strdup_printf ("%s+", account_full_name);
        g_free ((gpointer*) account_full_name);
        return return_string;
    }

    default:
        break;
    }

    return NULL;
}

static void
gnc_plugin_page_register_summarybar_position_changed (gpointer prefs,
                                                      gchar* pref,
                                                      gpointer user_data)
{
    GncPluginPage* plugin_page;
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;
    GtkPositionType position = GTK_POS_BOTTOM;

    g_return_if_fail (user_data != NULL);

    if (!GNC_IS_PLUGIN_PAGE (user_data))
        return;

    plugin_page = GNC_PLUGIN_PAGE (user_data);
    page = GNC_PLUGIN_PAGE_REGISTER (user_data);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    if (priv == NULL)
        return;

    if (gnc_prefs_get_bool (GNC_PREFS_GROUP_GENERAL,
                            GNC_PREF_SUMMARYBAR_POSITION_TOP))
        position = GTK_POS_TOP;

    gtk_box_reorder_child (GTK_BOX (priv->widget),
                           plugin_page->summarybar,
                           (position == GTK_POS_TOP ? 0 : -1));
}

/** This function is called to get the query associated with this
 *  plugin page.
 *
 *  @param page A pointer to the GncPluginPage.
 */
Query*
gnc_plugin_page_register_get_query (GncPluginPage* plugin_page)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page), NULL);

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    return gnc_ledger_display_get_query (priv->ledger);
}

/************************************************************/
/*                     "Sort By" Dialog                     */
/************************************************************/

/** This function is called whenever the number source book options is changed
 *  to adjust the displayed labels. Since the book option change may change the
 *  query sort, the gnc_split_reg_set_sort_type_force function is called to
 *  ensure the page is refreshed.
 *
 *  @param new_val A pointer to the boolean for the new value of the book option.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this sort order dialog.
 */
static void
gnc_plugin_page_register_sort_book_option_changed (gpointer new_val,
                                                   gpointer user_data)
{
    GncPluginPageRegisterPrivate* priv;
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    gboolean* new_data = (gboolean*)new_val;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (*new_data)
    {
        gtk_button_set_label (GTK_BUTTON (priv->sd.num_radio),
                              _ ("Transaction Number"));
        gtk_button_set_label (GTK_BUTTON (priv->sd.act_radio), _ ("Number/Action"));
    }
    else
    {
        gtk_button_set_label (GTK_BUTTON (priv->sd.num_radio), _ ("Number"));
        gtk_button_set_label (GTK_BUTTON (priv->sd.act_radio), _ ("Action"));
    }
    gnc_split_reg_set_sort_type_force (priv->gsr, (SortType)priv->gsr->sort_type, TRUE);
}

/** This function is called when the "Sort By…" dialog is closed.
 *  If the dialog was closed by any method other than clicking the OK
 *  button, the original sorting order will be restored.
 *
 *  @param dialog A pointer to the dialog box.
 *
 *  @param response A numerical value indicating why the dialog box was closed.
 *
 *  @param page A pointer to the GncPluginPageRegister associated with
 *  this dialog box.
 */
void
gnc_plugin_page_register_sort_response_cb (GtkDialog* dialog,
                                           gint response,
                                           GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* plugin_page;
    SortType type;
    const gchar* order;

    g_return_if_fail (GTK_IS_DIALOG (dialog));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER (" ");
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    plugin_page = GNC_PLUGIN_PAGE (page);

    if (response != GTK_RESPONSE_OK)
    {
        /* Restore the original sort order */
        gnc_split_reg_set_sort_reversed (priv->gsr, priv->sd.original_reverse_order,
                                         TRUE);
        priv->sd.reverse_order = priv->sd.original_reverse_order;
        gnc_split_reg_set_sort_type (priv->gsr, priv->sd.original_sort_type);
        priv->sd.save_order = priv->sd.original_save_order;
    }
    else
    {
        // clear the sort when unticking the save option
        if ((!priv->sd.save_order) && ((priv->sd.original_save_order) || (priv->sd.original_reverse_order)))
        {
            gnc_plugin_page_register_set_sort_order (plugin_page, DEFAULT_SORT_ORDER);
            gnc_plugin_page_register_set_sort_reversed (plugin_page, FALSE);
        }
        priv->sd.original_save_order = priv->sd.save_order;

        if (priv->sd.save_order)
        {
            type = gnc_split_reg_get_sort_type (priv->gsr);
            order = SortTypeasString (type);
            gnc_plugin_page_register_set_sort_order (plugin_page, order);
            gnc_plugin_page_register_set_sort_reversed (plugin_page,
                                                        priv->sd.reverse_order);
        }
    }
    gnc_book_option_remove_cb (OPTION_NAME_NUM_FIELD_SOURCE,
                               gnc_plugin_page_register_sort_book_option_changed,
                               page);
    priv->sd.dialog = NULL;
    priv->sd.num_radio = NULL;
    priv->sd.act_radio = NULL;
    gtk_widget_destroy (GTK_WIDGET (dialog));
    LEAVE (" ");
}


/** This function is called when a radio button in the "Sort By…"
 *  dialog is clicked.
 *
 *  @param button The button that was toggled.
 *
 *  @param page A pointer to the GncPluginPageRegister associated with
 *  this dialog box.
 */
void
gnc_plugin_page_register_sort_button_cb (GtkToggleButton* button,
                                         GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    const gchar* name;
    SortType type;

    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    name = gtk_buildable_get_name (GTK_BUILDABLE (button));
    ENTER ("button %s(%p), page %p", name, button, page);
    type = SortTypefromString (name);
    gnc_split_reg_set_sort_type (priv->gsr, type);
    LEAVE (" ");
}


/** This function is called whenever the save sort order is checked
 *  or unchecked which allows saving of the sort order.
 *
 *  @param button The toggle button that was changed.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this sort order dialog.
 */
void
gnc_plugin_page_register_sort_order_save_cb (GtkToggleButton* button,
                                             GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GTK_IS_CHECK_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("Save toggle button (%p), plugin_page %p", button, page);

    /* Compute the new save sort order */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    if (gtk_toggle_button_get_active (button))
        priv->sd.save_order = TRUE;
    else
        priv->sd.save_order = FALSE;
    LEAVE (" ");
}

/** This function is called whenever the reverse sort order is checked
 *  or unchecked which allows reversing of the sort order.
 *
 *  @param button The toggle button that was changed.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this sort order dialog.
 */
void
gnc_plugin_page_register_sort_order_reverse_cb (GtkToggleButton* button,
                                                GncPluginPageRegister* page)

{
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GTK_IS_CHECK_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("Reverse toggle button (%p), plugin_page %p", button, page);

    /* Compute the new save sort order */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    priv->sd.reverse_order = gtk_toggle_button_get_active (button);
    gnc_split_reg_set_sort_reversed (priv->gsr, priv->sd.reverse_order, TRUE);
    LEAVE (" ");
}

/************************************************************/
/*                    "Filter By" Dialog                    */
/************************************************************/

static void
gnc_ppr_update_for_search_query (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    if (reg->type == SEARCH_LEDGER)
    {
        Query* query_tmp = gnc_ledger_display_get_query (priv->ledger);

        // if filter_query is NULL, then the dialogue find has been run
        // before coming here. if query_tmp does not equal filter_query
        // then the dialogue find has been run again before coming here
        if ((priv->filter_query == NULL) ||
            (!qof_query_equal (query_tmp, priv->filter_query)))
        {
            qof_query_destroy (priv->search_query);
            priv->search_query = qof_query_copy (query_tmp);
        }
        gnc_ledger_display_set_query (priv->ledger, priv->search_query);
    }
}


/** This function updates the "cleared match" term of the register
 *  query.  It unconditionally removes any old "cleared match" query
 *  term, then adds back a new query term if needed.  There seems to
 *  be a bug in the current g2 register code such that when the number
 *  of entries in the register doesn't fill up the window, the blank
 *  space at the end of the window isn't correctly redrawn.  This
 *  function works around that problem, but a root cause analysis
 *  should probably be done.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
static void
gnc_ppr_update_status_query (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    Query* query;
    SplitRegister* reg;

    ENTER (" ");
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (!priv->ledger)
    {
        LEAVE ("no ledger");
        return;
    }
    // check if this a search register and save query
    gnc_ppr_update_for_search_query (page);

    query = gnc_ledger_display_get_query (priv->ledger);
    if (!query)
    {
        LEAVE ("no query found");
        return;
    }

    reg = gnc_ledger_display_get_split_register (priv->ledger);

    /* Remove the old status match */
    if (reg->type != SEARCH_LEDGER)
    {
        GSList *param_list = qof_query_build_param_list (SPLIT_RECONCILE, NULL);
        qof_query_purge_terms (query, param_list);
        g_slist_free (param_list);
    }

    /* Install the new status match */
    if (priv->fd.cleared_match != CLEARED_ALL)
        xaccQueryAddClearedMatch (query, priv->fd.cleared_match, QOF_QUERY_AND);

    // Set filter tooltip for summary bar
    gnc_plugin_page_register_set_filter_tooltip (page);

    // clear previous filter query and save current
    qof_query_destroy (priv->filter_query);
    priv->filter_query = qof_query_copy (query);

    if (priv->enable_refresh)
        gnc_ledger_display_refresh (priv->ledger);
    LEAVE (" ");
}


/** This function updates the "date posted" term of the register
 *  query.  It unconditionally removes any old "date posted" query
 *  term, then adds back a new query term if needed.  There seems to
 *  be a bug in the current g2 register code such that when the number
 *  of entries in the register doesn't fill up the window, the blank
 *  space at the end of the window isn't correctly redrawn.  This
 *  function works around that problem, but a root cause analysis
 *  should probably be done.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
static void
gnc_ppr_update_date_query (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    Query* query;
    SplitRegister* reg;

    ENTER (" ");
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (!priv->ledger)
    {
        LEAVE ("no ledger");
        return;
    }
    // check if this a search register and save query
    gnc_ppr_update_for_search_query (page);

    query = gnc_ledger_display_get_query (priv->ledger);

    if (!query)
    {
        LEAVE ("no query");
        return;
    }

    reg = gnc_ledger_display_get_split_register (priv->ledger);

    /* Delete any existing old date spec. */
    if (reg->type != SEARCH_LEDGER)
    {
        GSList *param_list = qof_query_build_param_list (SPLIT_TRANS,
                                                         TRANS_DATE_POSTED, NULL);
        qof_query_purge_terms (query, param_list);
        g_slist_free (param_list);
    }

    if (priv->fd.start_time || priv->fd.end_time)
    {
        /* Build a new spec */
        xaccQueryAddDateMatchTT (query,
                                 priv->fd.start_time != 0, priv->fd.start_time,
                                 priv->fd.end_time != 0,   priv->fd.end_time,
                                 QOF_QUERY_AND);
    }

    if (priv->fd.days > 0)
    {
        time64 start;
        struct tm tm;

        gnc_tm_get_today_start (&tm);

        tm.tm_mday = tm.tm_mday - priv->fd.days;
        start = gnc_mktime (&tm);
        xaccQueryAddDateMatchTT (query, TRUE, start, FALSE, 0, QOF_QUERY_AND);
    }

    // Set filter tooltip for summary bar
    gnc_plugin_page_register_set_filter_tooltip (page);

    // clear previous filter query and save current
    qof_query_destroy (priv->filter_query);
    priv->filter_query = qof_query_copy (query);

    if (priv->enable_refresh)
        gnc_ledger_display_refresh (priv->ledger);
    LEAVE (" ");
}


/* This function converts a time64 value date to a string */
static gchar*
gnc_plugin_page_register_filter_time2dmy (time64 raw_time)
{
    struct tm* timeinfo;
    gchar date_string[11];

    timeinfo = gnc_localtime (&raw_time);
    strftime (date_string, 11, "%d-%m-%Y", timeinfo);
    PINFO ("Date string is %s", date_string);
    gnc_tm_free (timeinfo);

    return g_strdup (date_string);
}


/* This function converts a string date to a time64 value */
static time64
gnc_plugin_page_register_filter_dmy2time (char* date_string)
{
    struct tm when;

    PINFO ("Date string is %s", date_string);
    memset (&when, 0, sizeof (when));

    sscanf (date_string, "%d-%d-%d", &when.tm_mday,
            &when.tm_mon, &when.tm_year);

    when.tm_mon -= 1;
    when.tm_year -= 1900;

    return gnc_mktime (&when);
}


/** This function is called whenever one of the status entries is
 *  checked or unchecked.  It updates the status value maintained for
 *  the filter dialog, and calls another function to do the work of
 *  applying the change to the register itself.
 *
 *  @param button The toggle button that was changed.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_status_one_cb (GtkToggleButton* button,
                                               GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    const gchar* name;
    gint i, value;

    g_return_if_fail (GTK_IS_CHECK_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    name = gtk_buildable_get_name (GTK_BUILDABLE (button));
    ENTER ("toggle button %s (%p), plugin_page %p", name, button, page);

    /* Determine what status bit to change */
    value = CLEARED_NONE;
    for (i = 0; status_actions[i].action_name; i++)
    {
        if (g_strcmp0 (name, status_actions[i].action_name) == 0)
        {
            value = status_actions[i].value;
            break;
        }
    }

    /* Compute the new match status */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (gtk_toggle_button_get_active (button))
        priv->fd.cleared_match = (cleared_match_t)(priv->fd.cleared_match | value);
    else
        priv->fd.cleared_match = (cleared_match_t)(priv->fd.cleared_match & ~value);
    gnc_ppr_update_status_query (page);
    LEAVE (" ");
}


/** This function is called whenever the "select all" status button is
 *  clicked.  It updates all of the checkbox widgets, then updates the
 *  query on the register.
 *
 *  @param button The button that was clicked.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_status_select_all_cb (GtkButton* button,
                                                      GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GtkWidget* widget;
    gint i;

    g_return_if_fail (GTK_IS_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(button %p, page %p)", button, page);

    /* Turn on all the check menu items */
    for (i = 0; status_actions[i].action_name; i++)
    {
        widget = status_actions[i].widget;
        g_signal_handlers_block_by_func (widget,
                                         (gpointer)gnc_plugin_page_register_filter_status_one_cb, page);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        g_signal_handlers_unblock_by_func (widget,
                                           (gpointer)gnc_plugin_page_register_filter_status_one_cb, page);
    }

    /* Set the requested status */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->fd.cleared_match = CLEARED_ALL;
    gnc_ppr_update_status_query (page);
    LEAVE (" ");
}



/** This function is called whenever the "clear all" status button is
 *  clicked.  It updates all of the checkbox widgets, then updates the
 *  query on the register.
 *
 *  @param button The button that was clicked.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_status_clear_all_cb (GtkButton* button,
                                                     GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GtkWidget* widget;
    gint i;

    g_return_if_fail (GTK_IS_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(button %p, page %p)", button, page);

    /* Turn off all the check menu items */
    for (i = 0; status_actions[i].action_name; i++)
    {
        widget = status_actions[i].widget;
        g_signal_handlers_block_by_func (widget,
                                         (gpointer)gnc_plugin_page_register_filter_status_one_cb, page);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
        g_signal_handlers_unblock_by_func (widget,
                                           (gpointer)gnc_plugin_page_register_filter_status_one_cb, page);
    }

    /* Set the requested status */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->fd.cleared_match = CLEARED_NONE;
    gnc_ppr_update_status_query (page);
    LEAVE (" ");
}


/** This function computes the starting and ending times for the
 *  filter by examining the dialog widgets to see which ones are
 *  selected, and will pull times out of the data entry boxes if
 *  necessary.  This function must exist to handle the case where the
 *  "show all" button was Selected, and the user clicks on the "select
 *  range" button.  Since it exists, it make sense for the rest of the
 *  callbacks to take advantage of it.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
static void
get_filter_times (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    time64 time_val;

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
                                          priv->fd.start_date_choose)))
    {
        time_val = gnc_date_edit_get_date (GNC_DATE_EDIT (priv->fd.start_date));
        time_val = gnc_time64_get_day_start (time_val);
        priv->fd.start_time = time_val;
    }
    else
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
                                              priv->fd.start_date_today)))
        {
            priv->fd.start_time = gnc_time64_get_today_start();
        }
        else
        {
            priv->fd.start_time = 0;
        }
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
                                          priv->fd.end_date_choose)))
    {
        time_val = gnc_date_edit_get_date (GNC_DATE_EDIT (priv->fd.end_date));
        time_val = gnc_time64_get_day_end (time_val);
        priv->fd.end_time = time_val;
    }
    else
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
                                              priv->fd.end_date_today)))
        {
            priv->fd.end_time = gnc_time64_get_today_end();
        }
        else
        {
            priv->fd.end_time = 0;
        }
    }
}


/** This function is called when the radio buttons changes state. This
 *  function is responsible for setting the sensitivity of the widgets
 *  controlled by each radio button choice and updating the time
 *  limitation on the register query. This is handled by a helper
 *  function as potentially all widgets will need to be examined.
 *
 *  @param button A pointer to the "select range" radio button.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_select_range_cb (GtkRadioButton* button,
                                                 GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    gboolean active;
    const gchar* name;

    g_return_if_fail (GTK_IS_RADIO_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(button %p, page %p)", button, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    name = gtk_buildable_get_name (GTK_BUILDABLE (button));
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    if (active && g_strcmp0 (name, "filter_show_range") == 0)
    {
        gtk_widget_set_sensitive (priv->fd.table, active);
        gtk_widget_set_sensitive (priv->fd.num_days, !active);
        get_filter_times (page);
    }
    else if (active && g_strcmp0 (name, "filter_show_days") == 0)
    {
        gtk_widget_set_sensitive (priv->fd.table, !active);
        gtk_widget_set_sensitive (priv->fd.num_days, active);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->fd.num_days), priv->fd.days);
    }
    else
    {
        gtk_widget_set_sensitive (priv->fd.table, FALSE);
        gtk_widget_set_sensitive (priv->fd.num_days, FALSE);
        priv->fd.days = 0;
        priv->fd.start_time = 0;
        priv->fd.end_time = 0;
    }
    gnc_ppr_update_date_query (page);
    LEAVE (" ");
}

void
gnc_plugin_page_register_clear_current_filter (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    priv->fd.days = 0;
    priv->fd.start_time = 0;
    priv->fd.end_time = 0;
    priv->fd.cleared_match = (cleared_match_t)g_ascii_strtoll (DEFAULT_FILTER, NULL, 16);

    gnc_ppr_update_date_query (GNC_PLUGIN_PAGE_REGISTER(plugin_page));
}

/** This function is called when the "number of days" spin button is
 *  changed which is then saved and updates the time limitation on
 *  the register query. This is handled by a helper function as
 *  potentially all widgets will need to be examined.
 *
 *  @param button A pointer to the "number of days" spin button.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_days_changed_cb (GtkSpinButton* button,
                                                 GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GTK_IS_SPIN_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(button %p, page %p)", button, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    priv->fd.days = gtk_spin_button_get_value (GTK_SPIN_BUTTON (button));
    gnc_ppr_update_date_query (page);
    LEAVE (" ");
}


/** This function is called when one of the start date entry widgets
 *  is updated.  It simply calls common routines to determine the
 *  start/end times and update the register query.
 *
 *  @param unused A pointer to a GncDateEntry widgets, but it could be
 *  any widget.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
static void
gnc_plugin_page_register_filter_gde_changed_cb (GtkWidget* unused,
                                                GncPluginPageRegister* page)
{
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(widget %s(%p), page %p)",
           gtk_buildable_get_name (GTK_BUILDABLE (unused)), unused, page);
    get_filter_times (page);
    gnc_ppr_update_date_query (page);
    LEAVE (" ");
}


/** This function is called when one of the start date radio buttons
 *  is selected.  It updates the sensitivity of the date entry widget,
 *  then calls a common routine to determine the start/end times and
 *  update the register query.
 *
 *  *Note: This function is actually called twice for each new radio
 *  button selection.  The first time call is to uncheck the old
 *  button, and the second time to check the new button.  This does
 *  make a kind of sense, as radio buttons are nothing more than
 *  linked toggle buttons where only one can be active.
 *
 *  @param radio The button whose state is changing.  This will be
 *  the previously selected button the first of the pair of calls to
 *  this function, and will be the newly selected button the second
 *  time.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_start_cb (GtkWidget* radio,
                                          GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    const gchar* name;
    gboolean active;

    g_return_if_fail (GTK_IS_RADIO_BUTTON (radio));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(radio %s(%p), page %p)",
           gtk_buildable_get_name (GTK_BUILDABLE (radio)), radio, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
    {
        LEAVE ("1st callback of pair. Defer to 2nd callback.");
        return;
    }

    name = gtk_buildable_get_name (GTK_BUILDABLE (radio));
    active = !g_strcmp0 (name, "start_date_choose");
    gtk_widget_set_sensitive (priv->fd.start_date, active);
    get_filter_times (page);
    gnc_ppr_update_date_query (page);
    LEAVE (" ");
}


/** This function is called when one of the end date radio buttons is
 *  selected.  It updates the sensitivity of the date entry widget,
 *  then calls a common routine to determine the start/end times and
 *  update the register query.
 *
 *  *Note: This function is actually called twice for each new radio
 *  button selection.  The first time call is to uncheck the old
 *  button, and the second time to check the new button.  This does
 *  make a kind of sense, as radio buttons are nothing more than
 *  linked toggle buttons where only one can be active.
 *
 *  @param radio The button whose state is changing.  This will be
 *  the previously selected button the first of the pair of calls to
 *  this function, and will be the newly selected button the second
 *  time.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_end_cb (GtkWidget* radio,
                                        GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    const gchar* name;
    gboolean active;

    g_return_if_fail (GTK_IS_RADIO_BUTTON (radio));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(radio %s(%p), page %p)",
           gtk_buildable_get_name (GTK_BUILDABLE (radio)), radio, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
    {
        LEAVE ("1st callback of pair. Defer to 2nd callback.");
        return;
    }

    name = gtk_buildable_get_name (GTK_BUILDABLE (radio));
    active = !g_strcmp0 (name, "end_date_choose");
    gtk_widget_set_sensitive (priv->fd.end_date, active);
    get_filter_times (page);
    gnc_ppr_update_date_query (page);
    LEAVE (" ");
}


/** This function is called whenever the save status is checked
 *  or unchecked. It will allow saving of the filter if required.
 *
 *  @param button The toggle button that was changed.
 *
 *  @param page A pointer to the GncPluginPageRegister that is
 *  associated with this filter dialog.
 */
void
gnc_plugin_page_register_filter_save_cb (GtkToggleButton* button,
                                         GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GTK_IS_CHECK_BUTTON (button));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("Save toggle button (%p), plugin_page %p", button, page);

    /* Compute the new save filter status */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (gtk_toggle_button_get_active (button))
        priv->fd.save_filter = TRUE;
    else
        priv->fd.save_filter = FALSE;
    LEAVE (" ");
}


/** This function is called when the "Filter By…" dialog is closed.
 *  If the dialog was closed by any method other than clicking the OK
 *  button, the original sorting order will be restored.
 *
 *  @param dialog A pointer to the dialog box.
 *
 *  @param response A numerical value indicating why the dialog box was closed.
 *
 *  @param page A pointer to the GncPluginPageRegister associated with
 *  this dialog box.
 */
void
gnc_plugin_page_register_filter_response_cb (GtkDialog* dialog,
                                             gint response,
                                             GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* plugin_page;

    g_return_if_fail (GTK_IS_DIALOG (dialog));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER (" ");
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    plugin_page = GNC_PLUGIN_PAGE (page);

    if (response != GTK_RESPONSE_OK)
    {
        /* Remove the old status match */
        priv->fd.cleared_match = priv->fd.original_cleared_match;
        priv->enable_refresh = FALSE;
        gnc_ppr_update_status_query (page);
        priv->enable_refresh = TRUE;
        priv->fd.start_time = priv->fd.original_start_time;
        priv->fd.end_time = priv->fd.original_end_time;
        priv->fd.days = priv->fd.original_days;
        priv->fd.save_filter = priv->fd.original_save_filter;
        gnc_ppr_update_date_query (page);
    }
    else
    {
        // clear the filter when unticking the save option
        if ((priv->fd.save_filter == FALSE) && (priv->fd.original_save_filter == TRUE))
            gnc_plugin_page_register_set_filter (plugin_page, NULL);

        priv->fd.original_save_filter = priv->fd.save_filter;

        if (priv->fd.save_filter)
        {
            gchar *filter;
            GList *flist = NULL;

            // cleared match
            flist = g_list_prepend
                (flist, g_strdup_printf ("0x%04x", priv->fd.cleared_match));

            // start time
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->fd.start_date_choose)) && priv->fd.start_time != 0)
                flist = g_list_prepend (flist, gnc_plugin_page_register_filter_time2dmy (priv->fd.start_time));
            else
                flist = g_list_prepend (flist, g_strdup ("0"));

            // end time
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->fd.end_date_choose))
                && priv->fd.end_time != 0)
                flist = g_list_prepend (flist, gnc_plugin_page_register_filter_time2dmy (priv->fd.end_time));
            else
                flist = g_list_prepend (flist, g_strdup ("0"));

            // number of days
            if (priv->fd.days > 0)
                flist = g_list_prepend (flist, g_strdup_printf ("%d", priv->fd.days));
            else
                flist = g_list_prepend (flist, g_strdup ("0"));

            flist = g_list_reverse (flist);
            filter = gnc_g_list_stringjoin (flist, ",");
            PINFO ("The filter to save is %s", filter);
            gnc_plugin_page_register_set_filter (plugin_page, filter);
            g_free (filter);
            g_list_free_full (flist, g_free);
        }
    }
    priv->fd.dialog = NULL;
    gtk_widget_destroy (GTK_WIDGET (dialog));
    LEAVE (" ");
}

static void
gpp_update_match_filter_text (cleared_match_t match, const guint mask,
                              const gchar* filter_name, GList **show, GList **hide)
{
    if ((match & mask) == mask)
        *show = g_list_prepend (*show, g_strdup (filter_name));
    else
        *hide = g_list_prepend (*hide, g_strdup (filter_name));
}

static void
gnc_plugin_page_register_set_filter_tooltip (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv;
    GList *t_list = NULL;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER (" ");
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    // filtered start time
    if (priv->fd.start_time != 0)
    {
        gchar* sdate = qof_print_date (priv->fd.start_time);
        t_list = g_list_prepend
            (t_list, g_strdup_printf ("%s %s", _("Start Date:"), sdate));
        g_free (sdate);
    }

    // filtered number of days
    if (priv->fd.days > 0)
        t_list = g_list_prepend
            (t_list, g_strdup_printf ("%s %d", _("Show previous number of days:"),
                                      priv->fd.days));

    // filtered end time
    if (priv->fd.end_time != 0)
    {
        gchar* edate = qof_print_date (priv->fd.end_time);
        t_list = g_list_prepend
            (t_list, g_strdup_printf ("%s %s", _("End Date:"), edate));
        g_free (edate);
    }

    // filtered match items
    if (priv->fd.cleared_match != CLEARED_ALL)
    {
        GList *show = NULL;
        GList *hide = NULL;

        gpp_update_match_filter_text (priv->fd.cleared_match, 0x01, _ ("Unreconciled"),
                                      &show, &hide);
        gpp_update_match_filter_text (priv->fd.cleared_match, 0x02, _ ("Cleared"),
                                      &show, &hide);
        gpp_update_match_filter_text (priv->fd.cleared_match, 0x04, _ ("Reconciled"),
                                      &show, &hide);
        gpp_update_match_filter_text (priv->fd.cleared_match, 0x08, _ ("Frozen"),
                                      &show, &hide);
        gpp_update_match_filter_text (priv->fd.cleared_match, 0x10, _ ("Voided"),
                                      &show, &hide);

        show = g_list_reverse (show);
        hide = g_list_reverse (hide);

        if (show)
        {
            char *str = gnc_list_formatter (show);
            t_list = g_list_prepend
                (t_list, g_strdup_printf ("%s %s", _("Show:"), str));
            g_free (str);
        }

        if (hide)
        {
            char *str = gnc_list_formatter (hide);
            t_list = g_list_prepend
                (t_list, g_strdup_printf ("%s %s", _("Hide:"), str));
            g_free (str);
        }

        g_list_free_full (show, g_free);
        g_list_free_full (hide, g_free);
    }

    t_list = g_list_reverse (t_list);

    if (t_list)
        t_list = g_list_prepend (t_list, g_strdup (_("Filter By:")));

    // free the existing text if present
    if (priv->gsr->filter_text != NULL)
        g_free (priv->gsr->filter_text);

    // set the tooltip text variable in the gsr
    priv->gsr->filter_text = gnc_g_list_stringjoin (t_list, "\n");

    g_list_free_full (t_list, g_free);

    LEAVE (" ");
}


static void
gnc_plugin_page_register_update_page_icon (GncPluginPage* plugin_page)
{
    GncPluginPageRegisterPrivate* priv;
    gboolean read_only;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (plugin_page);

    if (qof_book_is_readonly (gnc_get_current_book()) ||
        gnc_split_reg_get_read_only (priv->gsr))
        read_only = TRUE;
    else
        read_only = FALSE;

    main_window_update_page_set_read_only_icon (GNC_PLUGIN_PAGE(plugin_page),
                                                read_only);
}

/************************************************************/
/*                  Report Helper Functions                 */
/************************************************************/

static char*
gnc_reg_get_name (GNCLedgerDisplay* ledger, gboolean for_window)
{
    Account* leader;
    SplitRegister* reg;
    gchar* account_name;
    gchar* reg_name;
    gchar* name;
    GNCLedgerDisplayType ledger_type;

    if (ledger == NULL)
        return NULL;

    reg = gnc_ledger_display_get_split_register (ledger);
    ledger_type = gnc_ledger_display_type (ledger);

    switch (reg->type)
    {
    case GENERAL_JOURNAL:
    case INCOME_LEDGER:
        if (for_window)
            reg_name = _ ("General Journal");
        else
            reg_name = _ ("Transaction Report");
        break;
    case PORTFOLIO_LEDGER:
        if (for_window)
            reg_name = _ ("Portfolio");
        else
            reg_name = _ ("Portfolio Report");
        break;
    case SEARCH_LEDGER:
        if (for_window)
            reg_name = _ ("Search Results");
        else
            reg_name = _ ("Search Results Report");
        break;
    default:
        if (for_window)
            reg_name = _ ("Register");
        else
            reg_name = _ ("Transaction Report");
        break;
    }

    leader = gnc_ledger_display_leader (ledger);

    if ((leader != NULL) && (ledger_type != LD_GL))
    {
        account_name = gnc_account_get_full_name (leader);

        if (ledger_type == LD_SINGLE)
        {
            name = g_strconcat (account_name, " - ", reg_name, NULL);
        }
        else
        {
            name = g_strconcat (account_name, " ", _ ("and subaccounts"), " - ", reg_name,
                                NULL);
        }
        g_free (account_name);
    }
    else
        name = g_strdup (reg_name);

    return name;
}

static int
report_helper (GNCLedgerDisplay* ledger, Split* split, Query* query)
{
    SplitRegister* reg = gnc_ledger_display_get_split_register (ledger);
    Account* account;
    char* str;
    const char* tmp;
    swig_type_info* qtype;
    SCM args;
    SCM func;
    SCM arg;

    args = SCM_EOL;

    func = scm_c_eval_string ("gnc:register-report-create");
    g_return_val_if_fail (scm_is_procedure (func), -1);

    tmp = gnc_split_register_get_credit_string (reg);
    arg = scm_from_utf8_string (tmp ? tmp : _ ("Credit"));
    args = scm_cons (arg, args);

    tmp = gnc_split_register_get_debit_string (reg);
    arg = scm_from_utf8_string (tmp ? tmp : _ ("Debit"));
    args = scm_cons (arg, args);

    str = gnc_reg_get_name (ledger, FALSE);
    arg = scm_from_utf8_string (str ? str : "");
    args = scm_cons (arg, args);
    g_free (str);

    arg = SCM_BOOL (reg->use_double_line);
    args = scm_cons (arg, args);

    arg = SCM_BOOL (reg->type == GENERAL_JOURNAL || reg->type == INCOME_LEDGER
                    || reg->type == SEARCH_LEDGER);
    args = scm_cons (arg, args);

    arg = SCM_BOOL (reg->style == REG_STYLE_JOURNAL);
    args = scm_cons (arg, args);

    if (!query)
    {
        query = gnc_ledger_display_get_query (ledger);
        g_return_val_if_fail (query != NULL, -1);
    }

    qtype = SWIG_TypeQuery ("_p__QofQuery");
    g_return_val_if_fail (qtype, -1);

    arg = SWIG_NewPointerObj (query, qtype, 0);
    args = scm_cons (arg, args);
    g_return_val_if_fail (arg != SCM_UNDEFINED, -1);


    if (split)
    {
        qtype = SWIG_TypeQuery ("_p_Split");
        g_return_val_if_fail (qtype, -1);
        arg = SWIG_NewPointerObj (split, qtype, 0);
    }
    else
    {
        arg = SCM_BOOL_F;
    }
    args = scm_cons (arg, args);
    g_return_val_if_fail (arg != SCM_UNDEFINED, -1);


    qtype = SWIG_TypeQuery ("_p_Account");
    g_return_val_if_fail (qtype, -1);

    account = gnc_ledger_display_leader (ledger);
    arg = SWIG_NewPointerObj (account, qtype, 0);
    args = scm_cons (arg, args);
    g_return_val_if_fail (arg != SCM_UNDEFINED, -1);


    /* Apply the function to the args */
    arg = scm_apply (func, args, SCM_EOL);
    g_return_val_if_fail (scm_is_exact (arg), -1);

    return scm_to_int (arg);
}

/************************************************************/
/*                     Command callbacks                    */
/************************************************************/

static void
gnc_plugin_page_register_cmd_print_check (GSimpleAction *simple,
                                          GVariant      *paramter,
                                          gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    Split*          split;
    Transaction*    trans;
    GList*          splits = NULL, *item;
    GNCLedgerDisplayType ledger_type;
    Account*        account, *subaccount = NULL;
    GtkWidget*      window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    ledger_type = gnc_ledger_display_type (priv->ledger);
    window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));
    if (ledger_type == LD_SINGLE || ledger_type == LD_SUBACCOUNT)
    {
        account  = gnc_plugin_page_register_get_account (page);
        split    = gnc_split_register_get_current_split (reg);
        trans    = xaccSplitGetParent (split);
        if (ledger_type == LD_SUBACCOUNT)
        {
            /* Set up subaccount printing, where the check amount matches the
             * value displayed in the register. */
            subaccount = account;
        }

        if (split && trans)
        {
            if (xaccSplitGetAccount (split) == account)
            {
                splits = g_list_prepend (splits, split);
                gnc_ui_print_check_dialog_create (window, splits, subaccount);
                g_list_free (splits);
            }
            else
            {
                /* This split is not for the account shown in this register.  Get the
                   split that anchors the transaction to the registor */
                split = gnc_split_register_get_current_trans_split (reg, NULL);
                if (split)
                {
                    splits = g_list_prepend (splits, split);
                    gnc_ui_print_check_dialog_create (window, splits, subaccount);
                    g_list_free (splits);
                }
            }
        }
    }
    else if (ledger_type == LD_GL && reg->type == SEARCH_LEDGER)
    {
        Account* common_acct = NULL;

        /* the following GList* splits must not be freed */
        splits = qof_query_run (gnc_ledger_display_get_query (priv->ledger));

        /* Make sure each split is from the same account */
        for (item = splits; item; item = g_list_next (item))
        {
            split = (Split*) item->data;
            if (common_acct == NULL)
            {
                common_acct = xaccSplitGetAccount (split);
            }
            else
            {
                if (xaccSplitGetAccount (split) != common_acct)
                {
                    GtkWidget* dialog;
                    gint response;
                    const gchar* title = _ ("Print checks from multiple accounts?");
                    const gchar* message =
                        _ ("This search result contains splits from more than one account. "
                           "Do you want to print the checks even though they are not all "
                           "from the same account?");
                    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_MESSAGE_WARNING,
                                                     GTK_BUTTONS_CANCEL,
                                                     "%s", title);
                    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                              "%s", message);
                    gtk_dialog_add_button (GTK_DIALOG (dialog), _ ("_Print checks"),
                                           GTK_RESPONSE_YES);
                    response = gnc_dialog_run (GTK_DIALOG (dialog),
                                               GNC_PREF_WARN_CHECKPRINTING_MULTI_ACCT);
                    gtk_widget_destroy (dialog);
                    if (response != GTK_RESPONSE_YES)
                    {
                        LEAVE ("Multiple accounts");
                        return;
                    }
                    break;
                }
            }
        }
        gnc_ui_print_check_dialog_create (window, splits, NULL);
    }
    else
    {
        gnc_error_dialog (GTK_WINDOW (window), "%s",
                          _ ("You can only print checks from a bank account register or search results."));
        LEAVE ("Unsupported ledger type");
        return;
    }
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_cut (GSimpleAction *simple,
                                  GVariant      *paramter,
                                  gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnucash_register_cut_clipboard (priv->gsr->reg);
    LEAVE ("");
}


static void
gnc_plugin_page_register_cmd_copy (GSimpleAction *simple,
                                   GVariant      *paramter,
                                   gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnucash_register_copy_clipboard (priv->gsr->reg);
    LEAVE ("");
}


static void
gnc_plugin_page_register_cmd_paste (GSimpleAction *simple,
                                    GVariant      *paramter,
                                    gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnucash_register_paste_clipboard (priv->gsr->reg);
    LEAVE ("");
}


static void
gnc_plugin_page_register_cmd_edit_account (GSimpleAction *simple,
                                           GVariant      *paramter,
                                           gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account* account;
    GtkWindow* parent = GTK_WINDOW(gnc_plugin_page_get_window (GNC_PLUGIN_PAGE(page)));
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    account = gnc_plugin_page_register_get_account (page);
    if (account)
        gnc_ui_edit_account_window (parent, account);
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_find_account (GSimpleAction *simple,
                                           GVariant      *paramter,
                                           gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GtkWidget* window;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));
    gnc_find_account_dialog (window, NULL);
}


static void
gnc_plugin_page_register_cmd_find_transactions (GSimpleAction *simple,
                                                GVariant      *paramter,
                                                gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GtkWindow* window;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    window = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page)));
    gnc_ui_find_transactions_dialog_create (window, priv->ledger);
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_edit_tax_options (GSimpleAction *simple,
                                               GVariant      *paramter,
                                               gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GtkWidget *window;
    Account* account;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));
    account = gnc_plugin_page_register_get_account (page);
    gnc_tax_info_dialog (window, account);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_cut_transaction (GSimpleAction *simple,
                                              GVariant      *paramter,
                                              gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_cut_txn_handler (priv->gsr, NULL);
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_copy_transaction (GSimpleAction *simple,
                                               GVariant      *paramter,
                                               gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    gnc_split_register_copy_current (reg);
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_paste_transaction (GSimpleAction *simple,
                                                GVariant      *paramter,
                                                gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    gnc_split_register_paste_current (reg);
    LEAVE (" ");
}


static void
gnc_plugin_page_register_cmd_void_transaction (GSimpleAction *simple,
                                               GVariant      *paramter,
                                               gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GtkWidget* dialog, *entry;
    SplitRegister* reg;
    Transaction* trans;
    GtkBuilder* builder;
    const char* reason;
    gint result;
    GtkWindow* window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page)));
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    trans = gnc_split_register_get_current_trans (reg);
    if (trans == NULL)
        return;
    if (xaccTransHasSplitsInState (trans, VREC))
        return;
    if (xaccTransHasReconciledSplits (trans) ||
        xaccTransHasSplitsInState (trans, CREC))
    {
        gnc_error_dialog (window, "%s",
                          _ ("You cannot void a transaction with reconciled or cleared splits."));
        return;
    }
    reason = xaccTransGetReadOnly (trans);
    if (reason)
    {
        gnc_error_dialog (window,
                          _ ("This transaction is marked read-only with the comment: '%s'"), reason);
        return;
    }

    if (!gnc_plugin_page_register_finish_pending (GNC_PLUGIN_PAGE (page)))
        return;

    builder = gtk_builder_new();
    gnc_builder_add_from_file (builder, "gnc-plugin-page-register.glade",
                               "void_transaction_dialog");
    dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "void_transaction_dialog"));
    entry = GTK_WIDGET (gtk_builder_get_object (builder, "reason"));

    gtk_window_set_transient_for (GTK_WINDOW (dialog), window);

    result = gtk_dialog_run (GTK_DIALOG (dialog));
    if (result == GTK_RESPONSE_OK)
    {
        reason = gtk_entry_get_text (GTK_ENTRY (entry));
        if (reason == NULL)
            reason = "";
        gnc_split_register_void_current_trans (reg, reason);
    }

    /* All done. Get rid of it. */
    gtk_widget_destroy (dialog);
    g_object_unref (G_OBJECT (builder));
}


static void
gnc_plugin_page_register_cmd_unvoid_transaction (GSimpleAction *simple,
                                                 GVariant      *paramter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    Transaction* trans;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    trans = gnc_split_register_get_current_trans (reg);
    if (!xaccTransHasSplitsInState (trans, VREC))
        return;
    gnc_split_register_unvoid_current_trans (reg);
    LEAVE (" ");
}

static std::optional<time64>
input_date (GtkWidget * parent, const char *window_title, const char* title)
{
    time64 rv = gnc_time (nullptr);
    if (!gnc_dup_time64_dialog (parent, window_title, title, &rv))
        return {};

    return rv;
}

static void
gnc_plugin_page_register_cmd_reverse_transaction (GSimpleAction *simple,
                                                  GVariant      *paramter,
                                                  gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    GNCSplitReg* gsr;
    Transaction* trans, *new_trans;
    GtkWidget *window;
    Account *account;
    Split *split;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));
    trans = gnc_split_register_get_current_trans (reg);
    if (trans == NULL)
        return;

    split = gnc_split_register_get_current_split (reg);
    account = xaccSplitGetAccount (split);

    if (!account)
    {
        LEAVE ("shouldn't try to reverse the blank transaction...");
        return;
    }

    new_trans = xaccTransGetReversedBy (trans);
    if (new_trans)
    {
        const char *rev = _("A reversing entry has already been created for this transaction.");
        const char *jump = _("Jump to the transaction?");
        if (!gnc_verify_dialog (GTK_WINDOW (window), TRUE, "%s\n\n%s", rev, jump))
        {
            LEAVE ("reverse cancelled");
            return;
        }
    }
    else
    {
        auto date = input_date (window, _("Reverse Transaction"), _("New Transaction Information"));
        if (!date)
        {
            LEAVE ("reverse cancelled");
            return;
        }

        gnc_suspend_gui_refresh ();
        new_trans = xaccTransReverse (trans);

        /* Clear transaction level info */
        xaccTransSetDatePostedSecsNormalized (new_trans, date.value());
        xaccTransSetDateEnteredSecs (new_trans, gnc_time (NULL));

        gnc_resume_gui_refresh();
    }

    /* Now jump to new trans */
    gsr = gnc_plugin_page_register_get_gsr (GNC_PLUGIN_PAGE (page));
    split = xaccTransFindSplitByAccount(new_trans, account);

    /* Test for visibility of split */
    if (gnc_split_reg_clear_filter_for_split (gsr, split))
        gnc_plugin_page_register_clear_current_filter (GNC_PLUGIN_PAGE(page));

    gnc_split_reg_jump_to_split (gsr, split);
    LEAVE (" ");
}

static gboolean
gnc_plugin_page_register_show_fs_save (GncPluginPageRegister* page)
{
    GncPluginPageRegisterPrivate* priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    GNCLedgerDisplayType ledger_type = gnc_ledger_display_type (priv->ledger);
    SplitRegister* reg = gnc_ledger_display_get_split_register (priv->ledger);

    if (ledger_type == LD_SINGLE || ledger_type == LD_SUBACCOUNT)
        return TRUE;
    else
    {
        switch (reg->type)
        {
        case GENERAL_JOURNAL:
            return TRUE;
            break;

        case INCOME_LEDGER:
        case PORTFOLIO_LEDGER:
        case SEARCH_LEDGER:
        default:
            return FALSE;
            break;
        }
    }
}

static void
gnc_plugin_page_register_cmd_view_sort_by (GSimpleAction *simple,
                                           GVariant      *paramter,
                                           gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    GtkWidget* dialog, *button;
    GtkBuilder* builder;
    SortType sort;
    const gchar* name;
    gchar* title;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));
    ENTER ("(action %p, page %p)", simple, page);

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (priv->sd.dialog)
    {
        gtk_window_present (GTK_WINDOW (priv->sd.dialog));
        LEAVE ("existing dialog");
        return;
    }

    /* Create the dialog */

    builder = gtk_builder_new();
    gnc_builder_add_from_file (builder, "gnc-plugin-page-register.glade",
                               "sort_by_dialog");
    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "sort_by_dialog"));
    priv->sd.dialog = dialog;
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window)));
    /* Translators: The %s is the name of the plugin page */
    title = g_strdup_printf (_ ("Sort %s by…"),
                             gnc_plugin_page_get_page_name (GNC_PLUGIN_PAGE (page)));
    gtk_window_set_title (GTK_WINDOW (dialog), title);
    g_free (title);

    /* Set the button for the current sort order */
    sort = gnc_split_reg_get_sort_type (priv->gsr);
    name = SortTypeasString (sort);
    button = GTK_WIDGET (gtk_builder_get_object (builder, name));
    DEBUG ("current sort %d, button %s(%p)", sort, name, button);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    priv->sd.original_sort_type = sort;

    button = GTK_WIDGET (gtk_builder_get_object (builder, "sort_save"));
    if (priv->sd.save_order == TRUE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    // hide the save button if appropriate
    gtk_widget_set_visible (GTK_WIDGET (button),
                            gnc_plugin_page_register_show_fs_save (page));

    /* Set the button for the current reverse_order order */
    button = GTK_WIDGET (gtk_builder_get_object (builder, "sort_reverse"));
    if (priv->sd.reverse_order == TRUE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    priv->sd.original_reverse_order = priv->sd.reverse_order;

    priv->sd.num_radio = GTK_WIDGET (gtk_builder_get_object (builder, "BY_NUM"));
    priv->sd.act_radio = GTK_WIDGET (gtk_builder_get_object (builder,
                                                             "BY_ACTION"));
    /* Adjust labels related to Num/Action radio buttons based on book option */
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    if (reg && !reg->use_tran_num_for_num_field)
    {
        gtk_button_set_label (GTK_BUTTON (priv->sd.num_radio),
                              _ ("Transaction Number"));
        gtk_button_set_label (GTK_BUTTON (priv->sd.act_radio), _ ("Number/Action"));
    }
    gnc_book_option_register_cb (OPTION_NAME_NUM_FIELD_SOURCE,
                                 (GncBOCb)gnc_plugin_page_register_sort_book_option_changed,
                                 page);

    /* Wire it up */
    gtk_builder_connect_signals_full (builder, gnc_builder_connect_full_func,
                                      page);

    /* Show it */
    gtk_widget_show (dialog);
    g_object_unref (G_OBJECT (builder));
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_view_filter_by (GSimpleAction *simple,
                                             GVariant      *paramter,
                                             gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GtkWidget* dialog, *toggle, *button, *table, *hbox;
    time64 start_time, end_time, time_val;
    GtkBuilder* builder;
    gboolean sensitive, value;
    Query* query;
    gchar* title;
    int i;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));
    ENTER ("(action %p, page %p)", simple, page);

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    if (priv->fd.dialog)
    {
        gtk_window_present (GTK_WINDOW (priv->fd.dialog));
        LEAVE ("existing dialog");
        return;
    }

    /* Create the dialog */
    builder = gtk_builder_new();
    gnc_builder_add_from_file (builder, "gnc-plugin-page-register.glade",
                               "days_adjustment");
    gnc_builder_add_from_file (builder, "gnc-plugin-page-register.glade",
                               "filter_by_dialog");
    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "filter_by_dialog"));
    priv->fd.dialog = dialog;
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window)));

    /* Translators: The %s is the name of the plugin page */
    title = g_strdup_printf (_ ("Filter %s by…"),
                             gnc_plugin_page_get_page_name (GNC_PLUGIN_PAGE (page)));
    gtk_window_set_title (GTK_WINDOW (dialog), title);
    g_free (title);

    /* Set the check buttons for the current status */
    for (i = 0; status_actions[i].action_name; i++)
    {
        toggle = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     status_actions[i].action_name));
        value = priv->fd.cleared_match & status_actions[i].value;
        status_actions[i].widget = toggle;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), value);
    }
    priv->fd.original_cleared_match = priv->fd.cleared_match;

    button = GTK_WIDGET (gtk_builder_get_object (builder, "filter_save"));
    if (priv->fd.save_filter == TRUE)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    // hide the save button if appropriate
    gtk_widget_set_visible (GTK_WIDGET (button),
                            gnc_plugin_page_register_show_fs_save (page));

    /* Set up number of days */
    priv->fd.num_days = GTK_WIDGET (gtk_builder_get_object (builder,
                                                            "filter_show_num_days"));
    button = GTK_WIDGET (gtk_builder_get_object (builder, "filter_show_days"));

    query = gnc_ledger_display_get_query (priv->ledger);

    if (priv->fd.days > 0) // using number of days
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (priv->fd.num_days), TRUE);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->fd.num_days), priv->fd.days);
        priv->fd.original_days = priv->fd.days;

        /* Set the start_time and end_time to 0 */
        start_time = 0;
        end_time = 0;
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (priv->fd.num_days), FALSE);
        priv->fd.original_days = 0;
        priv->fd.days = 0;

        /* Get the start and end times */
        xaccQueryGetDateMatchTT (query, &start_time, &end_time);
    }

    /* Set the date info */
    priv->fd.original_start_time = start_time;
    priv->fd.start_time = start_time;
    priv->fd.original_end_time = end_time;
    priv->fd.end_time = end_time;

    button = GTK_WIDGET (gtk_builder_get_object (builder, "filter_show_range"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), start_time ||
                                  end_time);
    table = GTK_WIDGET (gtk_builder_get_object (builder, "select_range_table"));
    priv->fd.table = table;
    gtk_widget_set_sensitive (GTK_WIDGET (table), start_time || end_time);

    priv->fd.start_date_choose = GTK_WIDGET (gtk_builder_get_object (builder,
                                             "start_date_choose"));
    priv->fd.start_date_today = GTK_WIDGET (gtk_builder_get_object (builder,
                                            "start_date_today"));
    priv->fd.end_date_choose = GTK_WIDGET (gtk_builder_get_object (builder,
                                           "end_date_choose"));
    priv->fd.end_date_today = GTK_WIDGET (gtk_builder_get_object (builder,
                                          "end_date_today"));

    {
        /* Start date info */
        if (start_time == 0)
        {
            button = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_earliest"));
            time_val = xaccQueryGetEarliestDateFound (query);
            sensitive = FALSE;
        }
        else
        {
            time_val = start_time;
            if ((start_time >= gnc_time64_get_today_start()) &&
                (start_time <= gnc_time64_get_today_end()))
            {
                button = priv->fd.start_date_today;
                sensitive = FALSE;
            }
            else
            {
                button = priv->fd.start_date_choose;
                sensitive = TRUE;
            }
        }
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
        priv->fd.start_date = gnc_date_edit_new (gnc_time (NULL), FALSE, FALSE);
        hbox = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_hbox"));
        gtk_box_pack_start (GTK_BOX (hbox), priv->fd.start_date, TRUE, TRUE, 0);
        gtk_widget_show (priv->fd.start_date);
        gtk_widget_set_sensitive (GTK_WIDGET (priv->fd.start_date), sensitive);
        gnc_date_edit_set_time (GNC_DATE_EDIT (priv->fd.start_date), time_val);
        g_signal_connect (G_OBJECT (priv->fd.start_date), "date-changed",
                          G_CALLBACK (gnc_plugin_page_register_filter_gde_changed_cb),
                          page);
    }

    {
        /* End date info */
        if (end_time == 0)
        {
            button = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_latest"));
            time_val = xaccQueryGetLatestDateFound (query);
            sensitive = FALSE;
        }
        else
        {
            time_val = end_time;
            if ((end_time >= gnc_time64_get_today_start()) &&
                (end_time <= gnc_time64_get_today_end()))
            {
                button = priv->fd.end_date_today;
                sensitive = FALSE;
            }
            else
            {
                button = priv->fd.end_date_choose;
                sensitive = TRUE;
            }
        }
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
        priv->fd.end_date = gnc_date_edit_new (gnc_time (NULL), FALSE, FALSE);
        hbox = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_hbox"));
        gtk_box_pack_start (GTK_BOX (hbox), priv->fd.end_date, TRUE, TRUE, 0);
        gtk_widget_show (priv->fd.end_date);
        gtk_widget_set_sensitive (GTK_WIDGET (priv->fd.end_date), sensitive);
        gnc_date_edit_set_time (GNC_DATE_EDIT (priv->fd.end_date), time_val);
        g_signal_connect (G_OBJECT (priv->fd.end_date), "date-changed",
                          G_CALLBACK (gnc_plugin_page_register_filter_gde_changed_cb),
                          page);
    }

    /* Wire it up */
    gtk_builder_connect_signals_full (builder, gnc_builder_connect_full_func,
                                      page);

    /* Show it */
    gtk_widget_show (dialog);
    g_object_unref (G_OBJECT (builder));
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_reload (GSimpleAction *simple,
                                     GVariant      *paramter,
                                     gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    /* Check for trans being edited */
    if (gnc_split_register_changed (reg))
    {
        LEAVE ("register has pending edits");
        return;
    }
    gnc_ledger_display_refresh (priv->ledger);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_style_changed (GSimpleAction *simple,
                                            GVariant      *parameter,
                                            gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegisterStyle value;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    value = (SplitRegisterStyle)g_variant_get_int32 (parameter);

    g_action_change_state (G_ACTION(simple), parameter);

    gnc_split_reg_change_style (priv->gsr, value, priv->enable_refresh);

    gnc_plugin_page_register_ui_update (NULL, page);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_style_double_line (GSimpleAction *simple,
                                                GVariant      *parameter,
                                                gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    gboolean use_double_line;
    GVariant *state;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER(page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    state = g_action_get_state (G_ACTION(simple));

    g_action_change_state (G_ACTION(simple), g_variant_new_boolean (!g_variant_get_boolean (state)));

    use_double_line = !g_variant_get_boolean (state);

    if (use_double_line != reg->use_double_line)
    {
        gnc_split_register_config (reg, reg->type, reg->style, use_double_line);
        if (priv->enable_refresh)
            gnc_ledger_display_refresh (priv->ledger);
    }
    g_variant_unref (state);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_transfer (GSimpleAction *simple,
                                       GVariant      *paramter,
                                       gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account* account;
    GncWindow* gnc_window;
    GtkWidget* window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    account = gnc_plugin_page_register_get_account (page);
    gnc_window = GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window);
    window = GTK_WIDGET (gnc_window_get_gtk_window (gnc_window));
    gnc_xfer_dialog (window, account);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_reconcile (GSimpleAction *simple,
                                        GVariant      *paramter,
                                        gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account* account;
    GtkWindow* window;
    RecnWindow* recnData;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    /* To prevent mistakes involving saving an edited transaction after
     * finishing a reconciliation (reverting the reconcile state), require
     * pending activity on the current register to be finished.
     *
     * The reconcile window isn't modal so it's still possible to start editing
     * a transaction after opening it, but at that point the user should know
     * what they're doing is unsafe.
     */
    if (!gnc_plugin_page_register_finish_pending (GNC_PLUGIN_PAGE (page)))
        return;

    account = gnc_plugin_page_register_get_account (page);

    window = gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (
                                                        page)->window));
    recnData = recnWindow (GTK_WIDGET (window), account);
    gnc_ui_reconcile_window_raise (recnData);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_stock_assistant (GSimpleAction *simple,
                                              GVariant      *paramter,
                                              gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account *account;
    GtkWindow *window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));
    window = gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window));
    account = gnc_plugin_page_register_get_account (page);
    gnc_stock_transaction_assistant (GTK_WIDGET (window), account);

    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_autoclear (GSimpleAction *simple,
                                        GVariant      *paramter,
                                        gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account* account;
    GtkWindow* window;
    AutoClearWindow* autoClearData;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    account = gnc_plugin_page_register_get_account (page);

    window = gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (
                                                        page)->window));
    autoClearData = autoClearWindow (GTK_WIDGET (window), account);
    gnc_ui_autoclear_window_raise (autoClearData);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_stock_split (GSimpleAction *simple,
                                          GVariant      *paramter,
                                          gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    Account* account;
    GtkWindow* window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    account = gnc_plugin_page_register_get_account (page);
    window = gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window));
    gnc_stock_split_dialog (GTK_WIDGET (window), account);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_lots (GSimpleAction *simple,
                                   GVariant      *paramter,
                                   gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GtkWindow* window;
    Account* account;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = gnc_window_get_gtk_window (GNC_WINDOW (GNC_PLUGIN_PAGE (
                                                        page)->window));
    account = gnc_plugin_page_register_get_account (page);
    gnc_lot_viewer_dialog (window, account);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_enter_transaction (GSimpleAction *simple,
                                                GVariant      *paramter,
                                                gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnc_split_reg_enter (priv->gsr, FALSE);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_cancel_transaction (GSimpleAction *simple,
                                                 GVariant      *paramter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnc_split_register_cancel_cursor_trans_changes
    (gnc_ledger_display_get_split_register (priv->ledger));
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_delete_transaction (GSimpleAction *simple,
                                                 GVariant      *paramter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_delete_handler (priv->gsr, NULL);
    LEAVE (" ");

}

static void
gnc_plugin_page_register_cmd_linked_transaction (GSimpleAction *simple,
                                                 GVariant      *paramter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_doclink_handler (priv->gsr);
    gnc_plugin_page_register_ui_update (NULL, page);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_linked_transaction_open (GSimpleAction *simple,
                                                      GVariant      *paramter,
                                                      gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_doclink_open_handler (priv->gsr);
    LEAVE (" ");
}

static GncInvoice* invoice_from_split (Split* split)
{
    GncInvoice* invoice;
    GNCLot* lot;

    if (!split)
        return NULL;

    lot = xaccSplitGetLot (split);
    if (!lot)
        return NULL;

    invoice = gncInvoiceGetInvoiceFromLot (lot);
    if (!invoice)
        return NULL;

    return invoice;
}


static void
gnc_plugin_page_register_cmd_jump_linked_invoice (GSimpleAction *simple,
                                                  GVariant      *paramter,
                                                  gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    GncInvoice* invoice;
    Transaction *txn;
    GtkWidget *window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->gsr->ledger);
    txn = gnc_split_register_get_current_trans (reg);
    invoice = invoice_from_split (gnc_split_register_get_current_split (reg));
    window = GNC_PLUGIN_PAGE(page)->window;

    if (!invoice)
    {
        auto invoices = invoices_from_transaction (txn);
        if (invoices.empty())
            PERR ("shouldn't happen: if no invoices, function is never called");
        else if (invoices.size() == 1)
            invoice = invoices[0];
        else
        {
            GList *details = NULL;
            gint choice;
            const gchar *amt;
            for (const auto& inv : invoices)
            {
                gchar *date = qof_print_date (gncInvoiceGetDatePosted (inv));
                amt = xaccPrintAmount
                    (gncInvoiceGetTotal (inv),
                     gnc_account_print_info (gncInvoiceGetPostedAcc (inv), TRUE));
                details = g_list_prepend
                    (details,
                     /* Translators: %s refer to the following in
                        order: invoice type, invoice ID, owner name,
                        posted date, amount */
                     g_strdup_printf (_("%s %s from %s, posted %s, amount %s"),
                                      gncInvoiceGetTypeString (inv),
                                      gncInvoiceGetID (inv),
                                      gncOwnerGetName (gncInvoiceGetOwner (inv)),
                                      date, amt));
                g_free (date);
            }
            details = g_list_reverse (details);
            choice = gnc_choose_radio_option_dialog
                (window, _("Select document"),
                 _("Several documents are linked with this transaction. \
Please choose one:"), _("Select"), 0, details);
            if ((choice >= 0) && ((size_t)choice < invoices.size()))
                invoice = invoices[choice];
            g_list_free_full (details, g_free);
        }
    }

    if (invoice)
    {
        GtkWindow *gtk_window = gnc_window_get_gtk_window (GNC_WINDOW (window));
        gnc_ui_invoice_edit (gtk_window, invoice);
    }

    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_blank_transaction (GSimpleAction *simple,
                                                GVariant      *paramter,
                                                gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    if (gnc_split_register_save (reg, TRUE))
        gnc_split_register_redraw (reg);

    gnc_split_reg_jump_to_blank (priv->gsr);
    LEAVE (" ");
}

static bool
find_after_date (Split *split, time64* find_date)
{
    auto trans = xaccSplitGetParent (split);
    return !(xaccSplitGetAccount (split) != nullptr && 
             xaccTransGetDate (trans) >= *find_date &&
             xaccTransCountSplits (trans) != 1);
}

static void
gnc_plugin_page_register_cmd_goto_date (GSimpleAction *simple,
                                        GVariant      *paramter,
                                        gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GNCSplitReg* gsr;
    Query* query;
    GList *splits;
    GtkWidget *window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));

    ENTER ("(action %p, page %p)", simple, page);
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    auto date = input_date (window, _("Go to Date"), _("Go to Date"));

    if (!date)
    {
        LEAVE ("goto_date cancelled");
        return;
    }

    gsr = gnc_plugin_page_register_get_gsr (GNC_PLUGIN_PAGE (page));
    query = gnc_plugin_page_register_get_query (GNC_PLUGIN_PAGE (page));
    splits = g_list_copy (qof_query_run (query));
    splits = g_list_sort (splits, (GCompareFunc)xaccSplitOrder);

    // if gl register, there could be blank splits from other open registers
    // included in split list so check for and ignore them
    auto it = g_list_find_custom (splits, &date.value(), (GCompareFunc)find_after_date);

    if (it)
        gnc_split_reg_jump_to_split (gsr, GNC_SPLIT(it->data));
    else
        gnc_split_reg_jump_to_blank (gsr);

    g_list_free (splits);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_duplicate_transaction (GSimpleAction *simple,
                                                    GVariant      *paramter,
                                                    gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gnc_split_register_duplicate_current
    (gnc_ledger_display_get_split_register (priv->ledger));
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_reinitialize_transaction (GSimpleAction *simple,
                                                       GVariant      *paramter,
                                                       gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_reinit_handler (priv->gsr, NULL);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_expand_transaction (GSimpleAction *simple,
                                                 GVariant      *parameter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    gboolean expand;
    GVariant *state;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    state = g_action_get_state (G_ACTION(simple));

    g_action_change_state (G_ACTION(simple), g_variant_new_boolean (!g_variant_get_boolean (state)));

    expand = !g_variant_get_boolean (state);

    gnc_split_register_expand_current_trans (reg, expand);
    g_variant_unref (state);
    LEAVE (" ");
}

/** Callback for "Edit Exchange Rate" menu item.
 */
static void
gnc_plugin_page_register_cmd_exchange_rate (GSimpleAction *simple,
                                            GVariant      *paramter,
                                            gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    /* XXX Ignore the return value -- we don't care if this succeeds */
    (void)gnc_split_register_handle_exchange (reg, TRUE);
    LEAVE (" ");
}

static Split*
jump_multiple_splits_by_single_account (Account *account, Split *split)
{
    Transaction *trans;
    SplitList *splits;
    Account *other_account = NULL;
    Split *other_split = NULL;

    trans = xaccSplitGetParent(split);
    if (!trans)
        return NULL;

    for (splits = xaccTransGetSplitList(trans); splits; splits = splits->next)
    {
        Split *s = (Split*)splits->data;
        Account *a = xaccSplitGetAccount(s);

        if (!xaccTransStillHasSplit(trans, s))
            continue;

        if (a == account)
            continue;

        if (other_split)
        {
            if (other_account != a)
                return NULL;

            continue;
        }

        other_account = a;
        other_split = s;
    }

    // Jump to the same account so that the right warning is triggered
    if (!other_split)
        other_split = split;

    return other_split;
}

static Split*
jump_multiple_splits_by_value (Account *account, Split *split, gboolean largest)
{
    Transaction *trans;
    SplitList *splits;
    Split *other_split = NULL;
    gnc_numeric best;
    int cmp = largest ? 1 : -1;

    trans = xaccSplitGetParent(split);
    if (!trans)
        return NULL;

    for (splits = xaccTransGetSplitList(trans); splits; splits = splits->next)
    {
        Split *s = (Split*)splits->data;
        gnc_numeric value;

        if (!xaccTransStillHasSplit(trans, s))
            continue;

        if (xaccSplitGetAccount(s) == account)
            continue;

        value = gnc_numeric_abs(xaccSplitGetValue(s));
        if (gnc_numeric_check(value))
            continue;

        /* For splits with the same value as the best, the first split
         * encountered is used.
         */
        if (other_split && gnc_numeric_compare(value, best) != cmp)
            continue;

        best = value;
        other_split = s;
    }

    // Jump to the same account so that the right warning is triggered
    if (!other_split)
        other_split = split;

    return other_split;
}

static Split*
jump_multiple_splits (Account* account, Split *split)
{
    GncPrefJumpMultSplits mode = (GncPrefJumpMultSplits)gnc_prefs_get_enum(GNC_PREFS_GROUP_GENERAL_REGISTER, GNC_PREF_JUMP_MULT_SPLITS);

    switch (mode)
    {
    case JUMP_LARGEST_VALUE_FIRST_SPLIT:
        return jump_multiple_splits_by_value (account, split, TRUE);

    case JUMP_SMALLEST_VALUE_FIRST_SPLIT:
        return jump_multiple_splits_by_value (account, split, FALSE);

    case JUMP_DEFAULT:
    default:
        break;
    }

    // If there's only one other account, use that one
    return jump_multiple_splits_by_single_account (account, split);
}

static void
gnc_plugin_page_register_cmd_jump (GSimpleAction *simple,
                                   GVariant      *paramter,
                                   gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GncPluginPage* new_page;
    GtkWidget* window;
    GNCSplitReg* gsr;
    SplitRegister* reg;
    Account* account;
    Account* leader;
    Split* split;
    Split* other_split;
    gboolean multiple_splits;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    window = GNC_PLUGIN_PAGE (page)->window;
    if (window == NULL)
    {
        LEAVE ("no window");
        return;
    }

    reg = gnc_ledger_display_get_split_register (priv->ledger);
    split = gnc_split_register_get_current_split (reg);
    if (split == NULL)
    {
        LEAVE ("no split (1)");
        return;
    }

    account = xaccSplitGetAccount (split);
    if (account == NULL)
    {
        LEAVE ("no account");
        return;
    }

    other_split = xaccSplitGetOtherSplit (split);
    multiple_splits = other_split == NULL;

    leader = gnc_ledger_display_leader (priv->ledger);
    if (account == leader)
    {
        CursorClass cursor_class = gnc_split_register_get_current_cursor_class (reg);
        if (cursor_class == CURSOR_CLASS_SPLIT)
        {
            /* If you've selected the transaction itself, we jump to the "other"
             * account corresponding to the anchoring split.
             *
             * If you've selected the split for another account, we jump to that
             * split's account (account != leader, so this block is never
             * reached).
             *
             * If you've selected a split for this account, for consistency with
             * selecting the split of another account we should do nothing.
             * You're already on the account for the split you selected. Jumping
             * to the "other" account now would make the "multiple split"
             * options confusing.
             *
             * We could jump to a different anchoring split but that'll be very
             * subtle and only cause problems because it'll have to save any
             * modifications to the current register.
             */
            LEAVE ("split for this account");
            return;
        }

        if (multiple_splits)
        {
            other_split = jump_multiple_splits (account, split);
        }
        if (other_split == NULL)
        {
            GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                             (GtkDialogFlags)(GTK_DIALOG_MODAL
                                                | GTK_DIALOG_DESTROY_WITH_PARENT),
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "%s",
                                             _("Unable to jump to other account"));

            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                    "%s", _("This transaction involves more than one other account. Select a specific split to jump to that account."));
            gtk_dialog_add_button (GTK_DIALOG(dialog), _("_OK"), GTK_RESPONSE_OK);
            gnc_dialog_run (GTK_DIALOG(dialog), GNC_PREF_WARN_REG_TRANS_JUMP_MULTIPLE_SPLITS);
            gtk_widget_destroy (dialog);

            LEAVE ("no split (2)");
            return;
        }

        split = other_split;

        account = xaccSplitGetAccount (split);
        if (account == NULL)
        {
            LEAVE ("no account (2)");
            return;
        }

        if (account == leader)
        {
            GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                                             (GtkDialogFlags)(GTK_DIALOG_MODAL
                                                | GTK_DIALOG_DESTROY_WITH_PARENT),
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "%s",
                                             _("Unable to jump to other account"));

            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                    "%s", _("This transaction only involves the current account so there is no other account to jump to."));
            gtk_dialog_add_button (GTK_DIALOG(dialog), _("_OK"), GTK_RESPONSE_OK);
            gnc_dialog_run (GTK_DIALOG(dialog), GNC_PREF_WARN_REG_TRANS_JUMP_SINGLE_ACCOUNT);
            gtk_widget_destroy (dialog);

            LEAVE ("register open for account");
            return;
        }
    }

    new_page = gnc_plugin_page_register_new (account, FALSE);
    if (new_page == NULL)
    {
        LEAVE ("couldn't create new page");
        return;
    }

    gnc_main_window_open_page (GNC_MAIN_WINDOW (window), new_page);
    gsr = gnc_plugin_page_register_get_gsr (new_page);

    SplitRegister *new_page_reg = gnc_ledger_display_get_split_register (gsr->ledger);
    gboolean jump_twice = FALSE;

    /* Selecting the split (instead of just the transaction to open the "other"
     * account) requires jumping a second time after expanding the transaction,
     * in the basic and auto ledger modes.
     */
    if (new_page_reg->style != REG_STYLE_JOURNAL)
        jump_twice = TRUE;

    /* Test for visibility of split */
    if (gnc_split_reg_clear_filter_for_split (gsr, split))
        gnc_plugin_page_register_clear_current_filter (GNC_PLUGIN_PAGE(new_page));

    gnc_split_reg_jump_to_split (gsr, split);

    if (multiple_splits && jump_twice)
    {
        /* Expand the transaction for the basic and auto ledger to identify the
         * split in this register, but only if there are more than two splits.
         */
        gnc_split_register_expand_current_trans (new_page_reg, TRUE);
        gnc_split_reg_jump_to_split (gsr, split);
    }
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_schedule (GSimpleAction *simple,
                                       GVariant      *paramter,
                                       gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GtkWindow* window;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (
                                                         page)));
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    gsr_default_schedule_handler (priv->gsr, window);
    LEAVE (" ");
}

static void
scrub_split (Split *split)
{
    Account *acct;
    Transaction *trans;
    GNCLot *lot;

    g_return_if_fail (split);
    acct = xaccSplitGetAccount (split);
    trans = xaccSplitGetParent (split);
    lot = xaccSplitGetLot (split);
    g_return_if_fail (trans);

    xaccTransScrubOrphans (trans);
    xaccTransScrubImbalance (trans, gnc_get_current_root_account(), NULL);
    if (lot && xaccAccountIsAPARType (xaccAccountGetType (acct)))
    {
        gncScrubBusinessLot (lot);
        gncScrubBusinessSplit (split);
    }
}

static void
gnc_plugin_page_register_cmd_scrub_current (GSimpleAction *simple,
                                            GVariant      *paramter,
                                            gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    Query* query;
    SplitRegister* reg;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    query = gnc_ledger_display_get_query (priv->ledger);
    if (query == NULL)
    {
        LEAVE ("no query found");
        return;
    }

    reg = gnc_ledger_display_get_split_register (priv->ledger);

    gnc_suspend_gui_refresh();
    scrub_split (gnc_split_register_get_current_split (reg));
    gnc_resume_gui_refresh();
    LEAVE (" ");
}

static gboolean
scrub_kp_handler (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->length == 0) return FALSE;

    switch (event->keyval)
    {
    case GDK_KEY_Escape:
        {
            auto abort_scrub = gnc_verify_dialog (GTK_WINDOW(widget), false,
                                                  "%s", _(check_repair_abort_YN));

            if (abort_scrub)
                gnc_set_abort_scrub (TRUE);

            return TRUE;
        }
    default:
        break;
    }
    return FALSE;
}

static void
gnc_plugin_page_register_cmd_scrub_all (GSimpleAction *simple,
                                        GVariant      *paramter,
                                        gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    Query* query;
    GncWindow* window;
    GList* node, *splits;
    gint split_count = 0, curr_split_no = 0;
    gulong scrub_kp_handler_ID;
    const char* message = _ ("Checking splits in current register: %u of %u");

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    ENTER ("(action %p, page %p)", simple, page);

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    query = gnc_ledger_display_get_query (priv->ledger);
    if (!query)
    {
        LEAVE ("no query found");
        return;
    }

    gnc_suspend_gui_refresh();
    is_scrubbing = TRUE;
    gnc_set_abort_scrub (FALSE);
    window = GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window);
    scrub_kp_handler_ID = g_signal_connect (G_OBJECT (window), "key-press-event",
                                            G_CALLBACK (scrub_kp_handler), NULL);
    gnc_window_set_progressbar_window (window);

    splits = qof_query_run (query);
    split_count = g_list_length (splits);
    for (node = splits; node && !gnc_get_abort_scrub (); node = node->next, curr_split_no++)
    {
        auto split = GNC_SPLIT(node->data);

        if (!split) continue;

        PINFO ("Start processing split %d of %d",
               curr_split_no + 1, split_count);

        scrub_split (split);

        PINFO ("Finished processing split %d of %d",
               curr_split_no + 1, split_count);

        if (curr_split_no % 10 == 0)
        {
            char* progress_msg = g_strdup_printf (message, curr_split_no, split_count);
            gnc_window_show_progress (progress_msg, (100 * curr_split_no) / split_count);
            g_free (progress_msg);
        }
    }

    g_signal_handler_disconnect (G_OBJECT(window), scrub_kp_handler_ID);
    gnc_window_show_progress (NULL, -1.0);
    is_scrubbing = FALSE;
    show_abort_verify = TRUE;
    gnc_set_abort_scrub (FALSE);

    gnc_resume_gui_refresh();
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_account_report (GSimpleAction *simple,
                                             GVariant      *paramter,
                                             gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GncMainWindow* window;
    int id;

    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = GNC_MAIN_WINDOW (GNC_PLUGIN_PAGE (page)->window);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    id = report_helper (priv->ledger, NULL, NULL);
    if (id >= 0)
        gnc_main_window_open_report (id, window);
    LEAVE (" ");
}

static void
gnc_plugin_page_register_cmd_transaction_report (GSimpleAction *simple,
                                                 GVariant      *paramter,
                                                 gpointer       user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;
    GncMainWindow* window;
    SplitRegister* reg;
    Split* split;
    Query* query;
    int id;


    ENTER ("(action %p, page %p)", simple, page);

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);

    split = gnc_split_register_get_current_split (reg);
    if (!split)
        return;

    query = qof_query_create_for (GNC_ID_SPLIT);

    qof_query_set_book (query, gnc_get_current_book());

    xaccQueryAddGUIDMatch (query, xaccSplitGetGUID (split),
                           GNC_ID_SPLIT, QOF_QUERY_AND);

    window = GNC_MAIN_WINDOW (GNC_PLUGIN_PAGE (page)->window);
    id = report_helper (priv->ledger, split, query);
    if (id >= 0)
        gnc_main_window_open_report (id, window);
    LEAVE (" ");
}

/************************************************************/
/*                    Auxiliary functions                   */
/************************************************************/

void
gnc_plugin_page_register_set_options (GncPluginPage* plugin_page,
                                      gint lines_default,
                                      gboolean read_only)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page));

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
    priv->lines_default     = lines_default;
    priv->read_only         = read_only;
}

GNCSplitReg*
gnc_plugin_page_register_get_gsr (GncPluginPage* plugin_page)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;

    g_return_val_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (plugin_page), NULL);

    page = GNC_PLUGIN_PAGE_REGISTER (plugin_page);
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    return priv->gsr;
}

static void
gnc_plugin_page_help_changed_cb (GNCSplitReg* gsr,
                                 GncPluginPageRegister* register_page)
{
    GncPluginPageRegisterPrivate* priv;
    SplitRegister* reg;
    GncWindow* window;
    char* help;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (register_page));

    window = GNC_WINDOW (GNC_PLUGIN_PAGE (register_page)->window);
    if (!window)
    {
        // This routine can be called before the page is added to a
        // window.
        return;
    }

    // only update status text if on current page
    if (GNC_IS_MAIN_WINDOW(window) && (gnc_main_window_get_current_page
       (GNC_MAIN_WINDOW(window)) != GNC_PLUGIN_PAGE(register_page)))
       return;

    /* Get the text from the ledger */
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (register_page);
    reg = gnc_ledger_display_get_split_register (priv->ledger);
    help = gnc_table_get_help (reg->table);
    gnc_window_set_status (window, GNC_PLUGIN_PAGE (register_page), help);
    g_free (help);
}

static void
gnc_plugin_page_popup_menu_cb (GNCSplitReg* gsr,
                               GncPluginPageRegister* page)
{
    GncWindow* window;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));

    window = GNC_WINDOW (GNC_PLUGIN_PAGE (page)->window);
    if (!window)
    {
        // This routine can be called before the page is added to a
        // window.
        return;
    }
    gnc_main_window_popup_menu_cb (GTK_WIDGET (window),
                                   GNC_PLUGIN_PAGE (page));
}

static void
gnc_plugin_page_register_refresh_cb (GHashTable* changes, gpointer user_data)
{
    auto page = GNC_PLUGIN_PAGE_REGISTER(user_data);
    GncPluginPageRegisterPrivate* priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_REGISTER (page));
    priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);

    if (changes)
    {
        const EventInfo* ei;
        ei = gnc_gui_get_entity_events (changes, &priv->key);
        if (ei)
        {
            if (ei->event_mask & QOF_EVENT_DESTROY)
            {
                gnc_main_window_close_page (GNC_PLUGIN_PAGE (page));
                return;
            }
            if (ei->event_mask & QOF_EVENT_MODIFY)
            {
            }
        }
    }
    else
    {
        /* forced updates */
        gnucash_register_refresh_from_prefs (priv->gsr->reg);
        gtk_widget_queue_draw (priv->widget);
    }

    gnc_plugin_page_register_ui_update (NULL, page);
}

static void
gnc_plugin_page_register_close_cb (gpointer user_data)
{
    GncPluginPage* plugin_page = GNC_PLUGIN_PAGE (user_data);
    gnc_main_window_close_page (plugin_page);
}

/** This function is called when an account has been edited and an
 *  "extreme" change has been made to it.  (E.G. Changing from a
 *  credit card account to an expense account.  This routine is
 *  responsible for finding all open registers containing the account
 *  and closing them.
 *
 *  @param account A pointer to the account that was changed.
 */
static void
gppr_account_destroy_cb (Account* account)
{
    GncPluginPageRegister* page;
    GncPluginPageRegisterPrivate* priv;
    GNCLedgerDisplayType ledger_type;
    const GncGUID* acct_guid;
    const GList* citem;
    GList* item, *kill = NULL;

    acct_guid = xaccAccountGetGUID (account);

    /* Find all windows that need to be killed.  Don't kill them yet, as
     * that would affect the list being walked.*/
    citem = gnc_gobject_tracking_get_list (GNC_PLUGIN_PAGE_REGISTER_NAME);
    for (; citem; citem = g_list_next (citem))
    {
        page = (GncPluginPageRegister*)citem->data;
        priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
        ledger_type = gnc_ledger_display_type (priv->ledger);
        if (ledger_type == LD_GL)
        {
            kill = g_list_prepend (kill, page);
            /* kill it */
        }
        else if ((ledger_type == LD_SINGLE) || (ledger_type == LD_SUBACCOUNT))
        {
            if (guid_compare (acct_guid, &priv->key) == 0)
            {
                kill = g_list_prepend (kill, page);
            }
        }
    }

    kill = g_list_reverse (kill);
    /* Now kill them. */
    for (item = kill; item; item = g_list_next (item))
    {
        page = (GncPluginPageRegister*)item->data;
        gnc_main_window_close_page (GNC_PLUGIN_PAGE (page));
    }
    g_list_free (kill);
}

/** This function is the handler for all event messages from the
 *  engine.  Its purpose is to update the register page any time
 *  an account or transaction is changed.
 *
 *  @internal
 *
 *  @param entity A pointer to the affected item.
 *
 *  @param event_type The type of the affected item.
 *
 *  @param page A pointer to the register page.
 *
 *  @param ed
 */
static void
gnc_plugin_page_register_event_handler (QofInstance* entity,
                                        QofEventId event_type,
                                        GncPluginPageRegister* page,
                                        GncEventData* ed)
{
    Transaction* trans;
    QofBook* book;
    GncPluginPage* visible_page;
    GtkWidget* window;

    g_return_if_fail (page); /* Required */
    if (!GNC_IS_TRANS (entity) && !GNC_IS_ACCOUNT (entity))
        return;

    ENTER ("entity %p of type %d, page %p, event data %p",
           entity, event_type, page, ed);

    window = gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (page));

    if (GNC_IS_ACCOUNT (entity))
    {
        if (GNC_IS_MAIN_WINDOW (window))
        {
            GncPluginPageRegisterPrivate *priv = GNC_PLUGIN_PAGE_REGISTER_GET_PRIVATE (page);
            
            if (!gnc_ledger_display_leader (priv->ledger))
            {
                LEAVE ("account is NULL");
                return;
            }

            gchar *name = gnc_plugin_page_register_get_tab_name (GNC_PLUGIN_PAGE (page));
            main_window_update_page_name (GNC_PLUGIN_PAGE (page), name);

            gchar *long_name = gnc_plugin_page_register_get_long_name (GNC_PLUGIN_PAGE (page));
            main_window_update_page_long_name (GNC_PLUGIN_PAGE (page), long_name);

            gchar *color = gnc_plugin_page_register_get_tab_color (GNC_PLUGIN_PAGE (page));
            main_window_update_page_color (GNC_PLUGIN_PAGE (page), color);
            // update page icon if read only registers
            gnc_plugin_page_register_update_page_icon (GNC_PLUGIN_PAGE (page));

            g_free (color);
            g_free (name);
            g_free (long_name);
        }
        LEAVE ("tab contents updated");
        return;
    }

    if (! (event_type & (QOF_EVENT_MODIFY | QOF_EVENT_DESTROY)))
    {
        LEAVE ("not a modify");
        return;
    }
    trans = GNC_TRANS (entity);
    book = qof_instance_get_book (QOF_INSTANCE (trans));
    if (!gnc_plugin_page_has_book (GNC_PLUGIN_PAGE (page), book))
    {
        LEAVE ("not in this book");
        return;
    }

    if (GNC_IS_MAIN_WINDOW (window))
    {
        visible_page = gnc_main_window_get_current_page (GNC_MAIN_WINDOW (window));
        if (visible_page != GNC_PLUGIN_PAGE (page))
        {
            LEAVE ("page not visible");
            return;
        }
    }

    gnc_plugin_page_register_ui_update (NULL, page);
    LEAVE (" ");
    return;
}


/** @} */
/** @} */
