/* Copyright (C) 2005-2020 J.F.Dockes
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
#include "picoxml.h"

using namespace std;

class FragButsParser : public PicoXMLParser {
public:
    FragButsParser(
        const std::string& in, FragButs *_p, vector<FragButs::ButFrag>& _bts)
        : PicoXMLParser(in), parent(_p), vlw(new QVBoxLayout(parent)),
        vl(new QVBoxLayout()), buttons(_bts) {}

    void startElement(const std::string &nm,
                      const std::map<std::string, std::string>&) override {
        std::cerr << "startElement [" << nm << "]\n";
        currentText.clear();
        if (nm == "buttons") {
            radio = false;
            hl = new QHBoxLayout();
        } else if (nm == "radiobuttons") {
            radio = true;
            bg = new QButtonGroup(parent);
            hl = new QHBoxLayout();
        } else if (nm == "label" || nm == "frag" || nm == "fragbuts" ||
                   nm == "fragbut") {
        } else {
            QMessageBox::warning(
                0, "Recoll", QString("Bad element name: [%1]").arg(nm.c_str()));
        }
    }        
    void endElement(const std::string& nm) override {
        std::cerr << "endElement [" << nm << "]\n";

        if (nm == "label") {
            label = u8s2qs(currentText);
        } else if (nm == "frag") {
            frag = currentText;
        } else if (nm == "fragbut") {
            string slab = qs2utf8s(label);
            trimstring(slab, " \t\n\t");
            label = u8s2qs(slab.c_str());
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
            abut->setToolTip(u8s2qs(currentText));
            buttons.push_back(FragButs::ButFrag(abut, frag));
            hl->addWidget(abut);
        } else if (nm == "buttons" || nm == "radiobuttons") {
            vl->addLayout(hl);
            hl = 0;
        } else if (nm == "fragbuts") {
            vlw->addLayout(vl);
        } else {
            QMessageBox::warning(
                0, "Recoll", QString("Bad element name: [%1]").arg(nm.c_str()));
        }
    }
    void characterData(const std::string &str) override {
        std::cerr << "characterData [" << str << "]\n";
        currentText += str;
    }

private:
    QWidget *parent;
    QVBoxLayout *vlw;
    QVBoxLayout *vl;
    vector<FragButs::ButFrag>& buttons;

    // Temporary data while parsing.
    QHBoxLayout *hl{nullptr};
    QButtonGroup *bg{nullptr};
    QString label;
    std::string currentText;
    std::string frag;
    bool radio{false};
};


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
        QMessageBox::warning(
            0, "Recoll", tr("%1 not found.").arg(path2qs(m_fn)));
        LOGERR("Fragbuts:: can't read [" << m_fn << "]\n");
        return;
    }
    FragButsParser parser(data, this, m_buttons);
    if (!parser.Parse()) {
        QMessageBox::warning(
            0, "Recoll", tr("%1:\n %2").arg(path2qs(m_fn))
            .arg(u8s2qs(parser.getReason())));
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

