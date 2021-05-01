/* Copyright (C) 2005 J.F.Dockes
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

#include "safesysstat.h"

#include <string>
#include <vector>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMessageBox>
#include <QXmlDefaultHandler>

#include "fragbuts.h"
#include "pathut.h"
#include "smallut.h"
#include "recoll.h"
#include "log.h"
#include "readfile.h"
#include "copyfile.h"

using namespace std;

class FragButsParser : public QXmlDefaultHandler {
public:
    FragButsParser(FragButs *_parent, vector<FragButs::ButFrag>& _buttons)
    : parent(_parent), vlw(new QVBoxLayout(parent)), 
      vl(new QVBoxLayout()), buttons(_buttons),
      hl(0), bg(0), radio(false)
    {
    }

    bool startElement(const QString & /* namespaceURI */,
		      const QString & /* localName */,
		      const QString &qName,
		      const QXmlAttributes &attributes);
    bool endElement(const QString & /* namespaceURI */,
		    const QString & /* localName */,
		    const QString &qName);
    bool characters(const QString &str)
    {
	currentText += str;
	return true;
    }

    bool error(const QXmlParseException& exception) {
        fatalError(exception);
        return false;
    }
    bool fatalError(const QXmlParseException& x) {
        errorMessage = QString("%2 at line %3 column %4")
            .arg(x.message())
            .arg(x.lineNumber())
            .arg(x.columnNumber());
        return false;
    }

    QString errorMessage;
private:
    QWidget *parent;
    QVBoxLayout *vlw;
    QVBoxLayout *vl;
    vector<FragButs::ButFrag>& buttons;

    // Temporary data while parsing.
    QHBoxLayout *hl;
    QButtonGroup *bg;
    QString currentText;
    QString label;
    string frag;
    bool radio;
};

bool FragButsParser::startElement(const QString & /* namespaceURI */,
                                  const QString & /* localName */,
                                  const QString &qName,
                                  const QXmlAttributes &/*attributes*/)
{
    currentText = "";
    if (qName == "buttons") {
        radio = false;
        hl = new QHBoxLayout();
    } else if (qName == "radiobuttons") {
        radio = true;
        bg = new QButtonGroup(parent);
        hl = new QHBoxLayout();
    }
    return true;
}

bool FragButsParser::endElement(const QString & /* namespaceURI */,
                                const QString & /* localName */,
                                const QString &qName)
{
    if (qName == "label") {
        label = currentText;
    } else if (qName == "frag") {
        frag = qs2utf8s(currentText);
    } else if (qName == "fragbut") {
        string slab = qs2utf8s(label);
        trimstring(slab, " \t\n\t");
        label = QString::fromUtf8(slab.c_str());
        QAbstractButton *abut;
        if (radio) {
            QRadioButton *but = new QRadioButton(label, parent);
            bg->addButton(but);
            if (bg->buttons().length() == 1)
                but->setChecked(true);
            abut = but;
        } else {
            QCheckBox *but = new QCheckBox(label, parent);
            abut = but;
        }
	abut->setToolTip(currentText);
        buttons.push_back(FragButs::ButFrag(abut, frag));
        hl->addWidget(abut);
    } else if (qName == "buttons" || qName == "radiobuttons") {
        vl->addLayout(hl);
        hl = 0;
    } else if (qName == "fragbuts") {
        vlw->addLayout(vl);
    }
    return true;
}

FragButs::FragButs(QWidget* parent)
    : QWidget(parent), m_reftime(0), m_ok(false)
{
    m_fn = path_cat(theconfig->getConfDir(), "fragbuts.xml");

    string data, reason;
    if (!path_exists(m_fn)) {
        // config does not exist: try to create it from sample
        string src = path_cat(theconfig->getDatadir(), "examples");
        src = path_cat(src, "fragbuts.xml");
        copyfile(src.c_str(), m_fn.c_str(), reason);
    }
    if (!file_to_string(m_fn, data, &reason)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("%1 not found.").arg(
                                 QString::fromLocal8Bit(m_fn.c_str())));
        LOGERR("Fragbuts:: can't read ["  << (m_fn) << "]\n" );
        return;
    }
    FragButsParser parser(this, m_buttons);
    QXmlSimpleReader reader;
    reader.setContentHandler(&parser);
    reader.setErrorHandler(&parser);
    QXmlInputSource xmlInputSource;
    xmlInputSource.setData(QString::fromUtf8(data.c_str()));
    if (!reader.parse(xmlInputSource)) {
	QMessageBox::warning(0, "Recoll", tr("%1:\n %2")
                             .arg(QString::fromLocal8Bit(m_fn.c_str()))
                             .arg(parser.errorMessage));
        return;
    }
    for (vector<ButFrag>::iterator it = m_buttons.begin(); 
         it != m_buttons.end(); it++) {
        connect(it->button, SIGNAL(clicked(bool)), 
                this, SLOT(onButtonClicked(bool)));
    }
    setWindowTitle(tr("Query Fragments"));
    isStale(&m_reftime);
    m_ok = true;
}

FragButs::~FragButs()
{
}

bool FragButs::isStale(time_t *reftime)
{
    struct stat st;
    stat(m_fn.c_str(), &st);
    bool ret = st.st_mtime != m_reftime;
    if (reftime)
        *reftime = st.st_mtime;
    return ret;
}

void FragButs::onButtonClicked(bool on)
{
    LOGDEB("FragButs::onButtonClicked: ["  << (int(on)) << "]\n" );
    emit fragmentsChanged();
}

void FragButs::getfrags(std::vector<std::string>& frags)
{
    for (vector<ButFrag>::iterator it = m_buttons.begin(); 
         it != m_buttons.end(); it++) {
        if (it->button->isChecked() && !it->fragment.empty()) {
            LOGDEB("FragButs: fragment ["  << (it->fragment) << "]\n" );
            frags.push_back(it->fragment);
        }
    }
}

