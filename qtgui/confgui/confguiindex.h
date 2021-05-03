/* Copyright (C) 2007 J.F.Dockes
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

#ifndef _confguiindex_h_included_
#define _confguiindex_h_included_

/**
 * Classes to handle the gui for the indexing configuration. These group 
 * confgui elements, linked to configuration parameters, into panels.
 */

#include <QWidget>
#include <QString>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QListWidgetItem>
#include <QStringList>

#include <string>
#include <vector>

#include "confgui.h"

class ConfNull;
class RclConfig;

class ConfIndexW : public QWidget {
    Q_OBJECT
public:
    ConfIndexW(QWidget *parent, RclConfig *config)
        : m_parent(parent), m_rclconf(config) {}

public slots:
    void showPrefs(bool modal);
    void acceptChanges();
    QWidget *getDialog() {return m_w;}
    
private:
    void initPanels();
    bool setupTopPanel(int idx);
    bool setupWebHistoryPanel(int idx);
    bool setupSearchPanel(int idx);

    QWidget *m_parent;
    RclConfig *m_rclconf;
    ConfNull  *m_conf{nullptr};
    confgui::ConfTabsW *m_w{nullptr};
    QStringList m_stemlangs;
};


/** A special panel for parameters which may change in subdirectories: */
class ConfSubPanelW : public QWidget, public confgui::ConfPanelWIF {
    Q_OBJECT;

public:
    ConfSubPanelW(QWidget *parent, ConfNull **config, RclConfig *rclconf);

    virtual void storeValues();
    virtual void loadValues();

private slots:
    void subDirChanged(QListWidgetItem *, QListWidgetItem *);
    void subDirDeleted(QString);
    void restoreEmpty();
private:
    std::string            m_sk;
    ConfNull         **m_config;
    confgui::ConfParamDNLW    *m_subdirs;
    std::vector<confgui::ConfParamW*> m_widgets;
    QGroupBox        *m_groupbox;
};

#endif /* _confguiindex_h_included_ */
