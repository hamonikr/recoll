/* Copyright (C) 2016 J.F.Dockes 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "cmdtalk.h"

#include <stdio.h>

#include <iostream>
#include <sstream>
#include <mutex>

#include "smallut.h"
#include "execmd.h"
#ifdef MDU_INCLUDE_LOG
#include MDU_INCLUDE_LOG
#else
#include "log.h"
#endif

using namespace std;

class TimeoutExcept {};

class Canceler : public ExecCmdAdvise {
public:
    Canceler(int tmsecs) 
        : m_timeosecs(tmsecs) {}

    virtual void newData(int) {
        if (m_starttime && (time(0) - m_starttime) > m_timeosecs) {
            throw TimeoutExcept();
        }
    }

    void reset() {
        m_starttime = time(0);
    }
    int m_timeosecs;
    time_t m_starttime{0};
};

class CmdTalk::Internal {
public:
    Internal(int timeosecs)
        : m_cancel(timeosecs) {}

    ~Internal() {
        delete cmd;
    }

    bool readDataElement(string& name, string &data);

    bool talk(const pair<string, string>& arg0,
              const unordered_map<string, string>& args,
              unordered_map<string, string>& rep);
    bool running();
    
    ExecCmd *cmd{0};
    bool failed{false};
    Canceler m_cancel;
    std::mutex mmutex;
};

CmdTalk::CmdTalk(int timeosecs)
{
    m = new Internal(timeosecs);
}
CmdTalk::~CmdTalk()
{
    delete m;
}

bool CmdTalk::startCmd(const string& cmdname,
                       const vector<string>& args,
                       const vector<string>& env,
                       const vector<string>& path)
{
    LOGDEB("CmdTalk::startCmd\n");
    if (m->failed) {
        LOGINF("CmdTalk: command failed, not restarting\n");
        return false;
    }
    delete m->cmd;
    m->cmd = new ExecCmd;
    m->cmd->setAdvise(&m->m_cancel);

    for (const auto& it : env) {
        m->cmd->putenv(it);
    }

    string acmdname(cmdname);
    if (!path.empty()) {
        string colonpath;
        for (const auto& it: path) {
            colonpath += it + ":";
        }
        if (!colonpath.empty()) {
            colonpath.erase(colonpath.size()-1);
        }
        LOGDEB("CmdTalk::startCmd: PATH: [" << colonpath << "]\n");
        ExecCmd::which(cmdname, acmdname, colonpath.c_str());
    }

    if (m->cmd->startExec(acmdname, args, 1, 1) < 0) {
        return false;
    }
    return true;
}

// Messages are made of data elements. Each element is like:
// name: len\ndata
// An empty line signals the end of the message, so the whole thing
// would look like:
// Name1: Len1\nData1Name2: Len2\nData2\n
bool CmdTalk::Internal::readDataElement(string& name, string &data)
{
    string ibuf;

    m_cancel.reset();
    try {
        // Read name and length
        if (cmd->getline(ibuf) <= 0) {
            LOGERR("CmdTalk: getline error\n");
            return false;
        }
    } catch (TimeoutExcept) {
        LOGINF("CmdTalk:readDataElement: fatal timeout (" <<
               m_cancel.m_timeosecs << " S)\n");
        return false;
    }
    
    LOGDEB1("CmdTalk:rde: line [" << ibuf << "]\n");

    // Empty line (end of message) ?
    if (!ibuf.compare("\n")) {
        LOGDEB1("CmdTalk: Got empty line\n");
        return true;
    }

    // We're expecting something like Name: len\n
    vector<string> tokens;
    stringToTokens(ibuf, tokens);
    if (tokens.size() != 2) {
        LOGERR("CmdTalk: bad line in filter output: [" << ibuf << "]\n");
        return false;
    }
    vector<string>::iterator it = tokens.begin();
    name = *it++;
    string& slen = *it;
    int len;
    if (sscanf(slen.c_str(), "%d", &len) != 1) {
        LOGERR("CmdTalk: bad line in filter output: [" << ibuf << "]\n");
        return false;
    }

    // Read element data
    data.erase();
    if (len > 0 && cmd->receive(data, len) != len) {
        LOGERR("CmdTalk: expected " << len << " bytes of data, got " <<
               data.length() << "\n");
        return false;
    }
    LOGDEB1("CmdTalk:rde: got: name [" << name << "] len " << len <<"value ["<<
            (data.size() > 100 ? (data.substr(0, 100) + " ...") : data)<< endl);
    return true;
}

bool CmdTalk::Internal::running()
{
    if (failed || nullptr == cmd || cmd->getChildPid() <= 0) {
        return false;
    }
        
    int status;
    if (cmd->maybereap(&status)) {
        // Command exited. Set error status so that a restart will fail.
        LOGERR("CmdTalk::talk: command exited\n");
        failed = true;
        return false;
    }
    return true;
}

bool CmdTalk::Internal::talk(const pair<string, string>& arg0,
                             const unordered_map<string, string>& args,
                             unordered_map<string, string>& rep)
{
    std::unique_lock<std::mutex> lock(mmutex);

    if (!running()) {
        LOGERR("CmdTalk::talk: no process\n");
        return false;
    }

    ostringstream obuf;
    if (!arg0.first.empty()) {
        obuf << arg0.first << ": " << arg0.second.size() << "\n" << arg0.second;
    }
    for (const auto& it : args) {
        obuf << it.first << ": " << it.second.size() << "\n" << it.second;
    }
    obuf << "\n";

    if (cmd->send(obuf.str()) < 0) {
        cmd->zapChild();
        LOGERR("CmdTalk: send error\n");
        return false;
    }

    // Read answer (multiple elements)
    LOGDEB1("CmdTalk: reading answer\n");
    for (;;) {
        string name, data;
        if (!readDataElement(name, data)) {
            cmd->zapChild();
            return false;
        }
        if (name.empty()) {
            break;
        }
        trimstring(name, ":");
        LOGDEB1("CmdTalk: got [" << name << "] -> [" << data << "]\n");
        rep[name] = data;
    }

    if (rep.find("cmdtalkstatus") != rep.end()) {
        return false;
    } else {
        return true;
    }
}

bool CmdTalk::running()
{
    if (nullptr == m)
        return false;
    return m->running();
}

bool CmdTalk::talk(const unordered_map<string, string>& args,
                   unordered_map<string, string>& rep)
{
    if (nullptr == m)
        return false;
    return m->talk({"",""}, args, rep);
}

bool CmdTalk::callproc(
    const string& proc,
    const unordered_map<std::string, std::string>& args,
    unordered_map<std::string, std::string>& rep)
{
    if (nullptr == m)
        return false;
    return m->talk({"cmdtalk:proc", proc}, args, rep);
}
