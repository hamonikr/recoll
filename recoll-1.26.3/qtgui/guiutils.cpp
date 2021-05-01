/* Copyright (C) 2005-2019 Jean-Francois Dockes
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

#include <algorithm>
#include <cstdio>

#include "recoll.h"
#include "log.h"
#include "smallut.h"
#include "guiutils.h"
#include "pathut.h"
#include "base64.h"
#include "advshist.h"

#include <QSettings>
#include <QStringList>

RclDynConf *g_dynconf;
AdvSearchHist *g_advshistory;
RclConfig *theconfig;

// The table should not be necessary, but I found no css way to get
// qt 4.6 qtextedit to clear the margins after the float img without 
// introducing blank space.
const char *PrefsPack::dfltResListFormat = 
    "<table class=\"respar\">\n"
    "<tr>\n"
    "<td><a href='%U'><img src='%I' width='64'></a></td>\n"
    "<td>%L &nbsp;<i>%S</i> &nbsp;&nbsp;<b>%T</b><br>\n"
    "<span style='white-space:nowrap'><i>%M</i>&nbsp;%D</span>&nbsp;&nbsp;&nbsp; <i>%U</i>&nbsp;%i<br>\n"
    "%A %K</td>\n"
    "</tr></table>\n"
    ;

// The global preferences structure
PrefsPack prefs;

// Using the same macro to read/write a setting. insurance against typing 
// mistakes
#define SETTING_RW(var, nm, tp, def)            \
    if (writing) {                              \
        settings.setValue(nm , var);            \
    } else {                                    \
        var = settings.value(nm, def).to##tp    \
            ();                                 \
    }                                           

/** 
 * Saving and restoring user preferences. These are stored in a global
 * structure during program execution and saved to disk using the QT
 * settings mechanism
 */
/* Remember if settings were actually read (to avoid writing them if
 * we stopped before reading them (else some kinds of errors would reset
 * the qt/recoll settings to defaults) */
static bool havereadsettings;

void rwSettings(bool writing)
{
    LOGDEB1("rwSettings: write " << writing << "\n");
    if (writing && !havereadsettings)
        return;
    QSettings settings("Recoll.org", "recoll");
    SETTING_RW(prefs.mainwidth, "/Recoll/geometry/width", Int, 0);
    SETTING_RW(prefs.mainheight, "/Recoll/geometry/height", Int, 0);
    SETTING_RW(prefs.showmode, "/Recoll/geometry/showmode", Int, 0);
    SETTING_RW(prefs.pvwidth, "/Recoll/geometry/pvwidth", Int, 0);
    SETTING_RW(prefs.pvheight, "/Recoll/geometry/pvheight", Int, 0);
    SETTING_RW(prefs.toolArea, "/Recoll/geometry/toolArea", Int, 0);
    SETTING_RW(prefs.resArea, "/Recoll/geometry/resArea", Int, 0);
    SETTING_RW(prefs.ssearchTypSav, "/Recoll/prefs/ssearchTypSav", Bool, 0);
    SETTING_RW(prefs.ssearchTyp, "/Recoll/prefs/simpleSearchTyp", Int, 3);
    SETTING_RW(prefs.startWithAdvSearchOpen, 
               "/Recoll/prefs/startWithAdvSearchOpen", Bool, false);
    SETTING_RW(prefs.previewHtml, "/Recoll/prefs/previewHtml", Bool, true);
    SETTING_RW(prefs.previewActiveLinks,
               "/Recoll/prefs/previewActiveLinks", Bool, false);

    QString advSearchClauses;
    const int maxclauselistsize = 20;
    if (writing) {
        // Limit clause list size to non-absurd size
        if (prefs.advSearchClauses.size() > maxclauselistsize) {
            prefs.advSearchClauses.resize(maxclauselistsize);
        }
        for (auto clause : prefs.advSearchClauses) {
            char buf[20];
            sprintf(buf, "%d ", clause);
            advSearchClauses += QString::fromUtf8(buf);
        }
    }
    QString ascdflt;
    SETTING_RW(advSearchClauses,"/Recoll/prefs/adv/clauseList", String, ascdflt);
    if (!writing) {
        vector<string> clauses;
        stringToStrings(qs2utf8s(advSearchClauses), clauses);
        // There was a long-lurking bug where the clause list was
        // growing to absurd sizes. The prefs.advSearchClauses clear()
        // call was missing (ok with the now false initial assumption
        // that the prefs were read once per session), which was
        // causing a doubling of the size each time the prefs were
        // read. Should be fixed, but in any case, limit the clause
        // list to a non-absurd size.
        if (clauses.size() > maxclauselistsize) {
            clauses.resize(maxclauselistsize);
        }
        prefs.advSearchClauses.clear();
        prefs.advSearchClauses.reserve(clauses.size());
        for (auto clause : clauses) {
            prefs.advSearchClauses.push_back(atoi(clause.c_str()));
        }
    }

    SETTING_RW(prefs.ssearchNoComplete, 
               "/Recoll/prefs/ssearch/noComplete", Bool, false);
    SETTING_RW(prefs.ssearchStartOnComplete, 
               "/Recoll/prefs/ssearch/startOnComplete", Bool, true);
    SETTING_RW(prefs.filterCtlStyle, "/Recoll/prefs/filterCtlStyle", Int, 0);
    SETTING_RW(prefs.ssearchAutoPhrase, 
               "/Recoll/prefs/ssearchAutoPhrase", Bool, true);
    SETTING_RW(prefs.ssearchAutoPhraseThreshPC, 
               "/Recoll/prefs/ssearchAutoPhraseThreshPC", Double, 2.0);
    SETTING_RW(prefs.respagesize, "/Recoll/prefs/reslist/pagelen", Int, 8);
    SETTING_RW(prefs.historysize, "/Recoll/prefs/historysize", Int, -1);
    SETTING_RW(prefs.collapseDuplicates, 
               "/Recoll/prefs/reslist/collapseDuplicates", Bool, false);
    SETTING_RW(prefs.showResultsAsTable, 
               "/Recoll/prefs/showResultsAsTable", Bool, false);
    SETTING_RW(prefs.maxhltextmbs, "/Recoll/prefs/preview/maxhltextmbs", Int, 3);

    SETTING_RW(prefs.previewPlainPre, 
               "/Recoll/prefs/preview/plainPre", Int, PrefsPack::PP_PREWRAP);

    // History: used to be able to only set a bare color name. Can now
    // set any CSS style. Hack on ':' presence to keep compat with old
    // values
    SETTING_RW(prefs.qtermstyle, "/Recoll/prefs/qtermcolor", String,
               "color: blue");
    if (!writing && prefs.qtermstyle == "")
        prefs.qtermstyle = "color: blue";
    { // histo compatibility hack
        int colon = prefs.qtermstyle.indexOf(":");
        int semi = prefs.qtermstyle.indexOf(";");
        // The 2nd part of the test is to keep compat with the
        // injection hack of the 1st user who suggested this (had
        // #ff5000;font-size:110%;... in 'qtermcolor')
        if (colon == -1 || (colon != -1 && semi != -1 && semi < colon)) {
            prefs.qtermstyle = QString::fromUtf8("color: ") + prefs.qtermstyle;
        }
    }

    SETTING_RW(prefs.reslistdateformat, "/Recoll/prefs/reslist/dateformat", 
               String,"&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z");
    if (!writing && prefs.reslistdateformat == "")
        prefs.reslistdateformat = "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z";
    prefs.creslistdateformat = (const char*)prefs.reslistdateformat.toUtf8();

    SETTING_RW(prefs.reslistfontfamily, "/Recoll/prefs/reslist/fontFamily", 
               String, "");
    SETTING_RW(prefs.reslistfontsize, "/Recoll/prefs/reslist/fontSize", Int, 
               10);

    QString rlfDflt = QString::fromUtf8(prefs.dfltResListFormat);
    if (writing) {
        if (prefs.reslistformat.compare(rlfDflt)) {
            settings.setValue("/Recoll/prefs/reslist/format", 
                              prefs.reslistformat);
        } else {
            settings.remove("/Recoll/prefs/reslist/format");
        }
    } else {
        prefs.reslistformat = 
            settings.value("/Recoll/prefs/reslist/format", rlfDflt).toString();
        prefs.creslistformat = qs2utf8s(prefs.reslistformat);
    }

    SETTING_RW(prefs.reslistheadertext, "/Recoll/prefs/reslist/headertext", 
               String, "");
    SETTING_RW(prefs.qssFile, "/Recoll/prefs/stylesheet", String, "");
    SETTING_RW(prefs.snipCssFile, "/Recoll/prefs/snippets/cssfile", String, "");
    SETTING_RW(prefs.queryStemLang, "/Recoll/prefs/query/stemLang", String,
               "english");
    SETTING_RW(prefs.useDesktopOpen, "/Recoll/prefs/useDesktopOpen", 
               Bool, true);

    SETTING_RW(prefs.keepSort, 
               "/Recoll/prefs/keepSort", Bool, false);
    SETTING_RW(prefs.sortField, "/Recoll/prefs/sortField", String, "");
    SETTING_RW(prefs.sortActive, 
               "/Recoll/prefs/sortActive", Bool, false);
    SETTING_RW(prefs.sortDesc, 
               "/Recoll/prefs/query/sortDesc", Bool, 0);
    if (!writing) {
        // Handle transition from older prefs which did not store sortColumn
        // (Active always meant sort by date).
        if (prefs.sortActive && prefs.sortField.isNull())
            prefs.sortField = "mtime";
    }

    SETTING_RW(prefs.queryBuildAbstract, 
               "/Recoll/prefs/query/buildAbstract", Bool, true);
    SETTING_RW(prefs.queryReplaceAbstract, 
               "/Recoll/prefs/query/replaceAbstract", Bool, false);
    SETTING_RW(prefs.syntAbsLen, "/Recoll/prefs/query/syntAbsLen", 
               Int, 250);
    SETTING_RW(prefs.syntAbsCtx, "/Recoll/prefs/query/syntAbsCtx", 
               Int, 4);
    // Abstract snippet separator
    SETTING_RW(prefs.abssep, "/Recoll/prefs/reslist/abssep", String,"&hellip;");
    if (!writing && prefs.abssep == "")
        prefs.abssep = "&hellip;";
    SETTING_RW(prefs.snipwMaxLength, "/Recoll/prefs/snipwin/maxlen", Int, 1000);
    SETTING_RW(prefs.snipwSortByPage,"/Recoll/prefs/snipwin/bypage", Bool,false);

    SETTING_RW(prefs.autoSuffs, "/Recoll/prefs/query/autoSuffs", String, "");
    SETTING_RW(prefs.autoSuffsEnable, 
               "/Recoll/prefs/query/autoSuffsEnable", Bool, false);

    SETTING_RW(prefs.synFileEnable, 
               "/Recoll/prefs/query/synFileEnable", Bool, false);
    SETTING_RW(prefs.synFile, "/Recoll/prefs/query/synfile", String, "");
    
    SETTING_RW(prefs.termMatchType, "/Recoll/prefs/query/termMatchType", 
               Int, 0);
    SETTING_RW(prefs.noBeeps, "/Recoll/prefs/query/noBeeps", Bool, false);

    // This is not really the current program version, just a value to
    // be used in case we have incompatible changes one day
    SETTING_RW(prefs.rclVersion, "/Recoll/prefs/rclVersion", Int, 1009);

    // Ssearch combobox history list
    if (writing) {
        settings.setValue("/Recoll/prefs/query/ssearchHistory",
                          prefs.ssearchHistory);
    } else {
        prefs.ssearchHistory = 
            settings.value("/Recoll/prefs/query/ssearchHistory").toStringList();
    }

    // Ignored file types (advanced search)
    if (writing) {
        settings.setValue("/Recoll/prefs/query/asearchIgnFilTyps",
                          prefs.asearchIgnFilTyps);
    } else {
        prefs.asearchIgnFilTyps = settings.value(
            "/Recoll/prefs/query/asearchIgnFilTyps").toStringList();
    }

    // Field list for the restable
    if (writing) {
        settings.setValue("/Recoll/prefs/query/restableFields",
                          prefs.restableFields);
    } else {
        prefs.restableFields = 
            settings.value("/Recoll/prefs/query/restableFields").toStringList();
        if (prefs.restableFields.empty()) {
            prefs.restableFields.push_back("date");
            prefs.restableFields.push_back("title");
            prefs.restableFields.push_back("filename");
            prefs.restableFields.push_back("author");
            prefs.restableFields.push_back("url");
        }
    }

    // restable col widths
    QString rtcw;
    if (writing) {
        for (const auto& width : prefs.restableColWidths) {
            char buf[20];
            sprintf(buf, "%d ", width);
            rtcw += QString::fromUtf8(buf);
        }
    }
    SETTING_RW(rtcw, "/Recoll/prefs/query/restableWidths", String, 
               "83 253 132 172 130 ");
    if (!writing) {
        prefs.restableColWidths.clear();
        vector<string> widths;
        stringToStrings(qs2utf8s(rtcw), widths);
        for (const auto& width : widths) {
            prefs.restableColWidths.push_back(atoi(width.c_str()));
        }
    }

    SETTING_RW(prefs.fileTypesByCats, "/Recoll/prefs/query/asearchFilTypByCat",
               Bool, false);
    SETTING_RW(prefs.showTrayIcon, "/Recoll/prefs/showTrayIcon", Bool, false);
    SETTING_RW(prefs.closeToTray, "/Recoll/prefs/closeToTray", Bool, false);
    SETTING_RW(prefs.trayMessages, "/Recoll/prefs/trayMessages", Bool, false);
    // See qxtconfirmationmessage. Needs to be -1 for the dialog to show.
    SETTING_RW(prefs.showTempFileWarning, "Recoll/prefs/showTempFileWarning",
               Int, -1);

    if (g_dynconf == 0) {
        // Happens
        return;
    }
    // The extra databases settings. These are stored as a list of
    // xapian directory names, encoded in base64 to avoid any
    // binary/charset conversion issues. There are 2 lists for all
    // known dbs and active (searched) ones.
    // When starting up, we also add from the RECOLL_EXTRA_DBS environment
    // variable.
    // This are stored inside the dynamic configuration file (aka: history), 
    // as they are likely to depend on RECOLL_CONFDIR.
    if (writing) {
        g_dynconf->eraseAll(allEdbsSk);
        for (const auto& dbdir : prefs.allExtraDbs) {
            g_dynconf->enterString(allEdbsSk, dbdir);
        }
        g_dynconf->eraseAll(actEdbsSk);
        for (const auto& dbdir : prefs.activeExtraDbs) {
            g_dynconf->enterString(actEdbsSk, dbdir);
        }
    } else {
        prefs.allExtraDbs = g_dynconf->getStringEntries<vector>(allEdbsSk);
        const char *cp;
        if ((cp = getenv("RECOLL_EXTRA_DBS")) != 0) {
            vector<string> dbl;
            stringToTokens(cp, dbl, ":");
            for (const auto& path : dbl) {
                string dbdir = path_canon(path);
                path_catslash(dbdir);
                if (std::find(prefs.allExtraDbs.begin(),
                              prefs.allExtraDbs.end(), dbdir) != 
                    prefs.allExtraDbs.end())
                    continue;
                bool stripped;
                if (!Rcl::Db::testDbDir(dbdir, &stripped)) {
                    LOGERR("Not a xapian index: [" << dbdir << "]\n");
                    continue;
                }
                if (stripped != o_index_stripchars) {
                    LOGERR("Incompatible character stripping: [" << dbdir <<
                           "]\n");
                    continue;
                }
                prefs.allExtraDbs.push_back(dbdir);
            }
        }

        // Get the remembered "active external indexes":
        prefs.activeExtraDbs = g_dynconf->getStringEntries<vector>(actEdbsSk);

        // Clean up the list: remove directories which are not
        // actually there: useful for removable volumes.
        for (auto it = prefs.activeExtraDbs.begin();
             it != prefs.activeExtraDbs.end();) {
            bool stripped;
            if (!Rcl::Db::testDbDir(*it, &stripped) || 
                stripped != o_index_stripchars) {
                LOGINFO("Not a Xapian index or char stripping differs: ["  <<
                        *it << "]\n");
                it = prefs.activeExtraDbs.erase(it);
            } else {
                it++;
            }
        }

        // Get active db directives from the environment. This can only add to
        // the remembered and cleaned up list
        const char *cp4Act;
        if ((cp4Act = getenv("RECOLL_ACTIVE_EXTRA_DBS")) != 0) {
            vector<string> dbl;
            stringToTokens(cp4Act, dbl, ":");
            for (const auto& path : dbl) {
                string dbdir = path_canon(path);
                path_catslash(dbdir);
                if (std::find(prefs.activeExtraDbs.begin(),
                              prefs.activeExtraDbs.end(), dbdir) !=
                    prefs.activeExtraDbs.end())
                    continue;
                bool strpd;
                if (!Rcl::Db::testDbDir(dbdir, &strpd) || 
                    strpd != o_index_stripchars) {
                    LOGERR("Not a Xapian dir or diff. char stripping: ["  <<
                           dbdir << "]\n");
                    continue;
                }
                prefs.activeExtraDbs.push_back(dbdir);
            } //for
        } //if
    }

#if 0
    std::cerr << "All extra Dbs:\n";
    for (const auto& dir : prefs.allExtraDbs)
        std::cerr << "    [" << dir << "]\n";
    std::cerr << "Active extra Dbs:\n";
    for (const auto& dir : prefs.activeExtraDbs)
        std::cerr << "    [" << dir << "]\n";
#endif

    const string asbdSk = "asearchSbd";
    if (writing) {
        while (prefs.asearchSubdirHist.size() > 20)
            prefs.asearchSubdirHist.pop_back();
        g_dynconf->eraseAll(asbdSk);
        for (const auto& qdbd : prefs.asearchSubdirHist) {
            g_dynconf->enterString(asbdSk, qs2utf8s(qdbd));
        }
    } else {
        vector<string> tl = g_dynconf->getStringEntries<vector>(asbdSk);
        for (const auto& dbd: tl) {
            prefs.asearchSubdirHist.push_back(u8s2qs(dbd.c_str()));
        }
    }
    if (!writing)
        havereadsettings = true;
}

string PrefsPack::stemlang()
{
    string stemLang(qs2utf8s(prefs.queryStemLang));
    if (stemLang == "ALL") {
        if (theconfig)
            theconfig->getConfParam("indexstemminglanguages", stemLang);
        else
            stemLang = "";
    }
    return stemLang;
}

#ifdef SHOWEVENTS
const char *eventTypeToStr(int tp)
{
    switch (tp) {
    case  0: return "None";
    case  1: return "Timer";
    case  2: return "MouseButtonPress";
    case  3: return "MouseButtonRelease";
    case  4: return "MouseButtonDblClick";
    case  5: return "MouseMove";
    case  6: return "KeyPress";
    case  7: return "KeyRelease";
    case  8: return "FocusIn";
    case  9: return "FocusOut";
    case  10: return "Enter";
    case  11: return "Leave";
    case  12: return "Paint";
    case  13: return "Move";
    case  14: return "Resize";
    case  15: return "Create";
    case  16: return "Destroy";
    case  17: return "Show";
    case  18: return "Hide";
    case  19: return "Close";
    case  20: return "Quit";
    case  21: return "ParentChange";
    case  131: return "ParentAboutToChange";
    case  22: return "ThreadChange";
    case  24: return "WindowActivate";
    case  25: return "WindowDeactivate";
    case  26: return "ShowToParent";
    case  27: return "HideToParent";
    case  31: return "Wheel";
    case  33: return "WindowTitleChange";
    case  34: return "WindowIconChange";
    case  35: return "ApplicationWindowIconChange";
    case  36: return "ApplicationFontChange";
    case  37: return "ApplicationLayoutDirectionChange";
    case  38: return "ApplicationPaletteChange";
    case  39: return "PaletteChange";
    case  40: return "Clipboard";
    case  42: return "Speech";
    case  43: return "MetaCall";
    case  50: return "SockAct";
    case  132: return "WinEventAct";
    case  52: return "DeferredDelete";
    case  60: return "DragEnter";
    case  61: return "DragMove";
    case  62: return "DragLeave";
    case  63: return "Drop";
    case  64: return "DragResponse";
    case  68: return "ChildAdded";
    case  69: return "ChildPolished";
    case  70: return "ChildInserted";
    case  72: return "LayoutHint";
    case  71: return "ChildRemoved";
    case  73: return "ShowWindowRequest";
    case  74: return "PolishRequest";
    case  75: return "Polish";
    case  76: return "LayoutRequest";
    case  77: return "UpdateRequest";
    case  78: return "UpdateLater";
    case  79: return "EmbeddingControl";
    case  80: return "ActivateControl";
    case  81: return "DeactivateControl";
    case  82: return "ContextMenu";
    case  83: return "InputMethod";
    case  86: return "AccessibilityPrepare";
    case  87: return "TabletMove";
    case  88: return "LocaleChange";
    case  89: return "LanguageChange";
    case  90: return "LayoutDirectionChange";
    case  91: return "Style";
    case  92: return "TabletPress";
    case  93: return "TabletRelease";
    case  94: return "OkRequest";
    case  95: return "HelpRequest";
    case  96: return "IconDrag";
    case  97: return "FontChange";
    case  98: return "EnabledChange";
    case  99: return "ActivationChange";
    case  100: return "StyleChange";
    case  101: return "IconTextChange";
    case  102: return "ModifiedChange";
    case  109: return "MouseTrackingChange";
    case  103: return "WindowBlocked";
    case  104: return "WindowUnblocked";
    case  105: return "WindowStateChange";
    case  110: return "ToolTip";
    case  111: return "WhatsThis";
    case  112: return "StatusTip";
    case  113: return "ActionChanged";
    case  114: return "ActionAdded";
    case  115: return "ActionRemoved";
    case  116: return "FileOpen";
    case  117: return "Shortcut";
    case  51: return "ShortcutOverride";
    case  30: return "Accel";
    case  32: return "AccelAvailable";
    case  118: return "WhatsThisClicked";
    case  120: return "ToolBarChange";
    case  121: return "ApplicationActivated";
    case  122: return "ApplicationDeactivated";
    case  123: return "QueryWhatsThis";
    case  124: return "EnterWhatsThisMode";
    case  125: return "LeaveWhatsThisMode";
    case  126: return "ZOrderChange";
    case  127: return "HoverEnter";
    case  128: return "HoverLeave";
    case  129: return "HoverMove";
    case  119: return "AccessibilityHelp";
    case  130: return "AccessibilityDescription";
    case  150: return "EnterEditFocus";
    case  151: return "LeaveEditFocus";
    case  152: return "AcceptDropsChange";
    case  153: return "MenubarUpdated";
    case  154: return "ZeroTimerEvent";
    case  155: return "GraphicsSceneMouseMove";
    case  156: return "GraphicsSceneMousePress";
    case  157: return "GraphicsSceneMouseRelease";
    case  158: return "GraphicsSceneMouseDoubleClick";
    case  159: return "GraphicsSceneContextMenu";
    case  160: return "GraphicsSceneHoverEnter";
    case  161: return "GraphicsSceneHoverMove";
    case  162: return "GraphicsSceneHoverLeave";
    case  163: return "GraphicsSceneHelp";
    case  164: return "GraphicsSceneDragEnter";
    case  165: return "GraphicsSceneDragMove";
    case  166: return "GraphicsSceneDragLeave";
    case  167: return "GraphicsSceneDrop";
    case  168: return "GraphicsSceneWheel";
    case  169: return "KeyboardLayoutChange";
    case  170: return "DynamicPropertyChange";
    case  171: return "TabletEnterProximity";
    case  172: return "TabletLeaveProximity";
    default: return "UnknownEvent";
    }
}
#endif
