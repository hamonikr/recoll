/* Copyright (C) 2005 Jean-Francois Dockes 
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
#ifndef _GUIUTILS_H_INCLUDED_
#define _GUIUTILS_H_INCLUDED_

#include <string>
#include <list>
#include <vector>

#include <qstring.h>
#include <qstringlist.h>

#include "dynconf.h"
extern RclDynConf *g_dynconf;

#include "advshist.h"
extern AdvSearchHist *g_advshistory;

using std::string;
using std::list;
using std::vector;

/** Holder for preferences (gets saved to user Qt prefs) */
class PrefsPack {
 public:
    // Simple search entry behaviour
    bool ssearchNoComplete;
    bool ssearchStartOnComplete;
    // Decide if we display the doc category filter control as a
    // toolbar+combobox or as a button group under simple search
    enum FilterCtlStyle {FCS_BT, FCS_CMB, FCS_MN};
    int filterCtlStyle;
    int respagesize{8};
    int historysize{0};
    int maxhltextmbs;
    QString reslistfontfamily;
    // Not saved in prefs for now. Computed from qt defaults and used to
    // set main character color for webkit/textbrowser reslist and
    // snippets window.
    QString fontcolor; 
    QString qtermstyle; // CSS style for query terms in reslist and other places
    int reslistfontsize;
    // Result list format string
    QString reslistformat;
    string  creslistformat;
    QString reslistheadertext;
    // Date strftime format
    QString reslistdateformat;
    string creslistdateformat;
    QString qssFile;
    QString snipCssFile;
    QString queryStemLang;
    int mainwidth;
    int mainheight;
    enum ShowMode {SHOW_NORMAL, SHOW_MAX, SHOW_FULL};
    int showmode{SHOW_NORMAL};
    int pvwidth; // Preview window geom
    int pvheight;
    int toolArea; // Area for "tools" toolbar
    int resArea; // Area for "results" toolbar
    bool ssearchTypSav; // Remember last search mode (else always
			// start with same)
    int ssearchTyp{0};
    // Use single app (default: xdg-open), instead of per-mime settings
    bool useDesktopOpen; 
    // Remember sort state between invocations ?
    bool keepSort;   
    QString sortField;
    bool sortActive; 
    bool sortDesc; 
    // Abstract preferences. Building abstracts can slow result display
    bool queryBuildAbstract{true};
    bool queryReplaceAbstract{false};
    // Synthetized abstract length (chars) and word context size (words)
    int syntAbsLen;
    int syntAbsCtx;
    // Abstract snippet separator
    QString abssep;
    // Snippets window max list size
    int snipwMaxLength;
    // Snippets window sort by page (dflt: by weight)
    bool snipwSortByPage;
    bool startWithAdvSearchOpen{false};
    // Try to display html if it exists in the internfile stack.
    bool previewHtml;
    bool previewActiveLinks;
    // Use <pre> tag to display highlighted text/plain inside html (else
    // we use <br> at end of lines, which lets textedit wrap lines).
    enum PlainPre {PP_BR, PP_PRE, PP_PREWRAP};
    int  previewPlainPre; 
    bool collapseDuplicates;
    bool showResultsAsTable;

    // Extra query indexes. This are stored in the history file, not qt prefs
    vector<string> allExtraDbs;
    vector<string> activeExtraDbs;
    // Advanced search subdir restriction: we don't activate the last value
    // but just remember previously entered values
    QStringList asearchSubdirHist;
    // Textual history of simple searches (this is just the combobox list)
    QStringList ssearchHistory;
    // Make phrase out of search terms and add to search in simple search
    bool ssearchAutoPhrase;
    double ssearchAutoPhraseThreshPC;
    // Ignored file types in adv search (startup default)
    QStringList asearchIgnFilTyps;
    bool        fileTypesByCats;
    // Words that are automatically turned to ext:xx specs in the query
    // language entry. 
    QString autoSuffs;
    bool    autoSuffsEnable;
    // Synonyms file
    QString synFile;
    bool    synFileEnable;

    QStringList restableFields;
    vector<int> restableColWidths;

    // Remembered term match mode
    int termMatchType{0};

    // Program version that wrote this. Not used for now, in prevision
    // of the case where we might need an incompatible change
    int rclVersion{1505};
    // Suppress all noises
    bool noBeeps;
    
    bool showTrayIcon{false};
    bool closeToTray{false};
    bool trayMessages{false};
    
    // See qxtconfirmationmessage. Needs to be -1 for the dialog to show
    int showTempFileWarning;
    
    // Advanced search window clause list state
    vector<int> advSearchClauses;

    // Default paragraph format for result list
    static const char *dfltResListFormat;

    std::string stemlang();

};

/** Global preferences record */
extern PrefsPack prefs;

/** Read write settings from disk file */
extern void rwSettings(bool dowrite);

extern QString g_stringAllStem, g_stringNoStem;

#endif /* _GUIUTILS_H_INCLUDED_ */
