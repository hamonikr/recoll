/* Copyright (C) 2004-2018 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"

#include <string>

#include "xapian.h"

#include "rclconfig.h"
#include "smallut.h"
#include "log.h"
#include "unacpp.h"

using namespace std;

namespace Rcl {

void add_field_value(Xapian::Document& xdoc, const FieldTraits& ft,
                     const string& data)
{
    string ndata;

    switch (ft.valuetype) {
    case FieldTraits::STR:
        if (o_index_stripchars) {
            if (!unacmaybefold(data, ndata, "UTF-8", UNACOP_UNACFOLD)) {
                LOGDEB("Rcl::add_field_value: unac failed for ["<<data<< "]\n");
                ndata = data;
            } 
        } else {
            ndata = data;
        }
        break;
    case FieldTraits::INT:
    {
        ndata = data;
        int len = ft.valuelen ? ft.valuelen : 10;
        leftzeropad(ndata, len);
    }
    }
    LOGDEB0("Rcl::add_field_value: slot " << ft.valueslot << " [" <<
            ndata << "]\n");
    xdoc.add_value(ft.valueslot, ndata);
}


string convert_field_value(const FieldTraits& ft,
                                       const string& data)
{
    string ndata(data);
    switch (ft.valuetype) {
    case FieldTraits::STR:
        break;
    case FieldTraits::INT:
    {
        if (ndata.empty())
            break;
        
        // Apply suffixes
        char c = ndata.back();
        string zeroes;
        switch(c) {
        case 'k':case 'K': zeroes = "000";break;
        case 'm':case 'M': zeroes = "000000";break;
        case 'g':case 'G': zeroes = "000000000";break;
        case 't':case 'T': zeroes = "000000000000";break;
        default: break;
        }
        if (!zeroes.empty()) {
            ndata.pop_back();
            ndata += zeroes;
        }
        int len = ft.valuelen ? ft.valuelen : 10;
        leftzeropad(ndata, len);
    }
    }

    return ndata;
}

}


