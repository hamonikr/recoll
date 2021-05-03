/* Copyright (C) 2005-2020 J.F.Dockes 
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

#include "ssearch_w.h"

#include "guiutils.h"
#include "log.h"
#include "xmltosd.h"
#include "smallut.h"
#include "recoll.h"
#include "picoxml.h"

using namespace std;
using namespace Rcl;

class SDHXMLHandler : public PicoXMLParser {
public:
    SDHXMLHandler(const std::string& in)
        : PicoXMLParser(in) {
        resetTemps();
    }
    void startElement(
        const std::string& nm,
        const std::map<std::string, std::string>& attrs) {

        LOGDEB2("SDHXMLHandler::startElement: name [" << nm << "]\n");
        if (nm == "SD") {
            // Advanced search history entries have no type. So we're good
            // either if type is absent, or if it's searchdata
            auto attr = attrs.find("type");
            if (attr != attrs.end() && attr->second != "searchdata") {
                LOGDEB("XMLTOSD: bad type: " << attr->second << endl);
                contentsOk = false;
                return;
            }
            resetTemps();
            // A new search descriptor. Allocate data structure
            sd = std::shared_ptr<SearchData>(new SearchData);
            if (!sd) {
                LOGERR("SDHXMLHandler::startElement: out of memory\n");
                contentsOk = false;
                return;
            }
        }    
        return;
    }

    void endElement(const string & nm) {
        LOGDEB2("SDHXMLHandler::endElement: name [" << nm << "]\n");
        string curtxt{currentText};
        trimstring(curtxt, " \t\n\r");
        if (nm == "CLT") {
            if (curtxt == "OR") {
                sd->setTp(SCLT_OR);
            }
        } else if (nm == "CT") {
            whatclause = curtxt;
        } else if (nm == "NEG") {
            exclude = true;
        } else if (nm == "F") {
            field = base64_decode(curtxt);
        } else if (nm == "T") {
            text = base64_decode(curtxt);
        } else if (nm == "T2") {
            text2 = base64_decode(curtxt);
        } else if (nm == "S") {
            slack = atoi(curtxt.c_str());
        } else if (nm == "C") {
            SearchDataClause *c;
            if (whatclause == "AND" || whatclause.empty()) {
                c = new SearchDataClauseSimple(SCLT_AND, text, field);
                c->setexclude(exclude);
            } else if (whatclause == "OR") {
                c = new SearchDataClauseSimple(SCLT_OR, text, field);
                c->setexclude(exclude);
            } else if (whatclause == "RG") {
                c = new SearchDataClauseRange(text, text2, field);
                c->setexclude(exclude);
            } else if (whatclause == "EX") {
                // Compat with old hist. We don't generete EX
                // (SCLT_EXCL) anymore it's replaced with OR + exclude
                // flag
                c = new SearchDataClauseSimple(SCLT_OR, text, field);
                c->setexclude(true);
            } else if (whatclause == "FN") {
                c = new SearchDataClauseFilename(text);
                c->setexclude(exclude);
            } else if (whatclause == "PH") {
                c = new SearchDataClauseDist(SCLT_PHRASE, text, slack, field);
                c->setexclude(exclude);
            } else if (whatclause == "NE") {
                c = new SearchDataClauseDist(SCLT_NEAR, text, slack, field);
                c->setexclude(exclude);
            } else {
                LOGERR("Bad clause type ["  << whatclause << "]\n");
                contentsOk = false;
                return;
            }
            sd->addClause(c);
            whatclause = "";
            text.clear();
            field.clear();
            slack = 0;
            exclude = false;
        } else if (nm == "D") {
            d = atoi(curtxt.c_str());
        } else if (nm == "M") {
            m = atoi(curtxt.c_str());
        } else if (nm == "Y") {
            y = atoi(curtxt.c_str());
        } else if (nm == "DMI") {
            di.d1 = d;
            di.m1 = m;
            di.y1 = y;
            hasdates = true;
        } else if (nm == "DMA") {
            di.d2 = d;
            di.m2 = m;
            di.y2 = y;
            hasdates = true;
        } else if (nm == "MIS") {
            sd->setMinSize(atoll(curtxt.c_str()));
        } else if (nm == "MAS") {
            sd->setMaxSize(atoll(curtxt.c_str()));
        } else if (nm == "ST") {
            string types = curtxt.c_str();
            vector<string> vt;
            stringToTokens(types, vt);
            for (unsigned int i = 0; i < vt.size(); i++) 
                sd->addFiletype(vt[i]);
        } else if (nm == "IT") {
            vector<string> vt;
            stringToTokens(curtxt, vt);
            for (unsigned int i = 0; i < vt.size(); i++) 
                sd->remFiletype(vt[i]);
        } else if (nm == "YD") {
            string d;
            base64_decode(curtxt, d);
            sd->addClause(new SearchDataClausePath(d));
        } else if (nm == "ND") {
            string d;
            base64_decode(curtxt, d);
            sd->addClause(new SearchDataClausePath(d, true));
        } else if (nm == "SD") {
            // Closing current search descriptor. Finishing touches...
            if (hasdates)
                sd->setDateSpan(&di);
            resetTemps();
            isvalid = contentsOk;
        } 
        currentText.clear();
        return;
    }

    void characterData(const std::string &str) {
        currentText += str;
    }

    // The object we set up
    std::shared_ptr<SearchData> sd;
    bool isvalid{false};
    bool contentsOk{true};
    
private:
    void resetTemps() {
        currentText = whatclause = "";
        text.clear();
        text2.clear();
        field.clear();
        slack = 0;
        d = m = y = di.d1 = di.m1 = di.y1 = di.d2 = di.m2 = di.y2 = 0;
        hasdates = false;
        exclude = false;
    }

    // Temporary data while parsing.
    std::string currentText;
    std::string whatclause;
    std::string field, text, text2;
    int slack;
    int d, m, y;
    DateInterval di;
    bool hasdates;
    bool exclude;
};

std::shared_ptr<Rcl::SearchData> xmlToSearchData(const string& xml,
                                                 bool verbose)
{
    SDHXMLHandler handler(xml);
    if (!handler.Parse() || !handler.isvalid) {
        if (verbose) {
            LOGERR("xmlToSearchData: parse failed for ["  << xml << "]\n");
        }
        return std::shared_ptr<SearchData>();
    }
    return handler.sd;
}


// Handler for parsing saved simple search data
class SSHXMLHandler : public PicoXMLParser {
public:
    SSHXMLHandler(const std::string& in)
        : PicoXMLParser(in) {
        resetTemps();
    }

    void startElement(const std::string &nm,
                      const std::map<std::string, std::string>& attrs) override {
        LOGDEB2("SSHXMLHandler::startElement: name [" << nm << "]\n");
        if (nm == "SD") {
            // Simple search saved data has a type='ssearch' attribute.
            auto attr = attrs.find("type");
            if (attr == attrs.end() || attr->second != "ssearch") {
                if (attr == attrs.end()) {
                    LOGDEB("XMLTOSSS: bad type\n");
                } else {
                    LOGDEB("XMLTOSSS: bad type: " << attr->second << endl);
                }
                contentsOk = false;
            }
            resetTemps();
        }
    }
        
    void endElement(const string& nm) override {
        LOGDEB2("SSHXMLHandler::endElement: name ["  << nm << "]\n");
        std::string curtxt{currentText};
        trimstring(curtxt, " \t\n\r");
        if (nm == "SL") {
            stringToStrings(curtxt, data.stemlangs);
        } else if (nm == "T") {
            base64_decode(curtxt, data.text);
        } else if (nm == "EX") {
            data.extindexes.push_back(base64_decode(curtxt));
        } else if (nm == "SM") {
            if (curtxt == "QL") {
                data.mode = SSearch::SST_LANG;
            } else if (curtxt == "FN") {
                data.mode = SSearch::SST_FNM;
            } else if (curtxt == "OR") {
                data.mode = SSearch::SST_ANY;
            } else if (curtxt == "AND") {
                data.mode = SSearch::SST_ALL;
            } else {
                LOGERR("BAD SEARCH MODE: [" << curtxt << "]\n");
                contentsOk = false;
                return;
            }
        } else if (nm == "AS") {
            stringToStrings(curtxt, data.autosuffs);
        } else if (nm == "AP") {
            data.autophrase = true;
        } else if (nm == "SD") {
            // Closing current search descriptor. Finishing touches...
            resetTemps();
            isvalid = contentsOk;
        } 
        currentText.clear();
        return ;
    }

    void characterData(const std::string &str) override {
        currentText += str;
    }

    // The object we set up
    SSearchDef data;
    bool isvalid{false};
    bool contentsOk{true};
    
private:
    void resetTemps() {
        currentText = whatclause = "";
        text.clear();
    }

    // Temporary data while parsing.
    std::string currentText;
    std::string whatclause;
    string text;
};

bool xmlToSSearch(const string& xml, SSearchDef& data)
{
    SSHXMLHandler handler(xml);
    if (!handler.Parse() || !handler.isvalid) {
        LOGERR("xmlToSSearch: parse failed for ["  << xml << "]\n");
        return false;
    }
    data = handler.data;
    return true;
}

