/* Copyright (C) 2005 J.F.Dockes 
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

#include <QtXml/QXmlDefaultHandler>

#include "ssearch_w.h"

#include "guiutils.h"
#include "log.h"
#include "xmltosd.h"
#include "smallut.h"
#include "recoll.h"

using namespace std;
using namespace Rcl;

class SDHXMLHandler : public QXmlDefaultHandler {
public:
    SDHXMLHandler()
        : isvalid(false)
    {
	resetTemps();
    }
    bool startElement(const QString & /* namespaceURI */,
		      const QString & /* localName */,
		      const QString &qName,
		      const QXmlAttributes &attributes);
    bool endElement(const QString & /* namespaceURI */,
		    const QString & /* localName */,
		    const QString &qName);
    bool characters(const QString &str)
    {
	currentText += str;
	return true;
    }

    // The object we set up
    std::shared_ptr<SearchData> sd;
    bool isvalid;

private:
    void resetTemps() 
    {
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
    QString currentText;
    QString whatclause;
    string field, text, text2;
    int slack;
    int d, m, y;
    DateInterval di;
    bool hasdates;
    bool exclude;
};

bool SDHXMLHandler::startElement(const QString & /* namespaceURI */,
				const QString & /* localName */,
				const QString &qName,
				const QXmlAttributes &attrs)
{
    LOGDEB2("SDHXMLHandler::startElement: name ["  << qs2utf8s(qName) << "]\n");
    if (qName == "SD") {
        // Advanced search history entries have no type. So we're good
        // either if type is absent, or if it's searchdata
        int idx = attrs.index("type");
        if (idx >= 0 && attrs.value(idx).compare("searchdata")) {
            LOGDEB("XMLTOSD: bad type: " << qs2utf8s(attrs.value(idx)) << endl);
	    return false;
	}
	resetTemps();
	// A new search descriptor. Allocate data structure
        sd = std::shared_ptr<SearchData>(new SearchData);
	if (!sd) {
	    LOGERR("SDHXMLHandler::startElement: out of memory\n");
	    return false;
	}
    }	
    return true;
}

bool SDHXMLHandler::endElement(const QString & /* namespaceURI */,
                               const QString & /* localName */,
                               const QString &qName)
{
    LOGDEB2("SDHXMLHandler::endElement: name ["  << qs2utf8s(qName) << "]\n");

    if (qName == "CLT") {
	if (currentText == "OR") {
	    sd->setTp(SCLT_OR);
	}
    } else if (qName == "CT") {
	whatclause = currentText.trimmed();
    } else if (qName == "NEG") {
	exclude = true;
    } else if (qName == "F") {
	field = base64_decode(qs2utf8s(currentText.trimmed()));
    } else if (qName == "T") {
	text = base64_decode(qs2utf8s(currentText.trimmed()));
    } else if (qName == "T2") {
	text2 = base64_decode(qs2utf8s(currentText.trimmed()));
    } else if (qName == "S") {
	slack = atoi((const char *)currentText.toUtf8());
    } else if (qName == "C") {
	SearchDataClause *c;
	if (whatclause == "AND" || whatclause.isEmpty()) {
	    c = new SearchDataClauseSimple(SCLT_AND, text, field);
	    c->setexclude(exclude);
	} else if (whatclause == "OR") {
	    c = new SearchDataClauseSimple(SCLT_OR, text, field);
	    c->setexclude(exclude);
	} else if (whatclause == "RG") {
	    c = new SearchDataClauseRange(text, text2, field);
	    c->setexclude(exclude);
	} else if (whatclause == "EX") {
	    // Compat with old hist. We don't generete EX (SCLT_EXCL) anymore
	    // it's replaced with OR + exclude flag
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
	    LOGERR("Bad clause type ["  << qs2utf8s(whatclause) << "]\n");
	    return false;
	}
	sd->addClause(c);
	whatclause = "";
	text.clear();
	field.clear();
	slack = 0;
	exclude = false;
    } else if (qName == "D") {
	d = atoi((const char *)currentText.toUtf8());
    } else if (qName == "M") {
	m = atoi((const char *)currentText.toUtf8());
    } else if (qName == "Y") {
	y = atoi((const char *)currentText.toUtf8());
    } else if (qName == "DMI") {
	di.d1 = d;
	di.m1 = m;
	di.y1 = y;
	hasdates = true;
    } else if (qName == "DMA") {
	di.d2 = d;
	di.m2 = m;
	di.y2 = y;
	hasdates = true;
    } else if (qName == "MIS") {
	sd->setMinSize(atoll((const char *)currentText.toUtf8()));
    } else if (qName == "MAS") {
	sd->setMaxSize(atoll((const char *)currentText.toUtf8()));
    } else if (qName == "ST") {
	string types = (const char *)currentText.toUtf8();
	vector<string> vt;
	stringToTokens(types, vt);
	for (unsigned int i = 0; i < vt.size(); i++) 
	    sd->addFiletype(vt[i]);
    } else if (qName == "IT") {
	string types(qs2utf8s(currentText));
	vector<string> vt;
	stringToTokens(types, vt);
	for (unsigned int i = 0; i < vt.size(); i++) 
	    sd->remFiletype(vt[i]);
    } else if (qName == "YD") {
	string d;
	base64_decode(qs2utf8s(currentText.trimmed()), d);
	sd->addClause(new SearchDataClausePath(d));
    } else if (qName == "ND") {
	string d;
	base64_decode(qs2utf8s(currentText.trimmed()), d);
	sd->addClause(new SearchDataClausePath(d, true));
    } else if (qName == "SD") {
	// Closing current search descriptor. Finishing touches...
	if (hasdates)
	    sd->setDateSpan(&di);
	resetTemps();
        isvalid = true;
    } 
    currentText.clear();
    return true;
}


std::shared_ptr<Rcl::SearchData> xmlToSearchData(const string& xml,
                                                 bool verbose)
{
    SDHXMLHandler handler;
    QXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);

    QXmlInputSource xmlInputSource;
    xmlInputSource.setData(QString::fromUtf8(xml.c_str()));

    if (!reader.parse(xmlInputSource) || !handler.isvalid) {
        if (verbose) {
            LOGERR("xmlToSearchData: parse failed for ["  << xml << "]\n");
        }
        return std::shared_ptr<SearchData>();
    }
    return handler.sd;
}


// Handler for parsing saved simple search data
class SSHXMLHandler : public QXmlDefaultHandler {
public:
    SSHXMLHandler()
        : isvalid(false)
        {
            resetTemps();
        }
    bool startElement(const QString & /* namespaceURI */,
		      const QString & /* localName */,
		      const QString &qName,
		      const QXmlAttributes &attributes);
    bool endElement(const QString & /* namespaceURI */,
		    const QString & /* localName */,
		    const QString &qName);
    bool characters(const QString &str)
        {
            currentText += str;
            return true;
        }

    // The object we set up
    SSearchDef data;
    bool isvalid;

private:
    void resetTemps() 
        {
            currentText = whatclause = "";
            text.clear();
        }

    // Temporary data while parsing.
    QString currentText;
    QString whatclause;
    string text;
};

bool SSHXMLHandler::startElement(const QString & /* namespaceURI */,
                                 const QString & /* localName */,
                                 const QString &qName,
                                 const QXmlAttributes &attrs)
{
    LOGDEB2("SSHXMLHandler::startElement: name ["  << u8s2qs(qName) << "]\n");
    if (qName == "SD") {
        // Simple search saved data has a type='ssearch' attribute.
        int idx = attrs.index("type");
        if (idx < 0 || attrs.value(idx).compare("ssearch")) {
            if (idx < 0) {
                LOGDEB("XMLTOSSS: bad type\n");
            } else {
                LOGDEB("XMLTOSSS: bad type: " << qs2utf8s(attrs.value(idx))
                       << endl);
            }
            return false;
        }
	resetTemps();
    }	
    return true;
}

bool SSHXMLHandler::endElement(const QString & /* namespaceURI */,
                               const QString & /* localName */,
                               const QString &qName)
{
    LOGDEB2("SSHXMLHandler::endElement: name ["  << u8s2qs(qName) << "]\n");

    currentText = currentText.trimmed();

    if (qName == "SL") {
        stringToStrings(qs2utf8s(currentText), data.stemlangs);
    } else if (qName == "T") {
        base64_decode(qs2utf8s(currentText), data.text);
    } else if (qName == "EX") {
        data.extindexes.push_back(base64_decode(qs2utf8s(currentText)));
    } else if (qName == "SM") {
        if (!currentText.compare("QL")) {
            data.mode = SSearch::SST_LANG;
        } else if (!currentText.compare("FN")) {
            data.mode = SSearch::SST_FNM;
        } else if (!currentText.compare("OR")) {
            data.mode = SSearch::SST_ANY;
        } else if (!currentText.compare("AND")) {
            data.mode = SSearch::SST_ALL;
        } else {
            LOGERR("BAD SEARCH MODE: [" << qs2utf8s(currentText) << "]\n");
            return false;
        }
    } else if (qName == "AS") {
        stringToStrings(qs2utf8s(currentText), data.autosuffs);
    } else if (qName == "AP") {
        data.autophrase = true;
    } else if (qName == "SD") {
	// Closing current search descriptor. Finishing touches...
	resetTemps();
        isvalid = true;
    } 
    currentText.clear();
    return true;
}

bool xmlToSSearch(const string& xml, SSearchDef& data)
{
    SSHXMLHandler handler;
    QXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);

    QXmlInputSource xmlInputSource;
    xmlInputSource.setData(QString::fromUtf8(xml.c_str()));

    if (!reader.parse(xmlInputSource) || !handler.isvalid) {
        LOGERR("xmlToSSearch: parse failed for ["  << xml << "]\n");
        return false;
    }
    data = handler.data;
    return true;
}

