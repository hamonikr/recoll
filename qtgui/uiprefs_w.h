/* Copyright (C) 2006 J.F.Dockes 
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
#ifndef _UIPREFS_W_H_INCLUDED_
#define _UIPREFS_W_H_INCLUDED_
#include <qvariant.h>
#include <qdialog.h>

#include "ui_uiprefs.h"

#include <vector>
#include <QString>

class QDialog;
class ViewAction;
class RclMain;

class UIPrefsDialog : public QDialog, public Ui::uiPrefsDialogBase
{
    Q_OBJECT

public:
    UIPrefsDialog(RclMain* parent)
        : QDialog((QWidget*)parent), m_mainWindow(parent) {
        setupUi(this);
        init();
    }
    ~UIPrefsDialog(){};

    virtual void init();
    void setFromPrefs();
                           
public slots:
    virtual void showFontDialog();
    virtual void resetReslistFont();
    virtual void showStylesheetDialog();
    virtual void resetStylesheet();
    virtual void setDarkMode();
    virtual void showSynFileDialog();
    virtual void showSnipCssDialog();
    virtual void resetSnipCss();
    virtual void showViewAction();
    virtual void showViewAction(const QString& mt);
    virtual void addExtraDbPB_clicked();
    virtual void delExtraDbPB_clicked();
    virtual void togExtraDbPB_clicked();
    virtual void on_showTrayIconCB_clicked();
    virtual void actAllExtraDbPB_clicked();
    virtual void unacAllExtraDbPB_clicked();
    virtual void setStemLang(const QString& lang);
    virtual void editParaFormat();
    virtual void editHeaderText();
    virtual void extradDbSelectChanged();
    virtual void extraDbEditPtrans();
    virtual void resetShortcuts();
    
signals:
    void uiprefsDone();

protected slots:
    virtual void accept();
    virtual void reject();
private:
    void setupReslistFontPB();
    void readShortcuts();
    void storeShortcuts();
    void readShortcutsInternal(const QStringList&);
    void setSSButState();
    
    ViewAction *m_viewAction{nullptr};
    RclMain *m_mainWindow;

    // Locally stored data (pending ok/cancel), for the parameters for
    // which our UI state is not enough.
    QString paraFormat;
    QString headerText;
    std::vector<QString> m_scids;
    QString reslistFontFamily;
    int reslistFontSize;
    QString qssFile;
    bool darkMode{false};
    QString snipCssFile;
    QString synFile;
};

#endif /* _UIPREFS_W_H_INCLUDED_ */
