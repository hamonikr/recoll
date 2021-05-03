/* Copyright (C) 2004-2021 J.F.Dockes
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

#include <stdio.h>

#include "ecrontab.h"
#include "execmd.h"
#include "smallut.h"
#include "log.h"

// Read crontab file and split it into lines.
static bool eCrontabGetLines(vector<string>& lines)
{
    string crontab;
    ExecCmd croncmd;
    vector<string> args; 
    int status;

    // Retrieve current crontab contents. An error here means that no
    // crontab exists, and is not fatal, but we return a different
    // status than for an empty one
    args.push_back("-l");
    if ((status = croncmd.doexec("crontab", args, 0, &crontab))) {
        lines.clear();
        return false;
    }

    // Split crontab into lines
    stringToTokens(crontab, lines, "\n");
    return true;
}

// Concatenate lines and write crontab
static bool eCrontabWriteFile(const vector<string>& lines, string& reason)
{
    string crontab;
    ExecCmd croncmd;
    vector<string> args; 
    int status;

    for (const auto& line : lines) {
        crontab += line + "\n";
    }

    args.push_back("-");
    if ((status = croncmd.doexec("crontab", args, &crontab, 0))) {
        char nbuf[30]; 
        sprintf(nbuf, "0x%x", status);
        reason = string("Exec crontab -l failed: status: ") + nbuf;
        return false;
    }
    return true;
}

// Add / change / delete entry identified by marker and id
bool editCrontab(const string& marker, const string& id, 
                 const string& sched, const string& cmd, string& reason)
{
    vector<string> lines;

    if (!eCrontabGetLines(lines)) {
        // Special case: cmd is empty, no crontab, don't create one
        if (cmd.empty())
            return true;
    }

    // Remove old copy if any
    for (auto it = lines.begin(); it != lines.end(); it++) {
        // Skip comment
        if (it->find_first_of("#") == it->find_first_not_of(" \t"))
            continue;

        if (it->find(marker) != string::npos && it->find(id) != string::npos) {
            lines.erase(it);
            break;
        }
    }

    if (!cmd.empty()) {
        string nline = sched + " " + marker + " " + id + " " + cmd;
        lines.push_back(nline);
    }
    
    if (!eCrontabWriteFile(lines, reason))
        return false;

    return true;
}

bool checkCrontabUnmanaged(const string& marker, const string& data)
{
    vector<string> lines;
    if (!eCrontabGetLines(lines)) {
        // No crontab, answer is no
        return false;
    }
    // Scan crontab
    for (const auto& line : lines) {
        if (line.find(marker) == string::npos &&
            line.find(data) != string::npos) {
            return true;
        }
    }
    return false;
}

/** Retrieve the scheduling for a crontab entry */
bool getCrontabSched(const string& marker, const string& id, 
                     vector<string>& sched) 
{
    LOGDEB0("getCrontabSched: marker[" << marker << "], id[" << id << "]\n");
    vector<string> lines;
    if (!eCrontabGetLines(lines)) {
        // No crontab, answer is no
        sched.clear();
        return false;
    }
    string theline;

    for (const auto& line : lines) {
        // Skip comment
        if (line.find_first_of("#") == line.find_first_not_of(" \t"))
            continue;

        if (line.find(marker) != string::npos && 
            line.find(id) != string::npos) {
            theline = line;
            break;
        }
    }

    stringToTokens(theline, sched);
    sched.resize(5);
    return true;
}
