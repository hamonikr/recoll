/* Copyright (C) 2007-2019 J.F.Dockes
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
#ifndef _confgui_h_included_
#define _confgui_h_included_
/**
 * Utilities for a configuration/preferences settings user interface.
 * 
 * This file declares a number of data input Qt widgets (virtual base:
 * ConfParamW), with a well defined virtual interface to configuration storage,
 * which may be QSettings or something else (e.g. conftree).
 *
 * Subclasses are defined for entering different kind of data, e.g. a string,
 * a file name, an integer, etc.
 *
 * Each GUI object is linked to the configuration data through
 * a "link" object which knows the details of interacting with the actual
 * configuration data, like the parameter name, the actual
 * configuration interface, etc.
 *
 * The link object is set when the input widget is created and cannot be
 * changed.
 *
 * The link object get() methods are called for reading the initial data.
 *
 * The set() methods for all the objects are normally called when the
 * user clicks "Accept", only if the current value() differs from the
 * value obtained by get() when the object was initialized. This can
 * be used to avoid cluttering the output with values which are
 * unmodified from the defaults.
 *
 * The setImmediate() method can be called on the individial controls
 * to ensure that set() is inconditionnaly called whenever the user
 * changes the related value. This can be especially useful if the
 * configuration state can't be fully represented in the GUI (for
 * example if the same parameter name can exist in different sections
 * depending on the value of another parameter). It allows using local
 * storage for the values, and flushing, for example, when
 * sig_prefsChanged() is emitted after the user clicks accept.
 *
 * The file also defines a multi-tabbed dialog container for the
 * parameter objects, with simple interface methods to add tabs and add
 * configuration elements to them.
 *
 * Some of the tab widgets can be defined as "foreign", with specific internals.
 * They just need to implement a loadValues() to be called at
 * initialisation and a storeValues(), called when the user commits
 * the changes.
 */

#include <string>
#include <limits.h>

#include <memory>
#include <vector>
#include <string>

#include <QString>
#include <QWidget>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;

namespace confgui {

/** Interface between the GUI widget and the config storage mechanism: */
class ConfLinkRep {
public:
    virtual ~ConfLinkRep() {}
    virtual bool set(const std::string& val) = 0;
    virtual bool get(std::string& val) = 0;
};
typedef std::shared_ptr<ConfLinkRep> ConfLink;

// May be used to store/manage data which has no direct representation
// in the stored configuration.
class ConfLinkNullRep : public ConfLinkRep {
public:
    virtual ~ConfLinkNullRep() {}
    virtual bool set(const std::string&) {
        return true;
    }
    virtual bool get(std::string& val) {val = ""; return true;}
};

/** Link maker class. Will be called back by addParam() to create the link */
class ConfLinkFact {
public:
    virtual ~ConfLinkFact() {}
    virtual ConfLink operator()(const QString& nm) = 0;
};

/** Interface for "foreign" panels. The object must also be a QWidget, which
 * we don't express by inheriting here to avoid qt issues */
class ConfPanelWIF {
public:
    virtual ~ConfPanelWIF() {}
    virtual void storeValues() = 0;
    virtual void loadValues() = 0;
};

class ConfPanelW;
class ConfParamW;

/** The top level widget has tabs, each tab/panel has multiple widgets
 *  for setting parameter values */
class ConfTabsW : public QDialog {
    Q_OBJECT;

public:
    ConfTabsW(QWidget *parent, const QString& title, ConfLinkFact *linkfact);

    enum ParamType {CFPT_BOOL, CFPT_INT, CFPT_STR,
                    CFPT_CSTR, // Constrained string: from list
                    CFPT_FN, // File/directory
                    CFPT_STRL, CFPT_DNL, CFPT_CSTRL // lists of the same
    };

    /** Add tab and return its identifier / index */
    int addPanel(const QString& title);

    /** Add foreign tab where we only know to call loadvalues/storevalues.
     * The object has to derive from QWidget */
    int addForeignPanel(ConfPanelWIF* w, const QString& title);

    /** Add parameter setter to specified tab */
    ConfParamW *addParam(
        int tabindex, ParamType tp, const QString& varname, const QString& label,
        const QString& tooltip,
        int isdirorminval = 0, /* Dep. on type: directory flag or min value */
        int maxval = 0, const QStringList* sl = 0);

    /** Add explanatory text between 2 horizontal lines */
    QWidget *addBlurb(int tabindex, const QString& txt);
    
    /** Enable link between bool value and another parameter: the control will
     * be enabled depending on the boolean value (with possible inversion). Can
     * be called multiple times for the same bool to enable/disable
     * several controls */
    bool enableLink(ConfParamW* boolw, ConfParamW* otherw, bool revert = false);

    /** Call this when you are done filling up a tab */
    void endOfList(int tabindex);

    /** Find param widget associated with given variable name */
    ConfParamW *findParamW(const QString& varname);

    void hideButtons();
                      
public slots:
    void acceptChanges();
    void rejectChanges();
    void reloadPanels();
    void setCurrentIndex(int);
    
signals:
    /** This is emitted when acceptChanges() is called, after the
     *  values have been stored */
    void sig_prefsChanged();

private:
    ConfLinkFact *m_makelink{nullptr};
    // All ConfPanelW managed panels. Each has a load/store interface
    // and an internal list of controls
    std::vector<ConfPanelW *> m_panels;
    // "Foreign" panels. Just implement load/store
    std::vector<ConfPanelWIF *> m_widgets;

    QTabWidget       *tabWidget{nullptr};
    QDialogButtonBox *buttonBox{nullptr};
};

/////////////////////////////////////////////////
// The rest of the class definitions are only useful if you need to
// access a specific element for customisation (use findParamW() and a
// dynamic cast).

/** A panel/tab contains multiple controls for parameters */
class ConfPanelW : public QWidget {
    Q_OBJECT
public:
    ConfPanelW(QWidget *parent);
    void addParam(ConfParamW *w);
    void addWidget(QWidget *w);
    void storeValues();
    void loadValues();
    void endOfList();
    /** Find param widget associated with given variable name */
    ConfParamW *findParamW(const QString& varname);
private:
    QVBoxLayout *m_vboxlayout;
    std::vector<ConfParamW *> m_params;
};

/** Config panel element: manages one configuration
 *  parameter. Subclassed for specific parameter types.
 */
class ConfParamW : public QWidget {
    Q_OBJECT
public:
    ConfParamW(const QString& varnm, QWidget *parent, ConfLink cflink)
        : QWidget(parent), m_varname(varnm),
          m_cflink(cflink), m_fsencoding(false) {
    }
    virtual void loadValue() = 0;

    // Call setValue() each time the control changes, instead of on accept.
    virtual void setImmediate() = 0;
    
    virtual void setFsEncoding(bool onoff) {
        m_fsencoding = onoff;
    }
    const QString& getVarName() {
        return m_varname;
    }
public slots:
    virtual void setEnabled(bool) = 0;
    virtual void storeValue() = 0;

protected slots:
    void setValue(const QString& newvalue);
    void setValue(int newvalue);
    void setValue(bool newvalue);

protected:
    QString      m_varname;
    ConfLink     m_cflink;
    QHBoxLayout *m_hl;
    // File names are encoded as local8bit in the config files. Other
    // are encoded as utf-8
    bool         m_fsencoding;
    virtual bool createCommon(const QString& lbltxt, const QString& tltptxt);
};

//////// Widgets for setting the different types of configuration parameters:

/**  Boolean */
class ConfParamBoolW : public ConfParamW {
    Q_OBJECT
public:
    ConfParamBoolW(const QString& varnm, QWidget *parent, ConfLink cflink,
                   const QString& lbltxt,
                   const QString& tltptxt, bool deflt = false);
    virtual void loadValue();
    virtual void storeValue();
    virtual void setImmediate();
public slots:
    virtual void setEnabled(bool i) {
        if (m_cb) {
            ((QWidget*)m_cb)->setEnabled(i);
        }
    }
public:
    QCheckBox *m_cb;
    bool m_dflt;
    bool m_origvalue;
};

// Int
class ConfParamIntW : public ConfParamW {
    Q_OBJECT
public:
    // The default value is only used if none exists in the sample
    // configuration file. Defaults are normally set in there.
    ConfParamIntW(const QString& varnm, QWidget *parent, ConfLink cflink,
                  const QString& lbltxt,
                  const QString& tltptxt,
                  int minvalue = INT_MIN,
                  int maxvalue = INT_MAX,
                  int defaultvalue = 0);
    virtual void loadValue();
    virtual void storeValue();
    virtual void setImmediate();
public slots:
    virtual void setEnabled(bool i) {
        if (m_sb) {
            ((QWidget*)m_sb)->setEnabled(i);
        }
    }
protected:
    QSpinBox *m_sb;
    int       m_defaultvalue;
    int       m_origvalue;
};

// Arbitrary string
class ConfParamStrW : public ConfParamW {
    Q_OBJECT
public:
    ConfParamStrW(const QString& varnm, QWidget *parent, ConfLink cflink,
                  const QString& lbltxt,
                  const QString& tltptxt);
    virtual void loadValue();
    virtual void storeValue();
    virtual void setImmediate();
public slots:
    virtual void setEnabled(bool i) {
        if (m_le) {
            ((QWidget*)m_le)->setEnabled(i);
        }
    }
protected:
    QLineEdit *m_le;
    QString m_origvalue;
};

// Constrained string: choose from list
class ConfParamCStrW : public ConfParamW {
    Q_OBJECT
public:
    ConfParamCStrW(const QString& varnm, QWidget *parent, ConfLink cflink,
                   const QString& lbltxt,
                   const QString& tltptxt, const QStringList& sl);
    virtual void loadValue();
    virtual void storeValue();
    virtual void setList(const QStringList& sl);
    virtual void setImmediate();
public slots:
    virtual void setEnabled(bool i) {
        if (m_cmb) {
            ((QWidget*)m_cmb)->setEnabled(i);
        }
    }
protected:
    QComboBox *m_cmb;
    QString m_origvalue;
};

// File name
class ConfParamFNW : public ConfParamW {
    Q_OBJECT
public:
    ConfParamFNW(const QString& varnm, QWidget *parent, ConfLink cflink,
                 const QString& lbltxt,
                 const QString& tltptxt, bool isdir = false);
    virtual void loadValue();
    virtual void storeValue();
    virtual void setImmediate();
protected slots:
    void showBrowserDialog();
public slots:
    virtual void setEnabled(bool i) {
        if (m_le) {
            ((QWidget*)m_le)->setEnabled(i);
        }
        if (m_pb) {
            ((QWidget*)m_pb)->setEnabled(i);
        }
    }
protected:
    QLineEdit *m_le;
    QPushButton *m_pb;
    bool       m_isdir;
    QString m_origvalue;
};

// String list
class ConfParamSLW : public ConfParamW {
    Q_OBJECT
public:
    ConfParamSLW(const QString& varnm, QWidget *parent, ConfLink cflink,
                 const QString& lbltxt,
                 const QString& tltptxt);
    virtual void loadValue();
    virtual void storeValue();
    QListWidget *getListBox() {
        return m_lb;
    }
    virtual void setEditable(bool onoff);
    virtual void setImmediate() {
        m_immediate = true;
    }

public slots:
    virtual void setEnabled(bool i) {
        if (m_lb) {
            ((QWidget*)m_lb)->setEnabled(i);
        }
    }
protected slots:
    virtual void showInputDialog();
    void deleteSelected();
    void editSelected();
    void performInsert(const QString&);
signals:
    void entryDeleted(QString);
    void currentTextChanged(const QString&);
protected:
    QListWidget *m_lb;
    std::string listToString();
    std::string m_origvalue;
    QPushButton *m_pbE;
    bool m_immediate{false};
};

// Dir name list
class ConfParamDNLW : public ConfParamSLW {
    Q_OBJECT
public:
    ConfParamDNLW(const QString& varnm, QWidget *parent, ConfLink cflink,
                  const QString& lbltxt,
                  const QString& tltptxt)
        : ConfParamSLW(varnm, parent, cflink, lbltxt, tltptxt) {
        m_fsencoding = true;
    }
protected slots:
    virtual void showInputDialog();
};

// Constrained string list (chose from predefined)
class ConfParamCSLW : public ConfParamSLW {
    Q_OBJECT
public:
    ConfParamCSLW(const QString& varnm, QWidget *parent, ConfLink cflink,
                  const QString& lbltxt,
                  const QString& tltptxt,
                  const QStringList& sl)
        : ConfParamSLW(varnm, parent, cflink, lbltxt, tltptxt), m_sl(sl) {
    }
protected slots:
    virtual void showInputDialog();
protected:
    const QStringList m_sl;
};

extern void setSzPol(QWidget *w, QSizePolicy::Policy hpol,
                     QSizePolicy::Policy vpol,
                     int hstretch, int vstretch);

#ifdef ENABLE_XMLCONF
/**
 * Interpret an XML string and create a configuration interface. XML sample:
 *
 * <confcomments>
 *   <filetitle>Configuration file parameters for upmpdcli</filetitle>
 *   <grouptitle>MPD parameters</grouptitle>
 *   <var name="mpdhost" type="string">
 *     <brief>Host MPD runs on.</brief>
 *     <descr>Defaults to localhost. This can also be specified as -h</descr>
 *   </var>
 *   mpdhost = default-host
 *   <var name="mpdport" type="int" values="0 65635 6600">
 *     <brief>IP port used by MPD</brief>. 
 *     <descr>Can also be specified as -p port. Defaults to the...</descr>
 *   </var>
 *   mpdport = defport
 *   <var name="ownqueue" type="bool" values="1">
 *     <brief>Set if we own the MPD queue.</brief>
 *     <descr>If this is set (on by default), we own the MPD...</descr>
 *   </var>
 *   ownqueue = 
 * </confcomments>
 *
 * <grouptitle> creates a panel in which the following <var> are set.
 * The <var> attributes should be self-explanatory. "values"
 * is used for different things depending on the var type
 * (min/max, default, str list). Check the code about this. 
 * type values: "bool" "int" "string" "cstr" "cstrl" "fn" "dfn" "strl" "dnl"
 *
 * The XML would typically exist as comments inside a reference configuration
 * file (ConfSimple can extract such comments).
 *
 * This means that the reference configuration file can generate both
 * the documentation and the GUI interface.
 * 
 * @param xml the input xml
 * @param[output] toptxt the top level XML text (text not inside <var>, 
 *   normally commented variable assignments). This will be evaluated
 *   as a config for default values.
 * @lnkf factory to create the objects which link the GUI to the
 *   storage mechanism.
 */
extern ConfTabsW *xmlToConfGUI(const std::string& xml,
                               std::string& toptxt,
                               ConfLinkFact* lnkf,
                               QWidget *parent);
#endif

}

#endif /* _confgui_h_included_ */
