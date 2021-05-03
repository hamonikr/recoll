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
#include <set>

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
    int maxhltextkbs;
    QString reslistfontfamily;
    int reslistfontsize;
    // Not saved in prefs for now. Computed from qt defaults and used to
    // set main character color for webkit/textbrowser reslist and
    // snippets window.
    QString fontcolor; 
    QString qtermstyle; // CSS style for query terms in reslist and other places
    // Result list format string
    QString reslistformat;
    string  creslistformat;
    QString reslistheadertext;
    // This is either empty or the contents of the recoll-dark.css
    // file if we are in dark mode. It is set in the header before the
    // possible user string above. Not saved/restored to prefs as it
    // is controled by darkMode
    QString darkreslistheadertext;
    // Date strftime format
    string reslistdateformat;

    //  General Qt style sheet.
    QString qssFile;
    // Dark mode set-> style sheet is the default dark one. + special reslist header
    bool darkMode;
    
    QString snipCssFile;
    QString queryStemLang;
    enum ShowMode {SHOW_NORMAL, SHOW_MAX, SHOW_FULL};
    int showmode{SHOW_NORMAL};
    int pvwidth; // Preview window geom
    int pvheight;
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
    // Display Snippets links even for un-paged documents
    bool alwaysSnippets;
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
    // Temporary value while we run a saved query. Erased right after use.
    bool useTmpActiveExtraDbs{false};
    vector<string> tmpActiveExtraDbs;
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

    // Remembered term match mode
    int termMatchType{0};

    // Program version that wrote this. Not used for now, in prevision
    // of the case where we might need an incompatible change
    int rclVersion{1505};
    // Suppress all noises
    bool noBeeps;
    
    bool noToolbars{false};
    bool noClearSearch{false};
    bool noStatusBar{false};
    bool noMenuBar{false};
    bool noSSTypCMB{false};
    bool resTableTextNoShift{false};
    bool resTableNoHoverMeta{false};
    bool noResTableHeader{false};
    bool showResTableVHeader{false};
    bool noResTableRowJumpSC{false};
    bool showTrayIcon{false};
    bool closeToTray{false};
    bool trayMessages{false};
    
    // See widgets/qxtconfirmationmessage.
    // Values -1/positive. -1 will trigger the dialog.
    int showTempFileWarning{-1};
    
    // Advanced search window clause list state
    vector<int> advSearchClauses;

    // Default paragraph format for result list
    static const char *dfltResListFormat;

    std::string stemlang();

    void setupDarkCSS();
    
    // MIME types for which we prefer to use stored text from preview
    // rather than extracting the possibly nicer HTML because the
    // extractor is very slow. This is compiled in and there is no UI
    // for now.
    std::set<std::string> preferStoredTextMimes{"application/x-hwp"};
};

/** Global preferences record */
extern PrefsPack prefs;

/** Read write settings from disk file */
extern void rwSettings(bool dowrite);

extern QString g_stringAllStem, g_stringNoStem;

#endif /* _GUIUTILS_H_INCLUDED_ */
