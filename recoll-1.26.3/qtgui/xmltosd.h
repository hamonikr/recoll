/* Copyright (C) 2014 J.F.Dockes
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
#ifndef XMLTOSD_H_INCLUDED
#define XMLTOSD_H_INCLUDED
#include "autoconfig.h"

/** Parsing XML from saved queries or advanced search history.
 *
 * Here is how the schemas looks like:
 *
 * For advanced search
 *
 * <SD>                         <!-- Search Data -->
 *  <CL>                        <!-- Clause List -->
 *    <CLT>AND|OR</CLT>         <!-- conjunction AND is default, ommitted -->
 *    <C>                       <!-- Clause -->
 *     [<NEG/>]                 <!-- Possible negation -->
 *     <CT>AND|OR|FN|PH|NE</CT> <!-- Clause type -->
 *     <F>[base64data]</F>      <!-- Optional base64-encoded field name -->
 *     <T>[base64data]</T>      <!-- base64-encoded text -->
 *     <S>slack</S>             <!-- optional slack for near/phrase -->
 *    </C>
 *
 *    <ND>[base64 path]</ND>    <!-- Path exclusion -->
 *    <YD>[base64 path]</YD>    <!-- Path filter -->
 *  </CL>
 * 
 *  <DMI><D>1</D><M>6</M><Y>2014</Y></DMI> <--! datemin -->
 *  <DMA><D>30</D><M>6</M><Y>2014</Y></DMA> <--! datemax -->
 *  <MIS>minsize</MIS>          <!-- Min size -->
 *  <MAS>maxsize</MAS>          <!-- Max size -->
 *  <ST>space-sep mtypes</ST>   <!-- Included mime types -->
 *  <IT>space-sep mtypes</IT>   <!-- Excluded mime types -->
 *
 * </SD>
 *
 * For Simple search:
 *
 * <SD type='ssearch'>
 *   <T>base64-encoded query text</T>
 *   <SM>OR|AND|FN|QL</SM>
 *   <SL>space-separated lang list</SL>
 *   <AP/>                                   <!-- autophrase -->
 *   <AS>space-separated suffix list</AS>    <!-- autosuffs -->
 *   <EX>base64-encoded config path>/EX>     <!-- [multiple] ext index -->
 * </SD>
 */ 

#include <memory>
#include "searchdata.h"

// Parsing XML from advanced search history or saved advanced search into
// a SearchData structure:
std::shared_ptr<Rcl::SearchData> xmlToSearchData(const string& xml,
                                                 bool complain = true);

// Parsing XML from saved simple search to ssearch parameters
struct SSearchDef {
    SSearchDef() : autophrase(false), mode(0) {}
    std::vector<std::string> stemlangs;
    std::vector<std::string> autosuffs;
    std::vector<std::string> extindexes;
    std::string text;
    bool autophrase;
    int mode;
};
bool xmlToSSearch(const string& xml, SSearchDef&);
#endif /* XMLTOSD_H_INCLUDED */
