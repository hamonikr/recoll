/* Copyright (C) 2021 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _SCBASE_H_INCLUDED_
#define _SCBASE_H_INCLUDED_

#include <QString>
#include <QKeySequence>
#include <QStringList>
#include <QObject>

/** Shortcuts storage classe. Singleton. 
 *
 * Manage settings storage for key sequences shortcuts.
 * Each shortcut is defined by 4 strings: 
 *  - Context  (e.g. "Main Window").
 *  - Description (e.g. "Move focus to search entry").
 *  - Current value, possibly changed by user, e.g. "Ctrl+l".
 *  - Default value.
 *
 * The customised values are read from the stored settings by the SCBase 
 * constructor.
 * The entries with default values are created from the init() method
 * of each class responsible for a context (e.g. RclMain, SnippetsW),
 * or from a static method for classes which are not instantiated when
 * the program starts up. 
 *
 * Macros are provided for actually creating the shortcuts in the
 * init() routines, or for just creating the default entries (for use
 * in the preferences screen).
 */
class SCBase : public QObject {
    Q_OBJECT;
public:
    ~SCBase();

    /* Return a reference to the instantiated singleton */
    static SCBase& scBase();

    /** Get the current keysequence for the shortcut. If the entry was not
     * created from the settings, create it with the default
     * sequence. This is called from the context classes and returns
     * either the default or the customised sequence. */
    QKeySequence get(const QString& id, const QString& context,
                     const QString& description, const QString& defkeyseq);

    /** Set a customised value for the designated shortcut. Called
     * from the preference code. */
    void set(const QString& id, const QString& context,
             const QString& description, const QString& keyseq);

    /** Return a list of all shortcuts. This is used to create the
     *  preferences table. Each entry in the list is a string
     *  tuple: id, context, description, value, default */
    QStringList getAll();

    /** Return a list of all shortcuts, with only default values (no settings).
     * Used for resetting the defaults, especially if a lang changed
     * has messed up the keys */
    QStringList getAllDefaults();

    /** Store the customised values to the settings storage. Called
     * from the preferences accept() method. */
    void store();

    class Internal;

signals:

    /** Preference change has been accepted and client classes should
     * update their shortcuts */
    void shortcutsChanged();
    
private:
    Internal *m{nullptr};
    SCBase();
};

/** This can be used in the client class init method, to actually
 * create and connect the shortcuts. */
#define SETSHORTCUT(OBJ, ID, CTXT, DESCR, SEQ, FLD, SLTFUNC)            \
    do {                                                                \
        QKeySequence ks = SCBase::scBase().get(ID, CTXT, DESCR, SEQ);   \
        if (!ks.isEmpty()) {                                            \
            delete FLD;                                                 \
            FLD = new QShortcut(ks, OBJ, SLOT(SLTFUNC()));              \
        }                                                               \
    } while (false);

/** This can be used from a static method, to be called by the program
 * initialisation, for classes which are not instantiated at startup,
 * and so that the shortcuts are available for the preferences
 * customisation screen. Same param list as SETSHORTCUT to make it
 * easy to duplicate a list of ones into the other, even if some
 * parameters are not used here. */
#define LISTSHORTCUT(OBJ, ID, CTXT, DESCR, SEQ, FLD, SLTFUNC)       \
    do {                                                            \
        SCBase::scBase().get(ID, CTXT, DESCR, SEQ);                 \
    } while (false);


#endif /* _SCBASE_H_INCLUDED_ */
