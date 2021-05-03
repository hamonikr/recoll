/* Copyright (C) 2006-2021 J.F.Dockes
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

#include <mutex>
#include <stdlib.h>

#include "safeunistd.h"
#include "dlib.h"
#include "pathut.h"
#include "execmd.h"
#include "rclaspell.h"
#include "log.h"
#include "unacpp.h"
#include "rclutil.h"

using namespace std;

// Private rclaspell data
class AspellData {
public:
    string m_exec;
    ExecCmd m_speller;
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
        path_pkgdatadir(), "filters/aspell-installed/mingw32/lib/aspell-0.60");
    if (m_data->m_addCreateParam.empty()) {
        m_data->m_addCreateParam =
            string("--local-data-dir=") + path_cat(m_config->getConfDir(), "aspell");
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

    return true;
}


bool Aspell::ok() const
{
    return nullptr != m_data;
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

bool Aspell::make_speller(string& reason)
{
    if (!ok())
        return false;
    if (m_data->m_speller.getChildPid() > 0)
        return true;

    // aspell --lang=[lang] --encoding=utf-8 --master=[dicPath()] --sug-mode=fast --mode=none pipe

    string cmdstring(m_data->m_exec);

    ExecCmd aspell;
    vector<string> args;

    args.push_back(string("--lang=")+ m_lang);
    cmdstring += string(" ") + args.back();

    args.push_back("--encoding=utf-8");
    cmdstring += string(" ") + args.back();

#ifdef _WIN32
    args.push_back(string("--data-dir=") + m_data->m_datadir);
    cmdstring += string(" ") + args.back();
#endif

    if (!m_data->m_addCreateParam.empty()) {
        args.push_back(m_data->m_addCreateParam);
        cmdstring += string(" ") + args.back();
    }

    args.push_back(string("--master=") + dicPath());
    cmdstring += string(" ") + args.back();

    args.push_back(string("--sug-mode=fast"));
    cmdstring += string(" ") + args.back();

    args.push_back(string("--mode=none"));
    cmdstring += string(" ") + args.back();

    args.push_back("pipe");
    cmdstring += string(" ") + args.back();
                   
    LOGDEB("Starting aspell command [" << cmdstring << "]\n");
    if (m_data->m_speller.startExec(m_data->m_exec, args, true, true) != 0) {
        reason += "Can't start aspell: " + cmdstring;
        return false;
    }
    // Read initial line from aspell: version etc.
    string line;
    if (m_data->m_speller.getline(line, 2) <= 0) {
        reason += "Aspell: failed reading initial line";
        m_data->m_speller.zapChild();
        return false;
    }
    LOGDEB("rclaspell: aspell initial answer: [" << line << "]\n");
    return true;
}

bool Aspell::suggest(
    Rcl::Db &db, const string &_term, vector<string>& suggestions, string& reason)
{
    LOGDEB("Aspell::suggest: term [" << _term << "]\n");
    if (!ok() || !make_speller(reason))
        return false;
    string mterm(_term);
    if (mterm.empty())
        return true; //??

    if (!Rcl::Db::isSpellingCandidate(mterm)) {
        LOGDEB0("Aspell::suggest: [" << mterm << " not spelling candidate, return empty/true\n");
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

    m_data->m_speller.send(mterm + "\n");
    std::string line;
    if (m_data->m_speller.getline(line, 3) <= 0) {
        reason.append("Aspell error: ");
        return false;
    }
    LOGDEB1("ASPELL: got answer: " << line << "\n");
    string empty;
    if (m_data->m_speller.getline(empty, 1) <= 0) {
        reason.append("Aspell: failed reading final empty line\n");
        return false;
    }

    if (line[0] == '*' || line[0] == '#') {
        // Word is in dictionary, or there are no suggestions
        return true;
    }
    string::size_type colon;
    // Aspell suggestions line: & original count offset: miss, miss, …
    if (line[0] != '&' || (colon = line.find(':')) == string::npos || colon == line.size()-1) {
        // ??
        reason.append("Aspell: bad answer line: ");
        reason.append(line);
        return false;
    }
    std::vector<std::string> words;
    stringSplitString(line.substr(colon + 2), words, ", ");
    for (const auto& word : words) {
        if (db.termExists(word))
            suggestions.push_back(word);
    }
    return true;
}

#endif // RCL_USE_ASPELL
