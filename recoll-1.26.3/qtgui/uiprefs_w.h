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

class QDialog;
class ViewAction;
class RclMain;

class UIPrefsDialog : public QDialog, public Ui::uiPrefsDialogBase
{
    Q_OBJECT

public:
    UIPrefsDialog(RclMain* parent)
	: QDialog((QWidget*)parent), m_mainWindow(parent)
	{
	    setupUi(this);
	    init();
	}
	~UIPrefsDialog(){};

    QString reslistFontFamily;
    int reslistFontSize;
    QString qssFile;
    QString snipCssFile;
    QString synFile;

    virtual void init();
    void setFromPrefs();

public slots:
    virtual void showFontDialog();
    virtual void resetReslistFont();
    virtual void showStylesheetDialog();
    virtual void showSynFileDialog();
    virtual void showSnipCssDialog();
    virtual void resetStylesheet();
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
    
signals:
    void uiprefsDone();

protected slots:
    virtual void accept();
    virtual void reject();
private:
    void setupReslistFontPB();
    // Locally stored data (pending ok/cancel)
    QString paraFormat;
    QString headerText;
    ViewAction *m_viewAction;
    RclMain *m_mainWindow;
};

#endif /* _UIPREFS_W_H_INCLUDED_ */
