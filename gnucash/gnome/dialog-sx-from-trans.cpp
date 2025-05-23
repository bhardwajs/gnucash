/********************************************************************
 * dialog-sx-from-trans.c -- a simple dialog for creating a         *
 *                           scheduled transaction from a real one  *
 * Copyright (C) 2001 Robert Merkel <rgmerk@mira.net>               *
 * Copyright (C) 2001 Joshua Sled <jsled@asynchronous.org>          *
 * Copyright (c) 2006 David Hampton <hampton@employees.org>         *
 * Copyright (c) 2011 Robert Fewell                                 *
 *                                                                  *
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
 ********************************************************************/

#include <config.h>

#include "dialog-sx-editor.h"
#include "dialog-sx-from-trans.h"
#include "dialog-utils.h"
#include "gnc-component-manager.h"
#include "gnc-date.h"
#include "gnc-date-edit.h"
#include "gnc-dense-cal-store.h"
#include "gnc-dense-cal.h"
#include "gnc-engine.h"
#include "engine-helpers.h"
#include "gnc-prefs.h"
#include "gnc-ui-util.h"
#include "gnc-ui.h"
#include "qof.h"
#include "Recurrence.h"
#include "SchedXaction.h"
#include "SchedXaction.hpp"
#include "SX-book.h"
#include "SX-ttinfo.hpp"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define SXFTD_ERRNO_UNBALANCED_XACTION 3
#define SXFTD_ERRNO_OPEN_XACTION -3

#define SXFTD_EXCAL_NUM_MONTHS 4
#define SXFTD_EXCAL_MONTHS_PER_COL 4

#define SXFTD_RESPONSE_ADVANCED 100 /* 'Advanced' button response code */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN GNC_MOD_GUI_SX

static QofLogModule log_module = GNC_MOD_GUI_SX;

static void sxftd_freq_combo_changed( GtkWidget *w, gpointer user_data );
static void gnc_sx_trans_window_response_cb(GtkDialog *dialog, gint response, gpointer data);

static void sxftd_destroy( GtkWidget *w, gpointer user_data );

typedef enum { FREQ_DAILY = 0,  /* I know the =0 is redundant, but I'm using
                                 * the numeric equivalences explicitly here */
               FREQ_WEEKLY,
               FREQ_BIWEEKLY,
               FREQ_MONTHLY,
               FREQ_QUARTERLY,
               FREQ_ANNUALLY
             } SxftiFreqType;

typedef struct
{
    GtkBuilder *builder;
    GtkWidget *dialog;
    GtkEntry *name;
    GtkComboBox *freq_combo;

    GtkToggleButton *ne_but;
    GtkToggleButton *ed_but;
    GtkToggleButton *oc_but;
    GtkEntry *n_occurences;

    Transaction *trans;
    SchedXaction *sx;

    GncDenseCalStore *dense_cal_model;
    GncDenseCal *example_cal;

    GNCDateEdit *startDateGDE, *endDateGDE;

} SXFromTransInfo;

typedef struct
{
    gdcs_end_type type;
    GDate end_date;
    guint n_occurrences;
} getEndTuple;

static void sxftd_update_example_cal( SXFromTransInfo *sxfti );
static void sxftd_update_excal_adapt( GObject *o, gpointer ud );

struct widgetSignalHandlerTuple
{
    const gchar *name;
    const gchar *signal;
    void (*handlerFn)(GObject*,gpointer);
};

static void sxftd_ok_clicked(SXFromTransInfo *sxfti);
static void sxftd_advanced_clicked(SXFromTransInfo *sxfti);

static void
sxfti_attach_callbacks(SXFromTransInfo *sxfti)
{

    widgetSignalHandlerTuple callbacks[] =
    {
        /* Whenever any of the controls change, we want to update the
         * calendar. */
        { "never_end_button",     "clicked",      sxftd_update_excal_adapt },
        { "end_on_date_button",   "clicked",      sxftd_update_excal_adapt },
        { "n_occurrences_button", "clicked",      sxftd_update_excal_adapt },
        { "n_occurrences_entry",  "changed",      sxftd_update_excal_adapt },
        { NULL,                   NULL,           NULL }
    };

    int i;

    GtkWidget *w;
    for (i = 0; callbacks[i].name != NULL; i++)
    {
        w = GTK_WIDGET(gtk_builder_get_object(sxfti->builder, callbacks[i].name));

        g_signal_connect (G_OBJECT(w), callbacks[i].signal,
                          G_CALLBACK(callbacks[i].handlerFn),
                          sxfti );
    }

    g_signal_connect (G_OBJECT(sxfti->dialog), "response",
                      G_CALLBACK (gnc_sx_trans_window_response_cb),
                      sxfti);
}


static getEndTuple
sxftd_get_end_info(SXFromTransInfo *sxfti)
{
    getEndTuple retval;

    retval.type = BAD_END;
    g_date_clear( &(retval.end_date), 1 );
    retval.n_occurrences = 0;

    if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(sxfti->ne_but)))
    {
        retval.type = NEVER_END;
        return retval;
    }

    if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(sxfti->ed_but)))
    {
        time64 end_tt;
        retval.type = END_ON_DATE;
        g_date_clear( &(retval.end_date), 1 );
        end_tt = gnc_date_edit_get_date(sxfti->endDateGDE);
        gnc_gdate_set_time64( &(retval.end_date), end_tt);
        return retval;
    }

    if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(sxfti->oc_but) ))
    {
        guint n_occs;

        const gchar *text = gtk_entry_get_text (GTK_ENTRY(sxfti->n_occurences));
        if (!text || !text[0])
        {
            n_occs = 0;
        }
        else
        {
            gchar *endptr;
            n_occs = strtoul(text, &endptr, 10);
            if ( !endptr )
            {
                n_occs = -1;
            }
        }

        retval.type = END_AFTER_N_OCCS;
        retval.n_occurrences = n_occs;
        return retval;
    }
    return retval;
}


static guint
sxftd_add_template_trans(SXFromTransInfo *sxfti)
{

    Transaction *tr = sxfti->trans;
    GList *splits = NULL;
    TTInfoPtr tti = std::make_shared<TTInfo>();
    TTSplitInfoPtr ttsi;
    gnc_numeric runningBalance;
    gnc_numeric split_value;
    const char *tmpStr;

    runningBalance = gnc_numeric_zero();

    tti->set_description(xaccTransGetDescription(tr));
    tti->set_num(gnc_get_num_action(tr, NULL));
    tti->set_notes (xaccTransGetNotes (tr));
    tti->set_currency(xaccTransGetCurrency(tr));

    tti->clear_template_splits ();

    for (splits = xaccTransGetSplitList(tr); splits; splits = splits->next)
    {
        auto sp = GNC_SPLIT(splits->data);
        ttsi = std::make_shared<TTSplitInfo>();
        ttsi->set_action (gnc_get_num_action(NULL, sp));
        split_value = xaccSplitGetValue(sp);
        ttsi->set_memo (xaccSplitGetMemo(sp));

        runningBalance = gnc_numeric_add( runningBalance, split_value,
                                          100, (GNC_DENOM_AUTO | GNC_HOW_DENOM_LCD) );

        if (gnc_numeric_positive_p(split_value))
        {
            tmpStr = xaccPrintAmount( split_value,
                                      gnc_default_print_info(FALSE) );
            ttsi->set_debit_formula (tmpStr);
        }
        else
        {
            /* Negate the numeric so it prints w/o the sign at the front. */
            tmpStr = xaccPrintAmount( gnc_numeric_neg( split_value ),
                                      gnc_default_print_info(FALSE) );
            ttsi->set_credit_formula (tmpStr);
        }

        /* Copy over per-split account info */
        ttsi->set_account (xaccSplitGetAccount (sp));

        tti->append_template_split (ttsi);
    }

    if ( ! gnc_numeric_zero_p( runningBalance )
            && !gnc_verify_dialog (GTK_WINDOW (sxfti->dialog),
                                   FALSE, "%s",
                                   _("The Scheduled Transaction Editor "
                                     "cannot automatically balance "
                                     "this transaction. "
                                     "Should it still be "
                                     "entered?") ) )
    {
        return SXFTD_ERRNO_UNBALANCED_XACTION;
    }

    gnc_suspend_gui_refresh ();
    xaccSchedXactionSetTemplateTrans (sxfti->sx, { tti }, gnc_get_current_book ());
    gnc_resume_gui_refresh ();

    return 0;
}


static void
sxftd_update_schedule( SXFromTransInfo *sxfti, GDate *date, GList **recurrences)
{
    gint index;

    /* Note that we make the start date the *NEXT* instance, not the
     * present one. */

    index = gtk_combo_box_get_active(GTK_COMBO_BOX(sxfti->freq_combo));

    switch (index)
    {
    case FREQ_DAILY:
    {
        Recurrence *r = g_new0(Recurrence, 1);
        recurrenceSet(r, 1, PERIOD_DAY, date, WEEKEND_ADJ_NONE);
        *recurrences = g_list_append(*recurrences, r);
    }
    break;

    case FREQ_WEEKLY:
    case FREQ_BIWEEKLY:
    {
        Recurrence *r = g_new0(Recurrence, 1);
        int mult = (index == FREQ_BIWEEKLY ? 2 : 1);
        recurrenceSet(r, mult, PERIOD_WEEK, date, WEEKEND_ADJ_NONE);
        *recurrences = g_list_append(*recurrences, r);
    }
    break;

    case FREQ_MONTHLY:
    case FREQ_QUARTERLY:
    case FREQ_ANNUALLY:
    {
        Recurrence *r = g_new0(Recurrence, 1);
        int mult = (index == FREQ_MONTHLY
                    ? 1
                    : (index == FREQ_QUARTERLY
                       ? 3
                       : 12));
        recurrenceSet(r, mult, PERIOD_MONTH, date, recurrenceGetWeekendAdjust(r));
        *recurrences = g_list_append(*recurrences, r);
    }
    break;

    default:
        g_critical("nonexistent frequency selected");
        break;
    }
}


static gint
sxftd_init( SXFromTransInfo *sxfti )
{
    GtkWidget *w;
    const char *transName;
    gint pos;
    GList *schedule = NULL;
    time64 start_tt;
    GDate date, nextDate;

    if ( ! sxfti->sx )
    {
        return -1;
    }
    if ( ! sxfti->trans )
    {
        return -2;
    }
    if ( xaccTransIsOpen( sxfti->trans ) )
    {
        return SXFTD_ERRNO_OPEN_XACTION;
    }

    /* Setup Widgets */
    {
        sxfti->ne_but = GTK_TOGGLE_BUTTON(gtk_builder_get_object(sxfti->builder, "never_end_button"));
        sxfti->ed_but = GTK_TOGGLE_BUTTON(gtk_builder_get_object(sxfti->builder, "end_on_date_button"));
        sxfti->oc_but = GTK_TOGGLE_BUTTON(gtk_builder_get_object(sxfti->builder, "n_occurrences_button"));
        sxfti->n_occurences = GTK_ENTRY(gtk_builder_get_object(sxfti->builder, "n_occurrences_entry"));
    }

    /* Get the name from the transaction, try that as the initial SX name. */
    transName = xaccTransGetDescription( sxfti->trans );
    xaccSchedXactionSetName( sxfti->sx, transName );

    sxfti->name = GTK_ENTRY(gtk_builder_get_object(sxfti->builder, "name_entry" ));
    pos = 0;
    gtk_editable_insert_text( GTK_EDITABLE(sxfti->name), transName,
                              (strlen(transName) * sizeof(char)), &pos );

    sxfti_attach_callbacks(sxfti);

    /* Setup the example calendar and related data structures. */
    {
        int num_marks = SXFTD_EXCAL_NUM_MONTHS * 31;

        w = GTK_WIDGET(gtk_builder_get_object(sxfti->builder, "ex_cal_frame" ));
        sxfti->dense_cal_model = gnc_dense_cal_store_new(num_marks);
        sxfti->example_cal = GNC_DENSE_CAL(gnc_dense_cal_new_with_model (GTK_WINDOW(sxfti->dialog),
                                                                         GNC_DENSE_CAL_MODEL(sxfti->dense_cal_model)));
        g_object_ref_sink(sxfti->example_cal);

        g_assert(sxfti->example_cal);
        gnc_dense_cal_set_num_months( sxfti->example_cal, SXFTD_EXCAL_NUM_MONTHS );
        gnc_dense_cal_set_months_per_col( sxfti->example_cal, SXFTD_EXCAL_MONTHS_PER_COL );
        gtk_container_add( GTK_CONTAINER(w), GTK_WIDGET(sxfti->example_cal) );
    }

    /* Setup the start and end dates as GNCDateEdits */
    {
        GtkWidget *paramTable = GTK_WIDGET(gtk_builder_get_object(sxfti->builder, "param_table" ));
        sxfti->startDateGDE =  GNC_DATE_EDIT( gnc_date_edit_new (gnc_time (NULL), FALSE, FALSE));

        gtk_grid_attach (GTK_GRID(paramTable), GTK_WIDGET(sxfti->startDateGDE), 1, 2, 1, 1);
        gtk_widget_set_halign (GTK_WIDGET(sxfti->startDateGDE), GTK_ALIGN_FILL);
        gtk_widget_set_valign (GTK_WIDGET(sxfti->startDateGDE), GTK_ALIGN_FILL);
        gtk_widget_set_hexpand (GTK_WIDGET(sxfti->startDateGDE), TRUE);
        gtk_widget_set_vexpand (GTK_WIDGET(sxfti->startDateGDE), FALSE);
        g_object_set (GTK_WIDGET(sxfti->startDateGDE), "margin", 0, NULL);

        g_signal_connect( sxfti->startDateGDE, "date-changed",
                          G_CALLBACK( sxftd_update_excal_adapt ),
                          sxfti );
    }
    {
        GtkWidget *endDateBox = GTK_WIDGET(gtk_builder_get_object(sxfti->builder, "end_date_hbox" ));
        sxfti->endDateGDE =
            GNC_DATE_EDIT( gnc_date_edit_new (gnc_time (NULL),
                                              FALSE, FALSE));
        gtk_box_pack_start( GTK_BOX( endDateBox ),
                            GTK_WIDGET( sxfti->endDateGDE ),
                            TRUE, TRUE, 0 );
        g_signal_connect( sxfti->endDateGDE, "date-changed",
                          G_CALLBACK( sxftd_update_excal_adapt ),
                          sxfti );
    }

    /* Setup the initial start date for user display/confirmation */
    /* compute good initial date. */
    start_tt = xaccTransGetDate( sxfti->trans );
    gnc_gdate_set_time64( &date, start_tt );
    sxfti->freq_combo = GTK_COMBO_BOX(gtk_builder_get_object(sxfti->builder, "freq_combo_box"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(sxfti->freq_combo), 0);
    g_signal_connect( sxfti->freq_combo, "changed",
                      G_CALLBACK(sxftd_freq_combo_changed),
                      sxfti );
    sxftd_update_schedule( sxfti, &date, &schedule);
    recurrenceListNextInstance(schedule, &date, &nextDate);
    recurrenceListFree(&schedule);
    start_tt = gnc_time64_get_day_start_gdate (&nextDate);
    gnc_date_edit_set_time( sxfti->startDateGDE, start_tt );

    g_signal_connect( G_OBJECT(sxfti->name), "destroy",
                      G_CALLBACK(sxftd_destroy),
                      sxfti );

    sxftd_update_example_cal( sxfti );

    return 0;
}


static guint
sxftd_compute_sx(SXFromTransInfo *sxfti)
{
    GDate date;
    GList *schedule = NULL;
    getEndTuple end_info;
    guint sxftd_errno = 0; /* 0 == OK, > 0 means dialog needs to be run again */

    SchedXaction *sx = sxfti->sx;

    /* get the name */
    xaccSchedXactionSetName(sx, gtk_entry_get_text (sxfti->name));

    gnc_gdate_set_time64( &date, gnc_date_edit_get_date( sxfti->startDateGDE ) );

    sxftd_update_schedule(sxfti, &date, &schedule);
    if (sxftd_errno == 0)
    {
        gnc_sx_set_schedule(sx, schedule);
        xaccSchedXactionSetStartDate( sx, &date );
    }

    end_info = sxftd_get_end_info(sxfti);

    switch (end_info.type)
    {
    case NEVER_END:
        break;

    case END_ON_DATE:
        xaccSchedXactionSetEndDate(sx, &(end_info.end_date));
        break;

    case END_AFTER_N_OCCS:
        xaccSchedXactionSetNumOccur(sx, end_info.n_occurrences);
        break;

    default:
        sxftd_errno = 2;
        break;
    }

    gnc_sx_set_instance_count( sx, 1 );

    /* Set the autocreate, days-in-advance and remind-in-advance values from
     * options. */
    {
        gboolean autoCreateState, notifyState;
        gint daysInAdvance;

        autoCreateState =
            gnc_prefs_get_bool (GNC_PREFS_GROUP_SXED, GNC_PREF_CREATE_AUTO);
        notifyState =
            gnc_prefs_get_bool (GNC_PREFS_GROUP_SXED, GNC_PREF_NOTIFY);
        xaccSchedXactionSetAutoCreate( sx,
                                       autoCreateState,
                                       (autoCreateState & notifyState) );

        daysInAdvance =
            gnc_prefs_get_float (GNC_PREFS_GROUP_SXED, GNC_PREF_CREATE_DAYS);
        xaccSchedXactionSetAdvanceCreation( sx, daysInAdvance );

        daysInAdvance =
            gnc_prefs_get_float (GNC_PREFS_GROUP_SXED, GNC_PREF_REMIND_DAYS);
        xaccSchedXactionSetAdvanceReminder( sx, daysInAdvance );
    }

    if ( sxftd_add_template_trans( sxfti ) != 0 )
    {
        sxftd_errno = SXFTD_ERRNO_UNBALANCED_XACTION;
    }

    return sxftd_errno;
}


static void
sxftd_close(SXFromTransInfo *sxfti, gboolean delete_sx)
{
    if ( sxfti->sx && delete_sx )
    {
        gnc_sx_begin_edit(sxfti->sx);
        xaccSchedXactionDestroy(sxfti->sx);
    }
    sxfti->sx = NULL;

    gtk_widget_destroy (GTK_WIDGET (sxfti->dialog));
}


static void
sxftd_ok_clicked(SXFromTransInfo *sxfti)
{
    QofBook *book;
    SchedXactions *sxes;
    guint sx_error = sxftd_compute_sx(sxfti);

    if (sx_error != 0
            && sx_error != SXFTD_ERRNO_UNBALANCED_XACTION)
    {
        g_critical("sxftd_compute_sx after ok_clicked [%d]", sx_error);
    }
    else
    {
        if ( sx_error == SXFTD_ERRNO_UNBALANCED_XACTION )
        {
            gnc_error_dialog (GTK_WINDOW (sxfti->dialog), "%s",
                              _( "The Scheduled Transaction is unbalanced. "
                                 "You are strongly encouraged to correct this situation." ) );
        }
        book = gnc_get_current_book ();
        sxes = gnc_book_get_schedxactions(book);
        gnc_sxes_add_sx(sxes, sxfti->sx);
    }

    sxftd_close(sxfti, FALSE);
    return;
}


/**
 * Update start date... right now we always base this off the transaction
 * start date, but ideally we want to respect what the user has in the field,
 * somehow.
 **/
static void
sxftd_freq_combo_changed( GtkWidget *w, gpointer user_data )
{
    SXFromTransInfo *sxfti = (SXFromTransInfo*)user_data;
    GDate date, nextDate;
    time64 tmp_tt;
    GList *schedule = NULL;

    tmp_tt = xaccTransGetDate( sxfti->trans );
    gnc_gdate_set_time64 (&date, tmp_tt);

    g_date_clear(&nextDate, 1);
    sxftd_update_schedule(sxfti, &date, &schedule);
    recurrenceListNextInstance(schedule, &date, &nextDate);
    tmp_tt = gnc_time64_get_day_start_gdate (&nextDate);
    gnc_date_edit_set_time( sxfti->startDateGDE, tmp_tt );

    recurrenceListFree(&schedule);
    sxftd_update_example_cal( sxfti );
}


static void
sxftd_advanced_clicked(SXFromTransInfo *sxfti)
{
    guint sx_error = sxftd_compute_sx(sxfti);
    GMainContext *context;

    if ( sx_error != 0 && sx_error != SXFTD_ERRNO_UNBALANCED_XACTION )
    {
        // unbalanced-xaction is "okay", since this is also checked for by
        // the advanced editor.
        g_warning("something bad happened in sxftd_compute_sx [%d]", sx_error);
        return;
    }
    gtk_widget_hide( sxfti->dialog );
    /* force a gui update. */
    context = g_main_context_default();
    while (g_main_context_iteration(context, FALSE));

    gnc_ui_scheduled_xaction_editor_dialog_create(gnc_ui_get_main_window (sxfti->dialog),
        sxfti->sx, TRUE /* newSX */);
    /* close ourself, since advanced editing entails us, and there are sync
     * issues otherwise. */
    sxftd_close(sxfti, FALSE);
}


static void
sxftd_destroy( GtkWidget *w, gpointer user_data )
{
    SXFromTransInfo *sxfti = (SXFromTransInfo*)user_data;

    if ( sxfti->sx )
    {
        gnc_sx_begin_edit(sxfti->sx);
        xaccSchedXactionDestroy(sxfti->sx);
        sxfti->sx = NULL;
    }
    if (sxfti->dense_cal_model)
        g_object_unref(G_OBJECT(sxfti->dense_cal_model));
    if (sxfti->example_cal)
        g_object_unref(G_OBJECT(sxfti->example_cal));

    g_free(sxfti);
}


static void
gnc_sx_trans_window_response_cb (GtkDialog *dialog,
                                 gint response,
                                 gpointer data)
{
    SXFromTransInfo *sxfti = (SXFromTransInfo *)data;

    ENTER(" dialog %p, response %d, sx %p", dialog, response, sxfti);
    switch (response)
    {
    case GTK_RESPONSE_OK:
        DEBUG(" OK");
        sxftd_ok_clicked(sxfti);
        break;
    case SXFTD_RESPONSE_ADVANCED:
        DEBUG(" ADVANCED");
        sxftd_advanced_clicked(sxfti);
        break;
    case GTK_RESPONSE_CANCEL:
    default:
        DEBUG(" CANCEL");
        sxftd_close(sxfti, TRUE);
        break;

    }
    LEAVE(" ");
}


/**
 * Update the example calendar; make sure to take into account the end
 * specification.
 **/
static void
sxftd_update_example_cal( SXFromTransInfo *sxfti )
{
    time64 tmp_tt;
    GDate date, startDate, nextDate;
    GList *schedule = NULL;
    getEndTuple get;

    get = sxftd_get_end_info( sxfti );

    tmp_tt = gnc_date_edit_get_date( sxfti->startDateGDE );
    gnc_gdate_set_time64 (&date, tmp_tt);

    sxftd_update_schedule(sxfti, &date, &schedule);

    /* go one day before what's in the box so we can get the correct start
     * date. */
    startDate = date;
    g_date_subtract_days(&date, 1);
    g_date_clear(&nextDate, 1);
    recurrenceListNextInstance(schedule, &date, &nextDate);

    gnc_dense_cal_store_update_name (sxfti->dense_cal_model,
                                     gtk_entry_get_text (sxfti->name));

    {
        gchar *schedule_desc;
        schedule_desc = recurrenceListToCompactString(schedule);
        gnc_dense_cal_store_update_info(sxfti->dense_cal_model, schedule_desc);
        g_free(schedule_desc);
    }

    /* Set End date sensitivity */
    gtk_widget_set_sensitive( GTK_WIDGET(sxfti->endDateGDE), (get.type == END_ON_DATE) );
    gtk_widget_set_sensitive( GTK_WIDGET(sxfti->n_occurences), (get.type == END_AFTER_N_OCCS) );

    /* Use the day preceding the start date for the store to find the correct real start date */
    switch (get.type)
    {
    case NEVER_END:
        gnc_dense_cal_store_update_recurrences_no_end(sxfti->dense_cal_model, &date, schedule);
        break;
    case END_ON_DATE:
        gnc_dense_cal_store_update_recurrences_date_end(sxfti->dense_cal_model, &date, schedule, &get.end_date);
        break;
    case END_AFTER_N_OCCS:
        gnc_dense_cal_store_update_recurrences_count_end(sxfti->dense_cal_model, &date, schedule, get.n_occurrences);
        break;
    default:
        g_warning("unknown get.type [%d]\n", get.type);
        break;
    }

    gnc_dense_cal_set_month( sxfti->example_cal, g_date_get_month( &startDate ) );
    gnc_dense_cal_set_year( sxfti->example_cal, g_date_get_year( &startDate ) );

    recurrenceListFree(&schedule);
}


/***********************************
 * Callback to update the calendar *
 **********************************/
static void
sxftd_update_excal_adapt( GObject *o, gpointer ud )
{
    SXFromTransInfo *sxfti = (SXFromTransInfo*)ud;
    sxftd_update_example_cal( sxfti );
}


/*********************
 * Create the dialog *
 ********************/
void
gnc_sx_create_from_trans( GtkWindow *parent, Transaction *trans )
{
    int sx_error;
    SXFromTransInfo *sxfti = g_new0( SXFromTransInfo, 1);
    GtkBuilder *builder;
    GtkWidget *dialog;

    builder = gtk_builder_new();

    gnc_builder_add_from_file  (builder , "dialog-sx.glade", "freq_liststore");

    gnc_builder_add_from_file  (builder , "dialog-sx.glade", "sx_from_real_trans_dialog");
    dialog = GTK_WIDGET(gtk_builder_get_object (builder, "sx_from_real_trans_dialog"));

    // Set the name of this dialog so it can be easily manipulated with css
    gtk_widget_set_name (GTK_WIDGET(dialog), "gnc-id-sx-from-trans");
    gnc_widget_style_context_add_class (GTK_WIDGET(dialog), "gnc-class-sx");

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    sxfti->builder = builder;
    sxfti->dialog = dialog;
    sxfti->trans = trans;

    sxfti->sx = xaccSchedXactionMalloc(gnc_get_current_book ());

    if ( (sx_error = sxftd_init( sxfti )) < 0 )
    {
        if ( sx_error == SXFTD_ERRNO_OPEN_XACTION )
        {
            gnc_error_dialog (NULL, "%s",
                              _( "Cannot create a Scheduled Transaction "
                                 "from a Transaction currently "
                                 "being edited. Please Enter the "
                                 "Transaction before Scheduling." ) );
            sxftd_destroy (NULL, sxfti);
            return;
        }
        else
        {
            g_error("sxftd_init: %d", sx_error);
        }
    }

    gtk_widget_show_all(GTK_WIDGET(sxfti->dialog));

    gtk_builder_connect_signals(builder, sxfti);
    g_object_unref(G_OBJECT(builder));
}
