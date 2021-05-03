#!/usr/bin/env python
from __future__ import print_function

import sys
import datetime
try:
    from recoll import recoll
    from recoll import rclextract
    hasextract = True
except:
    import recoll
    hasextract = False

import rclmain
from getopt import getopt

from PyQt5 import QtCore
from PyQt5.QtCore import pyqtSlot
from PyQt5.QtGui import QKeySequence
from PyQt5.QtWidgets import *

####################
# Highlighting methods. Just for showing the groups usage, we add the
# original string for the match to the highlighted text. I don't think
# you'd want to do this in a real app, but maybe some kind of tooltip?
class HlMeths:
    def __init__(self, groups):
        self.groups = groups

    def startMatch(self, idx):
        ugroup = " ".join(self.groups[idx][0])
        return '<font size="tiny">'+ugroup+'</font><font color="blue">'

    def endMatch(self):
        return '</font>'

############
# Data extraction. The 2 following methods use the extractor module
# and get the data from the original document
#
# Extract and return document text (in text or html format, indicated
# by newdoc.mimetype)
def textextract(doc):
    extractor = rclextract.Extractor(doc)
    newdoc = extractor.textextract(doc.ipath)
    return newdoc
# Extract document in original format (ie: application/msword) and
# save it to a file. This only works if ipath is not null (else just
# use the url !)
def extractofile(doc, outfilename=""):
    extractor = rclextract.Extractor(doc)
    outfilename = extractor.idoctofile(doc.ipath, doc.mimetype, \
                                       ofilename=outfilename)
    return outfilename

#########
# RecollQuery wraps a recoll.query object in a Qt model
class RecollQuery(QtCore.QAbstractTableModel):
    def __init__(self):
        QtCore.QAbstractTableModel.__init__(self)
        self.totres = -1
        self.db = None
        self.query = None
        self.qtext = ""
        self.docs = []
        self.pagelen = 10
        self.attrs = ("filename", "title", "mtime", "url", "ipath")

    def rowCount(self, parent):
        ret = len(self.docs)
        #print("RecollQuery.rowCount(): %d"% ret)
        return ret

    def columnCount(self, parent):
        #print("RecollQuery.columnCount()")
        if parent.isValid():
            return 0
        else:
            return len(self.attrs)

    def setquery(self, db, q, sortfield="", ascending=True):
        """Parse and execute query on open db"""
        #print("RecollQuery.setquery():")
        # Get query object
        self.query = db.query()
        if sortfield:
            self.query.sortby(sortfield, ascending)
        # Parse/run input query string
        self.totres = self.query.execute(q)
        self.qtext = q
        self.db = db
        self.docs = []
        self.fetchMore(None)

    def getdoc(self, index):
        if index.row() < len(self.docs):
            return self.docs[index.row()]
        else:
            return None

    def sort(self, col, order):
        #print("sort %s %s", (col, order))
        self.setquery(self.db, self.qtext, sortfield=self.attrs[col],
                      ascending = order)

    def headerData(self, idx, orient, role):
        if orient == QtCore.Qt.Horizontal and role == QtCore.Qt.DisplayRole:
            return self.attrs[idx]
        return None

    def data(self, index, role):
        #print("RecollQuery.data: row %d, role: %s" % (index.row(),role))
        if not index.isValid():
            return QtCore.QVariant()

        if index.row() >= len(self.docs):
            return QtCore.QVariant()

        if role == QtCore.Qt.DisplayRole:
            #print("RecollQuery.data: row %d, col %d role: %s" % \
            #     (index.row(), index.column() role))
            attr = self.attrs[index.column()]
            value = getattr(self.docs[index.row()], attr)
            if attr == "mtime":
                dte = datetime.datetime.fromtimestamp(int(value))
                value = str(dte)
            return value
        else:
            return QtCore.QVariant()

    def canFetchMore(self, parent):
        #print("RecollQuery.canFetchMore:")
        if len(self.docs) < self.totres:
            return True
        else:
            return False

    def fetchMore(self, parent):
        #print("RecollQuery.fetchMore:")
        self.beginInsertRows(QtCore.QModelIndex(), len(self.docs), \
                             len(self.docs) + self.pagelen)
        for count in range(self.pagelen):
            try:
                self.docs.append(self.query.fetchone())
            except:
                break
        self.endInsertRows()


###
#  UI interaction code
class RclGui_Main(QMainWindow):
    def __init__(self, db, parent=None):
        QMainWindow.__init__(self, parent)
        self.ui = rclmain.Ui_MainWindow()
        self.ui.setupUi(self)
        self.db = db
        self.qmodel = RecollQuery()
        scq = QShortcut(QKeySequence("Ctrl+Q"), self)
        scq.activated.connect(self.onexit)
        header = self.ui.resTable.horizontalHeader()
        header.setSortIndicatorShown(True)
        header.setSortIndicator(-1, QtCore.Qt.AscendingOrder)
        self.ui.resTable.setSortingEnabled(True)
        self.currentindex = -1
        self.currentdoc = None
        
    def on_searchEntry_returnPressed(self):
        self.startQuery()

    def on_resTable_clicked(self, index):
        doc = self.qmodel.getdoc(index)
        self.currentindex = index
        self.currentdoc = doc
        if doc is None:
            print("NO DoC")
            return
        query = self.qmodel.query
        groups = query.getgroups()
        meths = HlMeths(groups)
        abs = query.makedocabstract(doc, methods=meths)
        self.ui.resDetail.setText(abs)
        if hasextract:
            ipath = doc.get('ipath')
            #print("ipath[%s]" % ipath)
            self.ui.previewPB.setEnabled(True)
            if ipath:
                self.ui.savePB.setEnabled(True)
            else:
                self.ui.savePB.setEnabled(False)

    @pyqtSlot()
    def on_previewPB_clicked(self):
        print("on_previewPB_clicked(self)")
        newdoc = textextract(self.currentdoc)
        query = self.qmodel.query;
        groups = query.getgroups()
        meths = HlMeths(groups)
        #print("newdoc.mimetype:", newdoc.mimetype)
        if newdoc.mimetype == 'text/html':
            ishtml = True
        else:
            ishtml = False
        text = '<qt><head></head><body>' + \
               query.highlight(newdoc.text,
                               methods=meths,
                               ishtml=ishtml,
                               eolbr=True)
        text += '</body></qt>'
        self.ui.resDetail.setText(text)

    @pyqtSlot()
    def on_savePB_clicked(self):
        print("on_savePB_clicked(self)")
        doc = self.currentdoc
        ipath = doc.ipath
        if not ipath:
            return
        fn = QFileDialog.getSaveFileName(self)
        if fn:
            docitems = doc.items()
            fn = extractofile(doc, str(fn.toLocal8Bit()))
            print("Saved as %s" % fn)
        else:
            print("Canceled", file=sys.stderr)
                
    def startQuery(self):
        self.qmodel.setquery(self.db, self.ui.searchEntry.text())
        self.ui.resTable.setModel(self.qmodel)

    def onexit(self):
        self.close()


def Usage():
    print('''Usage: qt.py [<qword1> [<qword2> ...]]''', file=sys.stderr)
    sys.exit(1)


def main(args):

    app = QApplication(args)

    confdir=""
    extra_dbs = []
    # Snippet params
    maxchars = 300
    contextwords = 6

    # Process options: [-c confdir] [-i extra_db [-i extra_db] ...]
    options, args = getopt(args[1:], "c:i:")
    for opt,val in options:
        if opt == "-c":
            confdir = val
        elif opt == "-i":
            extra_dbs.append(val)
        else:
            print("Bad opt: %s"% opt, file=sys.stderr) 
            Usage()

    # The query should be in the remaining arg(s)
    q = None
    if len(args) > 0:
        q = ""
        for word in args:
            q += word + " "

    db = recoll.connect(confdir=confdir, extra_dbs=extra_dbs)
    db.setAbstractParams(maxchars=maxchars, contextwords=contextwords)

    topwindow = RclGui_Main(db)
    topwindow.show()
    if q is not None:
        topwindow.ui.searchEntry.setText(q)
        topwindow.startQuery()
    
    sys.exit(app.exec_())


if __name__=="__main__":
    main(sys.argv)
