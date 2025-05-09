/********************************************************************
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
 * along with this program; if not, you can retrieve it from        *
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.html            *
 * or contact:                                                      *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 ********************************************************************/

// #include "config.h"
#include "csv-export-helpers.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop


#include <sstream>

#ifdef G_OS_WIN32
# define EOLSTR "\n"
#else
# define EOLSTR "\r\n"
#endif

TEST (CsvHelperTest, EmptyTests)
{
    std::ostringstream ss;
    gnc_csv_add_line (ss, {}, false, nullptr);
    ASSERT_EQ (ss.str(), EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, {}, true, ",");
    ASSERT_EQ (ss.str(), EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, {}, false, ",");
    ASSERT_EQ (ss.str(), EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, {}, true, nullptr);
    ASSERT_EQ (ss.str(), EOLSTR);
}

TEST (CsvHelperTest, BasicTests)
{
    std::ostringstream ss;
    gnc_csv_add_line (ss, { "A","B","C","","D" }, false, ",");
    ASSERT_EQ (ss.str(), "A,B,C,,D" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","","D" }, false, "");
    ASSERT_EQ (ss.str(), "ABCD" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","","D" }, false, nullptr);
    ASSERT_EQ (ss.str(), "ABCD" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","","D" }, false, ";");
    ASSERT_EQ (ss.str(), "A;B;C;;D" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","","D" }, true, ",");
    ASSERT_EQ (ss.str(), "\"A\",\"B\",\"C\",\"\",\"D\"" EOLSTR);

}


TEST (CsvHelperTest, ForcedQuote)
{
    std::ostringstream ss;
    gnc_csv_add_line (ss, { "A","B","C","\"","D" }, false, ",");
    ASSERT_EQ (ss.str(), "A,B,C,\"\"\"\",D" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","",",D" }, false, ",");
    ASSERT_EQ (ss.str(), "A,B,C,,\",D\"" EOLSTR);

    std::ostringstream().swap(ss);
    gnc_csv_add_line (ss, { "A","B","C","\n","D\r" }, false, ";");
    ASSERT_EQ (ss.str(), "A;B;C;\"\n\";\"D\r\"" EOLSTR);

}
