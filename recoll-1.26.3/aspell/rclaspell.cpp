/* Copyright (C) 2006-2019 J.F.Dockes
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

#ifdef RCL_USE_ASPELL

#include ASPELL_INCLUDE

#include <mutex>
#include <unistd.h>
#include <stdlib.h>

#include "dlib.h"
#include "pathut.h"
#include "execmd.h"
#include "rclaspell.h"
#include "log.h"
#include "unacpp.h"
#include "rclutil.h"

using namespace std;

// Aspell library entry points
class AspellApi {
public:
    struct AspellConfig *(*new_aspell_config)();
    int (*aspell_config_replace)(struct AspellConfig *, const char * key, 
                                 const char * value);
    struct AspellCanHaveError *(*new_aspell_speller)(struct AspellConfig *);
    void (*delete_aspell_config)(struct AspellConfig *);
    void (*delete_aspell_can_have_error)(struct AspellCanHaveError *);
    struct AspellSpeller * (*to_aspell_speller)(struct AspellCanHaveError *);
    struct AspellConfig * (*aspell_speller_config)(struct AspellSpeller *);
    const struct AspellWordList * (*aspell_speller_suggest)
        (struct AspellSpeller *, const char *, int);
    int (*aspell_speller_check)(struct AspellSpeller *, const char *, int);
    struct AspellStringEnumeration * (*aspell_word_list_elements)
        (const struct AspellWordList * ths);
    const char * (*aspell_string_enumeration_next)
        (struct AspellStringEnumeration * ths);
    void (*delete_aspell_string_enumeration)(struct AspellStringEnumeration *);
    const struct AspellError *(*aspell_error)
        (const struct AspellCanHaveError *);
    const char *(*aspell_error_message)(const struct AspellCanHaveError *);
    const char *(*aspell_speller_error_message)(const struct AspellSpeller *);
    void (*delete_aspell_speller)(struct AspellSpeller *);

};
static AspellApi aapi;
static std::mutex o_aapi_mutex;

#define NMTOPTR(NM, TP)                                         \
    if ((aapi.NM = TP dlib_sym(m_data->m_handle, #NM)) == 0) {  \
        badnames += #NM + string(" ");                          \
    }

static const vector<string> aspell_lib_suffixes {
#if defined(__APPLE__) 
    ".15.dylib",
        ".dylib",
#elif defined(_WIN32)
        "-15.dll",
#else
        ".so",
        ".so.15",
#endif
        };

// Private rclaspell data
class AspellData {
public:
    ~AspellData() {
        LOGDEB2("~AspellData\n" );
        if (m_handle) {
            dlib_close(m_handle);
            m_handle = nullptr;
        }
        if (m_speller) {
            // Dumps core if I do this?? 
            //aapi.delete_aspell_speller(m_speller);
            m_speller = nullptr;
            LOGDEB2("~AspellData: speller done\n" );
        }
    }

    void  *m_handle{nullptr};
    string m_exec;
    AspellSpeller *m_speller{nullptr};
#ifdef _WIN32
    string m_datadir;
#endif
    string m_addCreateParam;
};

Aspell::Aspell(const RclConfig *cnf)
    : m_config(cnf)
{
}

Aspell::~Aspell()
{
    deleteZ(m_data);
}

bool Aspell::init(string &reason)
{
    std::unique_lock<std::mutex> locker(o_aapi_mutex);
    deleteZ(m_data);

    // Language: we get this from the configuration, else from the NLS
    // environment. The aspell language names used for selecting language 
    // definition files (used to create dictionaries) are like en, fr
    if (!m_config->getConfParam("aspellLanguage", m_lang) || m_lang.empty()) {
        string lang = "en";
        const char *cp;
        if ((cp = getenv("LC_ALL")))
            lang = cp;
        else if ((cp = getenv("LANG")))
            lang = cp;
        if (!lang.compare("C"))
            lang = "en";
        m_lang = lang.substr(0, lang.find_first_of("_"));
        if (!m_lang.compare("ja")) {
            // Aspell has no support for Japanese. We substitute
            // english, as Japanese users often have texts with
            // interspersed english words or english texts. Japanese
            // parts of the text won't be sent to aspell (check
            // Rcl::Db::isSpellingCandidate())
            m_lang = "en";
        }
    }

    m_data = new AspellData;

    m_config->getConfParam("aspellAddCreateParam", m_data->m_addCreateParam);
#ifdef _WIN32
    m_data->m_datadir = path_cat(
        path_pkgdatadir(),
        "filters/aspell-installed/mingw32/lib/aspell-0.60");
    if (m_data->m_addCreateParam.empty()) {
        m_data->m_addCreateParam = string("--local-data-dir=") +
            path_cat(m_config->getConfDir(), "aspell");
    }
#endif // WIN32
    
    const char *aspell_prog_from_env = getenv("ASPELL_PROG");
    if (aspell_prog_from_env && access(aspell_prog_from_env, X_OK) == 0) {
        m_data->m_exec = aspell_prog_from_env;
    }
#ifdef ASPELL_PROG
    if (m_data->m_exec.empty()) {
        string cmd = m_config->findFilter(ASPELL_PROG);
        LOGDEB("rclaspell::init: findFilter returns " << cmd << endl);
        if (path_isabsolute(cmd)) {
            m_data->m_exec.swap(cmd);
        }
    }
#endif // ASPELL_PROG
    if (m_data->m_exec.empty()) {
        ExecCmd::which("aspell", m_data->m_exec);
    }
    if (m_data->m_exec.empty()) {
        reason = "aspell program not found or not executable";
        deleteZ(m_data);
        return false;
    }

    // Don't know what with Apple and (DY)LD_LIBRARY_PATH. Does not work
    // So we look in all ../lib in the PATH...
#if defined(__APPLE__) 
    vector<string> path;
    const char *pp = getenv("PATH");
    if (pp) {
        stringToTokens(pp, path, ":");
    }
#endif
    
    reason = "Could not open shared library ";
    string libbase("libaspell");
    string lib;
    for (const auto& suff : aspell_lib_suffixes) {
        lib = libbase + suff;
        reason += string("[") + lib + "] ";
        if ((m_data->m_handle = dlib_open(lib)) != 0) {
            reason.erase();
            goto found;
        }
        // Above was the normal lookup: let dlopen search the directories.
        // Also look in other places for Apple and Windows.
#if defined(__APPLE__) 
        for (const auto& dir : path) {
            string lib1 = path_canon(dir + "/../lib/" + lib);
            if ((m_data->m_handle = dlib_open(lib1)) != 0) {
                reason.erase();
                lib=lib1;
                goto found;
            }
        }
#endif
#if defined(_WIN32)
        // Look in the directory of the aspell binary
        {
            string bindir = path_getfather(m_data->m_exec);
            string lib1 = path_cat(bindir, lib);
            if ((m_data->m_handle = dlib_open(lib1)) != 0) {
                reason.erase();
                lib=lib1;
                goto found;
            }
        }
#endif
    }
    
found:
    if (m_data->m_handle == 0) {
        reason += string(" : ") + dlib_error();
        deleteZ(m_data);
        return false;
    }

    string badnames;
    NMTOPTR(new_aspell_config, (struct AspellConfig *(*)()));
    NMTOPTR(aspell_config_replace, (int (*)(struct AspellConfig *, 
                                            const char *, const char *)));
    NMTOPTR(new_aspell_speller, 
            (struct AspellCanHaveError *(*)(struct AspellConfig *)));
    NMTOPTR(delete_aspell_config, 
            (void (*)(struct AspellConfig *)));
    NMTOPTR(delete_aspell_can_have_error, 
            (void (*)(struct AspellCanHaveError *)));
    NMTOPTR(to_aspell_speller, 
            (struct AspellSpeller *(*)(struct AspellCanHaveError *)));
    NMTOPTR(aspell_speller_config, 
            (struct AspellConfig *(*)(struct AspellSpeller *)));
    NMTOPTR(aspell_speller_suggest, 
            (const struct AspellWordList *(*)(struct AspellSpeller *, 
                                              const char *, int)));
    NMTOPTR(aspell_speller_check, 
            (int (*)(struct AspellSpeller *, const char *, int)));
    NMTOPTR(aspell_word_list_elements, 
            (struct AspellStringEnumeration *(*)
             (const struct AspellWordList *)));
    NMTOPTR(aspell_string_enumeration_next, 
            (const char * (*)(struct AspellStringEnumeration *)));
    NMTOPTR(delete_aspell_string_enumeration, 
            (void (*)(struct AspellStringEnumeration *)));
    NMTOPTR(aspell_error, 
            (const struct AspellError*(*)(const struct AspellCanHaveError *)));
    NMTOPTR(aspell_error_message,
            (const char *(*)(const struct AspellCanHaveError *)));
    NMTOPTR(aspell_speller_error_message, 
            (const char *(*)(const struct AspellSpeller *)));
    NMTOPTR(delete_aspell_speller, (void (*)(struct AspellSpeller *)));

    if (!badnames.empty()) {
        reason = string("Aspell::init: symbols not found:") + badnames;
        deleteZ(m_data);
        return false;
    }

    return true;
}

bool Aspell::ok() const
{
    return m_data != 0 && m_data->m_handle != 0;
}

string Aspell::dicPath()
{
    string ccdir = m_config->getAspellcacheDir();
    return path_cat(ccdir, string("aspdict.") + m_lang + string(".rws"));
}


// The data source for the create dictionary aspell command. We walk
// the term list, filtering out things that are probably not words.
// Note that the manual for the current version (0.60) of aspell
// states that utf-8 is not well supported, so that we should maybe
// also filter all 8bit chars. Info is contradictory, so we only
// filter out CJK which is definitely not supported (katakana would
// make sense though, but currently no support).
class AspExecPv : public ExecCmdProvide {
public:
    string *m_input; // pointer to string used as input buffer to command
    Rcl::TermIter *m_tit;
    Rcl::Db &m_db;
    AspExecPv(string *i, Rcl::TermIter *tit, Rcl::Db &db) 
        : m_input(i), m_tit(tit), m_db(db)
        {}
    void newData() {
        while (m_db.termWalkNext(m_tit, *m_input)) {
            LOGDEB2("Aspell::buildDict: term: ["  << (m_input) << "]\n" );
            if (!Rcl::Db::isSpellingCandidate(*m_input)) {
                LOGDEB2("Aspell::buildDict: SKIP\n" );
                continue;
            }
            if (!o_index_stripchars) {
                string lower;
                if (!unacmaybefold(*m_input, lower, "UTF-8", UNACOP_FOLD))
                    continue;
                m_input->swap(lower);
            }
            // Got a non-empty sort-of appropriate term, let's send it to
            // aspell
            LOGDEB2("Apell::buildDict: SEND\n" );
            m_input->append("\n");
            return;
        }
        // End of data. Tell so. Exec will close cmd.
        m_input->erase();
    }
};


bool Aspell::buildDict(Rcl::Db &db, string &reason)
{
    if (!ok())
        return false;

    // We create the dictionary by executing the aspell command:
    // aspell --lang=[lang] create master [dictApath]
    string cmdstring(m_data->m_exec);
    ExecCmd aspell;
    vector<string> args;
    args.push_back(string("--lang=")+ m_lang);
    cmdstring += string(" ") + string("--lang=") + m_lang;
    args.push_back("--encoding=utf-8");
    cmdstring += string(" ") + "--encoding=utf-8";
#ifdef _WIN32
    args.push_back(string("--data-dir=") + m_data->m_datadir);
#endif
    if (!m_data->m_addCreateParam.empty()) {
        args.push_back(m_data->m_addCreateParam);
        cmdstring += string(" ") + m_data->m_addCreateParam;
    }
    args.push_back("create");
    cmdstring += string(" ") + "create";
    args.push_back("master");
    cmdstring += string(" ") + "master";
    args.push_back(dicPath());
    cmdstring += string(" ") + dicPath();

    // Have to disable stderr, as numerous messages about bad strings are
    // printed. We'd like to keep errors about missing databases though, so
    // make it configurable for diags
    bool keepStderr = false;
    m_config->getConfParam("aspellKeepStderr", &keepStderr);
    if (!keepStderr)
        aspell.setStderr("/dev/null");

    Rcl::TermIter *tit = db.termWalkOpen();
    if (tit == 0) {
        reason = "termWalkOpen failed\n";
        return false;
    }
    string termbuf;
    AspExecPv pv(&termbuf, tit, db);
    aspell.setProvide(&pv);
    
    if (aspell.doexec(m_data->m_exec, args, &termbuf)) {
        ExecCmd cmd;
        args.clear();
        args.push_back("dicts");
        string dicts;
        bool hasdict = false;
        if (cmd.doexec(m_data->m_exec, args, 0, &dicts)) {
            vector<string> vdicts;
            stringToTokens(dicts, vdicts, "\n\r\t ");
            if (find(vdicts.begin(), vdicts.end(), m_lang) != vdicts.end()) {
                hasdict = true;
            }
        }
        if (hasdict) {
            reason = string("\naspell dictionary creation command [") +
                cmdstring;
            reason += string(
                "] failed. Reason unknown.\n"
                "Try to set aspellKeepStderr = 1 in recoll.conf, and execute \n"
                "the indexing command in a terminal to see the aspell "
                "diagnostic output.\n");
        } else {
            reason = string("aspell dictionary creation command failed:\n") +
                cmdstring + "\n"
                "One possible reason might be missing language "
                "data files for lang = " + m_lang +
                ". Maybe try to execute the command by hand for a better diag.";
        }
        return false;
    }
    db.termWalkClose(tit);
    return true;
}

static const unsigned int ldatadiroptsz =
    string("--local-data-dir=").size();

bool Aspell::make_speller(string& reason)
{
    if (!ok())
        return false;
    if (m_data->m_speller != 0)
        return true;

    AspellCanHaveError *ret;

    AspellConfig *config = aapi.new_aspell_config();
    aapi.aspell_config_replace(config, "lang", m_lang.c_str());
    aapi.aspell_config_replace(config, "encoding", "utf-8");
    aapi.aspell_config_replace(config, "master", dicPath().c_str());
    aapi.aspell_config_replace(config, "sug-mode", "fast");
#ifdef _WIN32
    aapi.aspell_config_replace(config, "data-dir", m_data->m_datadir.c_str());
#endif
    if (m_data->m_addCreateParam.size() > ldatadiroptsz) {
        aapi.aspell_config_replace(
            config, "local-data-dir",
            m_data->m_addCreateParam.substr(ldatadiroptsz).c_str());
    }
    //    aapi.aspell_config_replace(config, "sug-edit-dist", "2");
    ret = aapi.new_aspell_speller(config);
    aapi.delete_aspell_config(config);

    if (aapi.aspell_error(ret) != 0) {
        reason = aapi.aspell_error_message(ret);
        aapi.delete_aspell_can_have_error(ret);
        return false;
    }
    m_data->m_speller = aapi.to_aspell_speller(ret);
    return true;
}

bool Aspell::check(const string &iterm, string& reason)
{
    LOGDEB("Aspell::check [" << iterm << "]\n");
    string mterm(iterm);

    if (!Rcl::Db::isSpellingCandidate(mterm)) {
        LOGDEB0("Aspell::check: [" << mterm <<
                " not spelling candidate, return true\n");
        return true;
    }
    if (!ok() || !make_speller(reason))
        return false;
    if (iterm.empty())
        return true; //??

    if (!o_index_stripchars) {
        string lower;
        if (!unacmaybefold(mterm, lower, "UTF-8", UNACOP_FOLD)) {
            LOGERR("Aspell::check: cant lowercase input\n");
            return false;
        }
        mterm.swap(lower);
    }

    int ret = aapi.aspell_speller_check(m_data->m_speller, 
                                        mterm.c_str(), mterm.length());
    reason.clear();
    switch (ret) {
    case 0: return false;
    case 1: return true;
    default:
    case -1:
        reason.append("Aspell error: ");
        reason.append(aapi.aspell_speller_error_message(m_data->m_speller));
        return false;
    }
}

bool Aspell::suggest(Rcl::Db &db, const string &_term, 
                     list<string>& suggestions, string& reason)
{
    LOGDEB("Aspell::suggest: term [" << _term << "]\n");
    if (!ok() || !make_speller(reason))
        return false;
    string mterm(_term);
    if (mterm.empty())
        return true; //??

    if (!Rcl::Db::isSpellingCandidate(mterm)) {
        LOGDEB0("Aspell::suggest: [" << mterm <<
                " not spelling candidate, return empty/true\n");
        return true;
    }

    if (!o_index_stripchars) {
        string lower;
        if (!unacmaybefold(mterm, lower, "UTF-8", UNACOP_FOLD)) {
            LOGERR("Aspell::check : cant lowercase input\n");
            return false;
        }
        mterm.swap(lower);
    }

    const AspellWordList *wl = 
        aapi.aspell_speller_suggest(m_data->m_speller, 
                                    mterm.c_str(), mterm.length());
    if (wl == 0) {
        reason = aapi.aspell_speller_error_message(m_data->m_speller);
        return false;
    }
    AspellStringEnumeration *els = aapi.aspell_word_list_elements(wl);
    const char *word;
    while ((word = aapi.aspell_string_enumeration_next(els)) != 0) {
        LOGDEB0("Aspell::suggest: got [" << word << "]\n");
        // Check that the word exists in the index (we don't want
        // aspell computed stuff, only exact terms from the
        // dictionary).  We used to also check that it stems
        // differently from the base word but this is complicated
        // (stemming on/off + language), so we now leave this to the
        // caller.
        if (db.termExists(word))
            suggestions.push_back(word);
    }
    aapi.delete_aspell_string_enumeration(els);
    return true;
}

#endif // RCL_USE_ASPELL
