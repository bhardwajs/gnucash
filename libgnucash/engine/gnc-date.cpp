/********************************************************************\
 * gnc-date.cpp -- C interface for date and time                    *
 *                                                                  *
 * Copyright 1997 Robin D. Clark <rclark@cs.hmc.edu>                *
 * Copyright 1998-2000, 2003 Linas Vepstas <linas@linas.org>        *
 * Copyright (C) 2005 David Hampton <hampton@employees.org>         *
 * Copyright 2011-2015 John Ralls <jralls@ceridwen.us               *
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
 *                                                                  *
\********************************************************************/

#include <glib.h>

#include <config.h>
#include <libintl.h>
#include <stdlib.h>
#include "platform.h"
#include "qof.h"

#ifdef HAVE_LANGINFO_D_FMT
# include <langinfo.h>
#endif
#ifndef HAVE_STRPTIME
#include <strptime.h>
#endif
#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#include <cinttypes>
#include <unicode/calendar.h>

#include "gnc-date.h"
#include "gnc-date-p.h"
#include "gnc-datetime.hpp"
#include "gnc-timezone.hpp"
#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/date_time/local_time/local_time.hpp>

#define N_(string) string //So that xgettext will find it

#ifdef HAVE_LANGINFO_D_FMT
#  define GNC_D_FMT (nl_langinfo (D_FMT))
#  define GNC_D_T_FMT (nl_langinfo (D_T_FMT))
#  define GNC_T_FMT (nl_langinfo (T_FMT))
#elif defined(G_OS_WIN32)
#  define GNC_D_FMT (qof_win32_get_time_format(QOF_WIN32_PICTURE_DATE))
#  define GNC_T_FMT (qof_win32_get_time_format(QOF_WIN32_PICTURE_TIME))
#  define GNC_D_T_FMT (qof_win32_get_time_format(QOF_WIN32_PICTURE_DATETIME))
#else
#  define GNC_D_FMT "%Y-%m-%d"
#  define GNC_D_T_FMT "%Y-%m-%d %r"
#  define GNC_T_FMT "%r"
#endif

const char *gnc_default_strftime_date_format =
#ifdef G_OS_WIN32
    /* The default date format for use with strftime in Win32. */
    N_("%B %#d, %Y")
#else
    /* The default date format for use with strftime in other OS. */
    /* Translators: call "man strftime" for possible values. */
    N_("%B %e, %Y")
#endif
    ;

/* This is now user configured through the gnome options system() */
static QofDateFormat dateFormat = QOF_DATE_FORMAT_LOCALE;
static QofDateFormat prevQofDateFormat = QOF_DATE_FORMAT_LOCALE;

static QofDateCompletion dateCompletion = QOF_DATE_COMPLETION_THISYEAR;
static int dateCompletionBackMonths = 6;

/* This static indicates the debugging module that this .o belongs to. */
static QofLogModule log_module = QOF_MOD_ENGINE;

/****************** Posix Replacement Functions ***************************/
void
gnc_tm_free (struct tm* time)
{
    free(time);
}

struct tm*
gnc_localtime (const time64 *secs)
{
    auto time = static_cast<struct tm*>(calloc(1, sizeof(struct tm)));
    if (gnc_localtime_r (secs, time) == nullptr)
    {
        gnc_tm_free (time);
        return nullptr;
    }
    return time;
}

struct tm*
gnc_localtime_r (const time64 *secs, struct tm* time)
{
    try
    {
        *time = static_cast<struct tm>(GncDateTime(*secs));
        return time;
    }
    catch(std::invalid_argument&)
    {
        return nullptr;
    }
}

static void
normalize_time_component (int *inner, int *outer, int divisor)
{
     while (*inner < 0)
     {
          --(*outer);
          *inner += divisor;
     }
     while (*inner >= divisor)
     {
          ++(*outer);
          *inner -= divisor;
     }
}

static void
normalize_struct_tm (struct tm* time)
{
     gint year = time->tm_year + 1900;
     gint last_day;

     /* Gregorian_date throws if it gets an out-of-range year
      * so clamp year into gregorian_date's range.
      */
     if (year < 1400) year += 1400;
     if (year > 9999) year %= 10000;

     normalize_time_component (&(time->tm_sec), &(time->tm_min), 60);
     normalize_time_component (&(time->tm_min), &(time->tm_hour), 60);
     normalize_time_component (&(time->tm_hour), &(time->tm_mday), 24);
     normalize_time_component (&(time->tm_mon), &year, 12);

     // auto month_in_range = []int (int m){ return (m + 12) % 12; }
     while (time->tm_mday < 1)
     {
         normalize_time_component (&(--time->tm_mon), &year, 12);
         last_day = gnc_date_get_last_mday (time->tm_mon, year);
         time->tm_mday += last_day;
     }
     last_day = gnc_date_get_last_mday (time->tm_mon, year);
     while (time->tm_mday > last_day)
     {
          time->tm_mday -= last_day;
          normalize_time_component(&(++time->tm_mon), &year, 12);
          last_day = gnc_date_get_last_mday (time->tm_mon, year);
     }
     time->tm_year = year - 1900;
}

struct tm*
gnc_gmtime (const time64 *secs)
{
    try
    {
        GncDateTime gncdt(*secs);
        auto time = static_cast<struct tm*>(calloc(1, sizeof(struct tm)));
        *time = gncdt.utc_tm();
        return time;
    }
    catch(std::invalid_argument&)
    {
        return nullptr;
    }

}

gint
gnc_start_of_week (void)
{
    /* icu's day of week is 1 based. Using 0 here to mean unset or error while setting */
    static int cached_result = 0;

    if (!cached_result)
    {
        UErrorCode err = U_ZERO_ERROR;
        auto cal = icu::Calendar::createInstance (err);
        if (!cal)
        {
            PERR("ICU error: %s\n", u_errorName (err));
            return 0;
        }

        /* 1 for sunday, 2 for monday, etc. */
        cached_result = cal->getFirstDayOfWeek (err);
        delete cal;
    }

    return cached_result;
}

time64
gnc_mktime (struct tm* time)
{
    try
    {
        normalize_struct_tm (time);
        GncDateTime gncdt(*time);
        *time = static_cast<struct tm>(gncdt);
        return static_cast<time64>(gncdt);
    }
    catch(std::invalid_argument&)
    {
        return 0;
    }
}

time64
gnc_timegm (struct tm* time)
{
    try
    {
        normalize_struct_tm(time);
        GncDateTime gncdt(*time);
        *time = static_cast<struct tm>(gncdt);
        time->tm_sec -= gncdt.offset();
        normalize_struct_tm(time);
#ifdef HAVE_STRUcT_TM_GMTOFF
        time->tm_gmtoff = 0;
#endif
        return static_cast<time64>(gncdt) - gncdt.offset();
    }
    catch(std::invalid_argument&)
    {
        return 0;
    }
}

char*
gnc_ctime (const time64 *secs)
{
    return gnc_print_time64(*secs, "%a %b %d %H:%M:%S %Y");
}

time64
gnc_time (time64 *tbuf)
{
    GncDateTime gncdt;
    auto time = static_cast<time64>(gncdt);
    if (tbuf != nullptr)
        *tbuf = time;
    return time;
}

gdouble
gnc_difftime (const time64 secs1, const time64 secs2)
{
    PWARN ("gnc_difftime is deprecated");
    return (double)secs1 - (double)secs2;
}

/****************************************************************************/

const char*
gnc_date_dateformat_to_string(QofDateFormat format)
{
    switch (format)
    {
    case QOF_DATE_FORMAT_US:
        return "us";
    case QOF_DATE_FORMAT_UK:
        return "uk";
    case QOF_DATE_FORMAT_CE:
        return "ce";
    case QOF_DATE_FORMAT_ISO:
        return "iso";
    case QOF_DATE_FORMAT_UTC:
        return "utc";
    case QOF_DATE_FORMAT_LOCALE:
        return "locale";
    case QOF_DATE_FORMAT_CUSTOM:
        return "custom";
    case QOF_DATE_FORMAT_UNSET:
        return "unset";
    default:
        return nullptr;
    }
}

gboolean
gnc_date_string_to_dateformat(const char* fmt_str, QofDateFormat *format)
{
    if (!fmt_str)
        return TRUE;

    if (!strcmp(fmt_str, "us"))
        *format = QOF_DATE_FORMAT_US;
    else if (!strcmp(fmt_str, "uk"))
        *format = QOF_DATE_FORMAT_UK;
    else if (!strcmp(fmt_str, "ce"))
        *format = QOF_DATE_FORMAT_CE;
    else if (!strcmp(fmt_str, "utc"))
        *format = QOF_DATE_FORMAT_UTC;
    else if (!strcmp(fmt_str, "iso"))
        *format = QOF_DATE_FORMAT_ISO;
    else if (!strcmp(fmt_str, "locale"))
        *format = QOF_DATE_FORMAT_LOCALE;
    else if (!strcmp(fmt_str, "custom"))
        *format = QOF_DATE_FORMAT_CUSTOM;
    else if (!strcmp(fmt_str, "unset"))
        *format = QOF_DATE_FORMAT_UNSET;
    else
        return TRUE;

    return FALSE;
}

const char*
gnc_date_monthformat_to_string(GNCDateMonthFormat format)
{
 //avoid UB if format is out of range
    switch (static_cast<uint8_t>(format))
    {
    case GNCDATE_MONTH_NUMBER:
        return "number";
    case GNCDATE_MONTH_ABBREV:
        return "abbrev";
    case GNCDATE_MONTH_NAME:
        return "name";
    default:
        return nullptr;
    }
}

gboolean
gnc_date_string_to_monthformat(const char *fmt_str, GNCDateMonthFormat *format)
{
    if (!fmt_str)
        return TRUE;

    if (!strcmp(fmt_str, "number"))
        *format = GNCDATE_MONTH_NUMBER;
    else if (!strcmp(fmt_str, "abbrev"))
        *format = GNCDATE_MONTH_ABBREV;
    else if (!strcmp(fmt_str, "name"))
        *format = GNCDATE_MONTH_NAME;
    else
        return TRUE;

    return FALSE;
}

char*
gnc_print_time64(time64 time, const char* format)
{
    try
    {
        GncDateTime gncdt(time);
        auto sstr = gncdt.format(format);
        //ugly C allocation so that the ptr can be freed at the other end
        char* cstr = static_cast<char*>(malloc(sstr.length() + 1));
        memset(cstr, 0, sstr.length() + 1);
        strncpy(cstr, sstr.c_str(), sstr.length());
        return cstr;
    }
    catch(std::runtime_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", time, err.what());
        return nullptr;
    }
    catch(std::logic_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", time, err.what());
        return nullptr;
    }
}

/********************************************************************\
\********************************************************************/


/* Converts any time on a day to midday that day.

 * given a timepair contains any time on a certain day (local time)
 * converts it to be midday that day.
 */
time64
time64CanonicalDayTime (time64 t)
{
    struct tm tm;
    gnc_localtime_r(&t, &tm);
    gnc_tm_set_day_middle(&tm);
    return gnc_mktime (&tm);
}

/* NB: month is 1-12, year is 0001 - 9999. */
int gnc_date_get_last_mday (int month, int year)
{
    static int last_day_of_month[12] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
    g_assert(month >= 0 && month < 12);

    // To be a leap year, the year number must be divisible by four,
    // except for end-of-century years, which must be divisible by 400.

    return last_day_of_month[month] +
        (month == 1 && year % 4 == 0 && !(year % 100 == 0 && year % 400 != 0) ?
        1 : 0);
}

QofDateFormat qof_date_format_get (void)
{
    return dateFormat;
}

void qof_date_format_set(QofDateFormat df)
{
//avoid UB if df is out of range
    auto dfi{static_cast<uint8_t>(df)};
    if (dfi >= DATE_FORMAT_FIRST && dfi <= DATE_FORMAT_LAST)
    {
        prevQofDateFormat = dateFormat;
        dateFormat = df;
    }
    else
    {
        /* hack alert - Use a neutral default. */
        PERR("non-existent date format set attempted. Setting ISO default");
        prevQofDateFormat = dateFormat;
        dateFormat = QOF_DATE_FORMAT_ISO;
    }

    return;
}

/* set date completion method

set dateCompletion to one of QOF_DATE_COMPLETION_THISYEAR (for
completing the year to the current calendar year) or
QOF_DATE_COMPLETION_SLIDING (for using a sliding 12-month window). The
sliding window starts 'backmonth' months before the current month (0-11).
checks to make sure it's a legal value

param QofDateCompletion: indicates preferred completion method
param int: the number of months to go back in time (0-11)

return void

Globals: dateCompletion dateCompletionBackMonths
*/
void qof_date_completion_set(QofDateCompletion dc, int backmonths)
{
    if (dc == QOF_DATE_COMPLETION_THISYEAR ||
            dc == QOF_DATE_COMPLETION_SLIDING)
    {
        dateCompletion = dc;
    }
    else
    {
        /* hack alert - Use a neutral default. */
        PERR("non-existent date completion set attempted. Setting current year completion as default");
        dateCompletion = QOF_DATE_COMPLETION_THISYEAR;
    }

    if (backmonths < 0)
    {
        backmonths = 0;
    }
    else if (backmonths > 11)
    {
        backmonths = 11;
    }
    dateCompletionBackMonths = backmonths;

    return;
}

/*
 qof_date_format_get_string
 get the date format string for the current format
 returns: string

 Globals: dateFormat
*/
const gchar *qof_date_format_get_string(QofDateFormat df)
{
    switch (df)
    {
    case QOF_DATE_FORMAT_US:
        return "%m/%d/%Y";
    case QOF_DATE_FORMAT_UK:
        return "%d/%m/%Y";
    case QOF_DATE_FORMAT_CE:
        return "%d.%m.%Y";
    case QOF_DATE_FORMAT_UTC:
        return "%Y-%m-%dT%H:%M:%SZ";
    case QOF_DATE_FORMAT_ISO:
        return "%Y-%m-%d";
    case QOF_DATE_FORMAT_UNSET: // use global
        return qof_date_format_get_string (dateFormat);
    case QOF_DATE_FORMAT_LOCALE:
    default:
        break;
    };
    return GNC_D_FMT;
}

const gchar *qof_date_text_format_get_string(QofDateFormat df)
{
    switch (df)
    {
    case QOF_DATE_FORMAT_US:
        return "%b %d, %Y";
    case QOF_DATE_FORMAT_UK:
    case QOF_DATE_FORMAT_CE:
        return "%d %b %Y";
    case QOF_DATE_FORMAT_UTC:
        return "%Y-%m-%dT%H:%M:%SZ";
    case QOF_DATE_FORMAT_ISO:
        return "%Y-%b-%d";
    case QOF_DATE_FORMAT_UNSET: // use global
        return qof_date_text_format_get_string (dateFormat);
    case QOF_DATE_FORMAT_LOCALE:
    default:
        break;
    };
    return GNC_D_FMT;
}

size_t
qof_print_date_dmy_buff (char * buff, const size_t len, int day, int month, int year)
{
    if (!buff) return 0;

    try
    {
        GncDate date(year, month, day);
        std::string str = date.format(qof_date_format_get_string(dateFormat));
        strncpy(buff, str.c_str(), len);
        if (str.length() >= len)
            buff[len - 1] = '\0';
    }
    catch(std::logic_error& err)
    {
        PWARN("Error processing year-month-day %d-%d-%d: %s",
              year, month, day, err.what());
    }
    catch(std::runtime_error& err)
    {
        PWARN("Error processing year-month-day %d-%d-%d: %s",
              year, month, day, err.what());
    }
    return strlen(buff);
}

size_t
qof_print_date_buff (char * buff, const size_t len, time64 t)
{
    if (!buff) return 0;

    try
    {
        GncDateTime gncdt(t);
        std::string str = gncdt.format(qof_date_format_get_string(dateFormat));
        strncpy(buff, str.c_str(), len);
        if (str.length() >= len)
            buff[len - 1] = '\0';
    }
    catch(std::logic_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", t, err.what());
    }
    catch(std::runtime_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", t, err.what());
    }
    return strlen(buff);
}

size_t
qof_print_gdate( char *buf, size_t len, const GDate *gd )
{
    GDate date;
    g_date_clear (&date, 1);
    date = *gd;
    return qof_print_date_dmy_buff( buf, len,
                                    g_date_get_day(&date),
                                    g_date_get_month(&date),
                                    g_date_get_year(&date) );
}

char *
qof_print_date (time64 t)
{
    char buff[MAX_DATE_LENGTH + 1];
    memset (buff, 0, sizeof (buff));
    qof_print_date_buff (buff, MAX_DATE_LENGTH, t);
    return g_strdup (buff);
}

/* ============================================================== */

/* return the greatest integer <= a/b; works for b > 0 and positive or
   negative a. */
static int
floordiv(int a, int b)
{
    if (a >= 0)
    {
        return a / b;
    }
    else
    {
        return - ((-a - 1) / b) - 1;
    }
}

/* Normalize the localized date format to avoid date scanning issues.
 *
 *   The 'O' and 'E' format modifiers are for localized input/output
 *   characters. Remove them as we are always using Arabic numbers.
 */
static inline std::string
normalize_format (const std::string& format)
{
    bool is_pct = false;
    std::string normalized;
    std::remove_copy_if(
        format.begin(), format.end(), back_inserter(normalized),
        [&is_pct](char e){
            bool r = (is_pct && (e == 'E' || e == 'O' || e == '-'));
            is_pct = e == '%';
            return r;
        });
    return normalized;
}

/* Convert a string into  day, month and year integers

    Convert a string into  day / month / year integers according to
    the current dateFormat value.

    This function will always parse a single number as the day of
    the month, regardless of the ordering of the dateFormat value.
    Two numbers will always be parsed as the day and the month, in
    the same order that they appear in the dateFormat value.  Three
    numbers are parsed exactly as specified in the dateFormat field.

    Fully formatted UTC timestamp strings are converted separately.

    param   buff - pointer to date string
    param     day -  will store day of the month as 1 ... 31
    param     month - will store month of the year as 1 ... 12
    param     year - will store the year (4-digit)

    return TRUE if date appeared to be valid.

    Globals: global dateFormat value
*/
static gboolean
qof_scan_date_internal (const char *buff, int *day, int *month, int *year,
                        QofDateFormat which_format)
{
    char *dupe, *tmp, *first_field, *second_field, *third_field;
    int iday, imonth, iyear;
    int now_day, now_month, now_year;
    struct tm *now, utc;
    time64 secs;

    if (!buff) return(FALSE);

    if (which_format == QOF_DATE_FORMAT_UTC)
    {
        if (strptime(buff, QOF_UTC_DATE_FORMAT, &utc)
            || strptime (buff, "%Y-%m-%d", &utc))
        {
            *day = utc.tm_mday;
            *month = utc.tm_mon + 1;
            *year = utc.tm_year + 1900;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    dupe = g_strdup (buff);

    tmp = dupe;
    first_field = nullptr;
    second_field = nullptr;
    third_field = nullptr;

    /* Use strtok to find delimiters */
    if (tmp)
    {
        static const char *delims = ".,-+/\\()년월年月 ";

        first_field = strtok (tmp, delims);
        if (first_field)
        {
            second_field = strtok (nullptr, delims);
            if (second_field)
            {
                third_field = strtok (nullptr, delims);
            }
        }
    }

    /* today's date */
    gnc_time (&secs);
    now = gnc_localtime (&secs);
    if (!now)
        return FALSE;
    now_day = now->tm_mday;
    now_month = now->tm_mon + 1;
    now_year = now->tm_year + 1900;
    gnc_tm_free (now);

    /* set defaults: if day or month appear to be blank, use today's date */
    iday = now_day;
    imonth = now_month;
    iyear = -1;

    /* get numeric values */
    switch (which_format)
    {
    case QOF_DATE_FORMAT_LOCALE:
        if (buff[0] != '\0')
        {
            struct tm thetime;
            /* Parse time string. */
            memset(&thetime, -1, sizeof(struct tm));
            char *strv = strptime (buff, normalize_format(GNC_D_FMT).c_str(),
                                   &thetime);

            if (third_field)
            {
                if (!strv) // Parse failed, continuing gives the wrong result.
                    return FALSE;

                /* Easy.  All three values were parsed. */
                iyear = thetime.tm_year + 1900;
                iday = thetime.tm_mday;
                imonth = thetime.tm_mon + 1;
            }
            else if (second_field)
            {
                /* Hard. Two values parsed.  Figure out the ordering. */
                if (thetime.tm_year == -1)
                {
                    /* %m-%d or %d-%m. Don't care. Already parsed correctly. */
                    iday = thetime.tm_mday;
                    imonth = thetime.tm_mon + 1;
                }
                else if (thetime.tm_mon != -1)
                {
                    /* Must be %Y-%m-%d. Reparse as %m-%d.*/
                    imonth = atoi(first_field);
                    iday = atoi(second_field);
                }
                else
                {
                    /* Must be %Y-%d-%m. Reparse as %d-%m. */
                    iday = atoi(first_field);
                    imonth = atoi(second_field);
                }
            }
            else if (first_field)
            {
                iday = atoi(first_field);
            }
        }
        break;
    case QOF_DATE_FORMAT_UK:
    case QOF_DATE_FORMAT_CE:
        if (third_field)
        {
            iday = atoi(first_field);
            imonth = atoi(second_field);
            iyear = atoi(third_field);
        }
        else if (second_field)
        {
            iday = atoi(first_field);
            imonth = atoi(second_field);
        }
        else if (first_field)
        {
            iday = atoi(first_field);
        }
        break;
    case QOF_DATE_FORMAT_ISO:
        if (third_field)
        {
            iyear = atoi(first_field);
            imonth = atoi(second_field);
            iday = atoi(third_field);
        }
        else if (second_field)
        {
            imonth = atoi(first_field);
            iday = atoi(second_field);
        }
        else if (first_field)
        {
            iday = atoi(first_field);
        }
        break;
    case QOF_DATE_FORMAT_US:
    default:
        if (third_field)
        {
            imonth = atoi(first_field);
            iday = atoi(second_field);
            iyear = atoi(third_field);
        }
        else if (second_field)
        {
            imonth = atoi(first_field);
            iday = atoi(second_field);
        }
        else if (first_field)
        {
            iday = atoi(first_field);
        }
        break;
    }

    g_free (dupe);

    if ((imonth == 0) || (iday == 0))
        return FALSE;

    if ((12 < imonth) || (31 < iday))
    {
        /*
         * Ack! Thppfft!  Someone just fed this routine a string in the
         * wrong date format.  This is known to happen if a register
         * window is open when changing the date format.  Try the
         * previous date format.  If that doesn't work, see if we can
         * exchange month and day. If that still doesn't work,
         * bail and give the caller what they asked for (garbage)
         * parsed in the new format.
         *
         * Note: This test cannot detect any format change that only
         * swaps month and day field, if the day is 12 or less.  This is
         * deemed acceptable given the obscurity of this bug.
         */
        if ((which_format != prevQofDateFormat) &&
                qof_scan_date_internal(buff, day, month, year, prevQofDateFormat))
        {
            return(TRUE);
        }
        if ((12 < imonth) && (12 >= iday))
        {
            int tmp = imonth;
            imonth = iday;
            iday = tmp;
        }
        else
        {
            return FALSE;
        }
    }

    /* if no year was entered, choose a year according to the
       dateCompletion preference. If it is
       QOF_DATE_COMPLETION_THISYEAR, use the current year, else if it
       is QOF_DATE_COMPLETION_SLIDING, use a sliding window that
       starts dateCompletionBackMonths before the current month.

       We go by whole months, rather than days, because presumably
       this is less confusing.
    */

    if (iyear == -1)
    {
        if (dateCompletion == QOF_DATE_COMPLETION_THISYEAR)
        {
            iyear = now_year;  /* use the current year */
        }
        else
        {
            iyear = now_year - floordiv(imonth - now_month +
                                        dateCompletionBackMonths, 12);
        }
    }

    /* If the year entered is smaller than 100, assume we mean the current
       century (and are not revising some roman emperor's books) */
    if (iyear < 100)
        iyear += ((int) ((now_year + 50 - iyear) / 100)) * 100;

    /* Fix up any goofy dates */
    struct tm tm{};
    tm.tm_year = iyear - 1900;
    tm.tm_mon = imonth - 1;
    tm.tm_mday = iday;
    normalize_struct_tm(&tm);

    if (year) *year = tm.tm_year + 1900;
    if (month) *month = tm.tm_mon + 1;
    if (day) *day = tm.tm_mday;
    return(TRUE);
}

gboolean
qof_scan_date (const char *buff, int *day, int *month, int *year)
{
    return qof_scan_date_internal(buff, day, month, year, dateFormat);
}

/* Return the field separator for the current date format
return date character
*/
char dateSeparator (void)
{
    static char locale_separator = '\0';

    switch (dateFormat)
    {
    case QOF_DATE_FORMAT_CE:
        return '.';
    case QOF_DATE_FORMAT_ISO:
    case QOF_DATE_FORMAT_UTC:
        return '-';
    case QOF_DATE_FORMAT_US:
    case QOF_DATE_FORMAT_UK:
    default:
        return '/';
    case QOF_DATE_FORMAT_LOCALE:
        if (locale_separator != '\0')
            return locale_separator;
        else
        {
            /* Make a guess */
            gchar string[256];
            struct tm tm;
            time64 secs;
            gchar *s;

            secs = gnc_time (nullptr);
            gnc_localtime_r(&secs, &tm);
            auto normalized_fmt =
                normalize_format(qof_date_format_get_string(dateFormat));
            qof_strftime(string, sizeof(string), normalized_fmt.c_str(), &tm);

            for (s = string; *s != '\0'; s++)
                if (!isdigit(*s))
                    return (locale_separator = *s);
        }
        break;
    }
    return '\0';
}

/* The following functions have Win32 forms in qof-win32.c */
#ifndef G_OS_WIN32
gchar *
qof_time_format_from_utf8(const gchar *utf8_format)
{
    gchar *retval;
    GError *error = nullptr;

    retval = g_locale_from_utf8(utf8_format, -1, nullptr, nullptr, &error);

    if (!retval)
    {
        g_warning("Could not convert format '%s' from UTF-8: %s", utf8_format,
                  error->message);
        g_error_free(error);
    }
    return retval;
}

gchar *
qof_formatted_time_to_utf8(const gchar *locale_string)
{
    gchar *retval;
    GError *error = nullptr;

    retval = g_locale_to_utf8(locale_string, -1, nullptr, nullptr, &error);

    if (!retval)
    {
        g_warning("Could not convert '%s' to UTF-8: %s", locale_string,
                  error->message);
        g_error_free(error);
    }
    return retval;
}
#endif /* G_OS_WIN32 */

static gchar *
qof_format_time(const gchar *format, const struct tm *tm)
{
    gchar *locale_format, *tmpbuf, *retval;
    gsize tmplen, tmpbufsize;

    g_return_val_if_fail(format, 0);
    g_return_val_if_fail(tm, 0);

    locale_format = qof_time_format_from_utf8(format);
    if (!locale_format)
        return nullptr;

    tmpbufsize = MAX(128, strlen(locale_format) * 2);
    while (TRUE)
    {
        tmpbuf = static_cast<gchar*>(g_malloc(tmpbufsize));

        /* Set the first byte to something other than '\0', to be able to
         * recognize whether strftime actually failed or just returned "".
         */
        tmpbuf[0] = '\1';
        tmplen = strftime(tmpbuf, tmpbufsize, locale_format, tm);

        if (tmplen == 0 && tmpbuf[0] != '\0')
        {
            g_free(tmpbuf);
            tmpbufsize *= 2;

            if (tmpbufsize > 65536)
            {
                g_warning("Maximum buffer size for qof_format_time "
                          "exceeded: giving up");
                g_free(locale_format);

                return nullptr;
            }
        }
        else
        {
            break;
        }
    }
    g_free(locale_format);

    retval = qof_formatted_time_to_utf8(tmpbuf);
    g_free(tmpbuf);

    return retval;
}

gsize
qof_strftime(gchar *buf, gsize max, const gchar *format, const struct tm *tm)
{
    gsize convlen, retval;
    gchar *convbuf;

    g_return_val_if_fail(buf, 0);
    g_return_val_if_fail(max > 0, 0);
    g_return_val_if_fail(format, 0);
    g_return_val_if_fail(tm, 0);

    convbuf = qof_format_time(format, tm);
    if (!convbuf)
    {
        buf[0] = '\0';
        return 0;
    }

    convlen = strlen(convbuf);

    if (max <= convlen)
    {
        /* Ensure only whole characters are copied into the buffer. */
        gchar *end = g_utf8_find_prev_char(convbuf, convbuf + max);
        g_assert(end != nullptr);
        convlen = end - convbuf;

        /* Return 0 because the buffer isn't large enough. */
        retval = 0;
    }
    else
    {
        retval = convlen;
    }

    memcpy(buf, convbuf, convlen);
    buf[convlen] = '\0';
    g_free(convbuf);

    return retval;
}

/********************************************************************\
\********************************************************************/

gchar *
gnc_date_timestamp (void)
{
    auto timestamp = GncDateTime::timestamp();
    return g_strdup(timestamp.c_str());
}

/********************************************************************\
 * iso 8601 datetimes should look like 1998-07-02 11:00:00.68-05
\********************************************************************/
/* Unfortunately, not all strptime or struct tm implementations
 * support timezones, so we have to do this with sscanf.
 */

#define ISO_DATE_FORMAT "%d-%d-%d %d:%d:%lf%s"
time64
gnc_iso8601_to_time64_gmt(const char *cstr)
{
    if (!cstr) return INT64_MAX;
    try
    {
        GncDateTime gncdt(cstr);
        return static_cast<time64>(gncdt);
    }
    catch(std::logic_error& err)
    {
        PWARN("Error processing %s: %s", cstr, err.what());
        return INT64_MAX;
    }
    catch(std::runtime_error& err)
    {
        PWARN("Error processing time64 %s: %s", cstr, err.what());
        return INT64_MAX;
    }
}

/********************************************************************\
\********************************************************************/

char *
gnc_time64_to_iso8601_buff (time64 time, char * buff)
{
    if (!buff) return nullptr;
    try
    {
        GncDateTime gncdt(time);
        auto sstr = gncdt.format_iso8601();

        memset(buff, 0, sstr.length() + 1);
        strncpy(buff, sstr.c_str(), sstr.length());
        return buff + sstr.length();
    }
    catch(std::logic_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", time, err.what());
        return buff;
    }
    catch(std::runtime_error& err)
    {
        PWARN("Error processing time64 %" PRId64 ": %s", time, err.what());
        return buff;
    }
}

#define THIRTY_TWO_YEARS 0x3c30fc00LL

static time64
gnc_dmy2time64_internal (int day, int month, int year, DayPart day_part)
{
    try
    {
        auto date = GncDate(year, month, day);
        return static_cast<time64>(GncDateTime (date, day_part));
    }
    catch(const std::logic_error& err)
    {
        PWARN("Date computation error from Y-M-D %d-%d-%d: %s",
              year, month, day, err.what());
        return INT64_MAX;
    }
    catch(const std::runtime_error& err)
    {
        PWARN("Date computation error from Y-M-D %d-%d-%d: %s",
              year, month, day, err.what());
        return INT64_MAX;
    }
}

time64
gnc_dmy2time64 (int day, int month, int year)
{
    return gnc_dmy2time64_internal (day, month, year, DayPart::start);
}

time64
gnc_dmy2time64_end (int day, int month, int year)
{
    return gnc_dmy2time64_internal (day, month, year, DayPart::end);
}

time64
gnc_dmy2time64_neutral (int day, int month, int year)
{
    return gnc_dmy2time64_internal (day, month, year, DayPart::neutral);
}


/* The GDate setter functions all in the end use g_date_set_time_t,
 * which in turn relies on localtime and is therefore subject to the
 * 2038 bug.
 */
GDate time64_to_gdate (time64 t)
{
    GDate result;

    g_date_clear (&result, 1);
    GncDateTime time(t);
    auto date = time.date().year_month_day();
    g_date_set_dmy (&result, date.day, static_cast<GDateMonth>(date.month),
                    date.year);
    g_assert(g_date_valid (&result));

    return result;
}

GDate* gnc_g_date_new_today ()
{
    GncDate gncd;
    auto ymd = gncd.year_month_day();
    auto month = static_cast<GDateMonth>(ymd.month);
    auto result = g_date_new_dmy (ymd.day, month, ymd.year);
    g_assert(g_date_valid (result));
    return result;
}

void
gnc_gdate_set_today (GDate* gd)
{
    GDate *today = gnc_g_date_new_today ();
    g_date_set_julian (gd, g_date_get_julian (today));
    g_date_free (today);
}

void
gnc_gdate_set_time64 (GDate* gd, time64 time)
{
    struct tm tm;
    gnc_localtime_r(&time, &tm);
    g_date_set_dmy (gd, tm.tm_mday,
                    static_cast<GDateMonth>(tm.tm_mon + 1),
                    tm.tm_year + 1900);
}

time64 gdate_to_time64 (GDate d)
{
    return gnc_dmy2time64_neutral (g_date_get_day(&d),
                                     g_date_get_month(&d),
                                     g_date_get_year(&d));
}

static void
gnc_tm_get_day_start (struct tm *tm, time64 time_val)
{
    /* Get the equivalent time structure */
    if (!gnc_localtime_r(&time_val, tm))
        return;
    gnc_tm_set_day_start(tm);
}

void
gnc_tm_set_day_neutral (struct tm *tm)
{
    auto time_val{gnc_dmy2time64_internal(tm->tm_mday, tm->tm_mon + 1,
                                          tm->tm_year + 1900, DayPart::neutral)};
    gnc_localtime_r(&time_val, tm);
}

static void
gnc_tm_get_day_neutral (struct tm *tm, time64 time_val)
{
    /* Get the equivalent time structure */
    if (!gnc_localtime_r(&time_val, tm))
        return;
    gnc_tm_set_day_neutral(tm);
}

static void
gnc_tm_get_day_end (struct tm *tm, time64 time_val)
{
    /* Get the equivalent time structure */
    if (!gnc_localtime_r(&time_val, tm))
        return;
    gnc_tm_set_day_end(tm);
}

time64
gnc_time64_get_day_start (time64 time_val)
{
    struct tm tm;
    time64 new_time;

    gnc_tm_get_day_start(&tm, time_val);
    new_time = gnc_mktime(&tm);
    return new_time;
}

time64
gnc_time64_get_day_neutral (time64 time_val)
{
    struct tm tm;
    gnc_localtime_r(&time_val, &tm);
    return gnc_dmy2time64_internal(tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                                   DayPart::neutral);
}

time64
gnc_time64_get_day_end (time64 time_val)
{
    struct tm tm;
    time64 new_time;

    gnc_tm_get_day_end(&tm, time_val);
    new_time = gnc_mktime(&tm);
    return new_time;
}

/* ======================================================== */

void
gnc_tm_get_today_start (struct tm *tm)
{
    gnc_tm_get_day_start(tm, gnc_time(nullptr));
}

void
gnc_tm_get_today_neutral (struct tm *tm)
{
    gnc_tm_get_day_neutral(tm, gnc_time(nullptr));
}

void
gnc_tm_get_today_end (struct tm *tm)
{
    gnc_tm_get_day_end(tm, gnc_time(nullptr));
}

time64
gnc_time64_get_today_start (void)
{
    struct tm tm;

    gnc_tm_get_day_start(&tm, gnc_time(nullptr));
    return gnc_mktime(&tm);
}

time64
gnc_time64_get_today_end (void)
{
    struct tm tm;

    gnc_tm_get_day_end(&tm, gnc_time(nullptr));
    return gnc_mktime(&tm);
}

void
gnc_dow_abbrev(gchar *buf, int buf_len, int dow)
{
    struct tm my_tm;
    int i;

    memset(buf, 0, buf_len);
    memset(&my_tm, 0, sizeof(struct tm));
    my_tm.tm_wday = dow;
    i = qof_strftime(buf, buf_len, "%a", &my_tm);
    buf[i] = 0;
}

/* *******************************************************************
 *  GValue handling
 ********************************************************************/

static Time64*
time64_boxed_copy_func (Time64 *in_time64)
{
    Time64* newvalue = g_new (Time64, 1);
    *newvalue = *in_time64;

    return newvalue;
}

static void
time64_boxed_free_func (Time64 *in_time64)
{
    g_free (in_time64);
}

G_DEFINE_BOXED_TYPE (Time64, time64, time64_boxed_copy_func, time64_boxed_free_func)

/* ================================================= */

gboolean
gnc_gdate_equal(gconstpointer gda, gconstpointer gdb)
{
    return (g_date_compare( (GDate*)gda, (GDate*)gdb ) == 0 ? TRUE : FALSE);
}

guint
gnc_gdate_hash( gconstpointer gd )
{
    gint val = (g_date_get_year( (GDate*)gd ) * 10000)
               + (g_date_get_month( (GDate*)gd ) * 100)
               + g_date_get_day( (GDate*)gd );
    return g_int_hash( &val );
}

/* ================================================= */

time64
gnc_time64_get_day_start_gdate (const GDate *date)
{
    struct tm stm;
    time64 secs;

    /* First convert to a 'struct tm' */
    g_date_to_struct_tm (date, &stm);

    /* Then convert to number of seconds */
    secs = gnc_mktime (&stm);
    return secs;
}

time64
gnc_time64_get_day_end_gdate (const GDate *date)
{
    struct tm stm;
    time64 secs;

    /* First convert to a 'struct tm' */
    g_date_to_struct_tm(date, &stm);

    /* Force to th last second of the day */
    stm.tm_hour = 23;
    stm.tm_min = 59;
    stm.tm_sec = 59;
    stm.tm_isdst = -1;

    /* Then convert to number of seconds */
    secs = gnc_mktime (&stm);
    return secs;
}

/* ================================================= */

void
gnc_gdate_set_month_start (GDate *date)
{
    g_date_set_day(date, 1);
}

/** Convert a GDate to the last day of the month.  This routine has no
 *  knowledge of how many days are in a month, whether its a leap
 *  year, etc.  All that information is contained in the glib date
 *  functions.
 *
 *  @param date The GDate to modify.
 */
void
gnc_gdate_set_month_end (GDate *date)
{
    /* First set the start of next month. */
    g_date_set_day(date, 1);
    g_date_add_months(date, 1);

    /* Then back up one day */
    g_date_subtract_days(date, 1);
}

/** Convert a GDate to the first day of the prebvious month.  This
 *  routine has no knowledge of how many days are in a month, whether
 *  its a leap year, etc.  All that information is contained in the
 *  glib date functions.
 *
 *  @param date The GDate to modify.
 */
void
gnc_gdate_set_prev_month_start (GDate *date)
{
    g_date_set_day(date, 1);
    g_date_subtract_months(date, 1);
}

/** Convert a GDate to the last day of the prebvious month.  This
 *  routine has no knowledge of how many days are in a month, whether
 *  its a leap year, etc.  All that information is contained in the
 *  glib date functions.
 *
 *  @param date The GDate to modify.
 */
void
gnc_gdate_set_prev_month_end (GDate *date)
{
    /* This will correctly handle the varying month lengths */
    g_date_set_day(date, 1);
    g_date_subtract_days(date, 1);
}

/* ================================================= */

void
gnc_gdate_set_quarter_start (GDate *date)
{
    gint months;

    /* Set the date to the first day of the specified month. */
    g_date_set_day(date, 1);

    /* Back up 0-2 months */
    months = (g_date_get_month(date) - G_DATE_JANUARY) % 3;
    g_date_subtract_months(date, months);
}

void
gnc_gdate_set_quarter_end (GDate *date)
{
     const GDateMonth months[] = {G_DATE_MARCH, G_DATE_JUNE,
                                  G_DATE_SEPTEMBER, G_DATE_DECEMBER};
     const GDateDay days[] = {31, 30, 30, 31};
     int quarter = (g_date_get_month (date) - 1) / 3;

     g_date_set_month (date, months[quarter]);
     g_date_set_day (date, days[quarter]);
}

void
gnc_gdate_set_prev_quarter_start (GDate *date)
{
    g_date_subtract_months(date, 3);
    gnc_gdate_set_quarter_start(date);
}

void
gnc_gdate_set_prev_quarter_end (GDate *date)
{
    g_date_subtract_months(date, 3);
    gnc_gdate_set_quarter_end(date);
}

/* ================================================= */

void
gnc_gdate_set_year_start (GDate *date)
{
    g_date_set_month(date, G_DATE_JANUARY);
    g_date_set_day(date, 1);
}

void
gnc_gdate_set_year_end (GDate *date)
{
    g_date_set_month(date, G_DATE_DECEMBER);
    g_date_set_day(date, 31);
}

void
gnc_gdate_set_prev_year_start (GDate *date)
{
    gnc_gdate_set_year_start(date);
    g_date_subtract_years(date, 1);
}

void
gnc_gdate_set_prev_year_end (GDate *date)
{
    gnc_gdate_set_year_end(date);
    g_date_subtract_years(date, 1);
}

/* ================================================= */

void
gnc_gdate_set_fiscal_year_start (GDate *date,
                                 const GDate *fy_end)
{
    GDate temp;
    gboolean new_fy;

    g_return_if_fail(date);
    g_return_if_fail(fy_end);

    /* Compute the FY end that occurred this CY */
    temp = *fy_end;
    g_date_set_year(&temp, g_date_get_year(date));

    /* Has it already passed? */
    new_fy = (g_date_compare(date, &temp) > 0);

    /* Set start date */
    *date = temp;
    g_date_add_days(date, 1);
    if (!new_fy)
        g_date_subtract_years(date, 1);
}

void
gnc_gdate_set_fiscal_year_end (GDate *date,
                               const GDate *fy_end)
{
    GDate temp;
    gboolean new_fy;

    g_return_if_fail(date);
    g_return_if_fail(fy_end);

    /* Compute the FY end that occurred this CY */
    temp = *fy_end;
    g_date_set_year(&temp, g_date_get_year(date));

    /* Has it already passed? */
    new_fy = (g_date_compare(date, &temp) > 0);

    /* Set end date */
    *date = temp;
    if (new_fy)
        g_date_add_years(date, 1);
}

void
gnc_gdate_set_prev_fiscal_year_start (GDate *date,
                                      const GDate *fy_end)
{
    g_return_if_fail(date);
    g_return_if_fail(fy_end);

    gnc_gdate_set_fiscal_year_start(date, fy_end);
    g_date_subtract_years(date, 1);
}

void
gnc_gdate_set_prev_fiscal_year_end (GDate *date,
                                    const GDate *fy_end)
{
    g_return_if_fail(date);
    g_return_if_fail(fy_end);

    gnc_gdate_set_fiscal_year_end(date, fy_end);
    g_date_subtract_years(date, 1);
}

Testfuncs*
gnc_date_load_funcs (void)
{
    Testfuncs *tf = g_slice_new (Testfuncs);
    return tf;
}
