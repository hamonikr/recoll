#!/usr/bin/env python3
"""Extract Html content from an EPUB file (.chm), concatenating all sections"""
from __future__ import print_function

import sys
import os
import re

import rclexecm
from rclbasehandler import RclBaseHandler

sys.path.append(sys.path[0]+"/recollepub.zip")
try:
    import epub
except:
    print("RECFILTERROR HELPERNOTFOUND python3:epub")
    sys.exit(1);

class EPUBConcatExtractor(RclBaseHandler):
    """RclExecM slave worker for extracting all text from an EPUB
    file. This version concatenates all nodes."""

    def __init__(self, em):
        super(EPUBConcatExtractor, self).__init__(em)

    def _docheader(self):
        meta = self.book.opf.metadata
        title = ""
        for tt, lang in meta.titles:
            title += tt + " "
        author = ""
        for name, role, fileas in meta.creators:
            author += name + " "
        data = "<html>\n<head>\n"
        if title:
            data += "<title>" + rclexecm.htmlescape(title) + "</title>\n"
        if author:
            data += '<meta name="author" content="' + \
                rclexecm.htmlescape(author).strip() + '">\n'
        if meta.description:
            data += '<meta name="description" content="' + \
                rclexecm.htmlescape(meta.description) + '">\n'
        for value in meta.subjects:
            data += '<meta name="dc:subject" content="' + \
                rclexecm.htmlescape(value) + '">\n' 
        data += "</head>"
        return data.encode('UTF-8')

    def _catbodies(self):
        data = b'<body>'
        ids = []
        if self.book.opf.spine:
            for id, linear in self.book.opf.spine.itemrefs:
                ids.append(id)
        else:
            for id, item in self.book.opf.manifest.items():
                ids.append(id)

        for id in ids:
            item = self.book.get_item(id)
            if item is None or item.media_type != 'application/xhtml+xml':
                continue
            doc = self.book.read_item(item)
            doc = re.sub(b'''<\?.*\?>''', b'', doc)
            doc = re.sub(b'''<html.*<body[^>]*>''',
                         b'', doc, 1, flags=re.DOTALL|re.I)
            doc = re.sub(b'''</body>''', b'', doc, flags=re.I)
            doc = re.sub(b'''</html>''', b'', doc, flags=re.I)
            data += doc

        data += b'</body></html>'
        return data
        
    def html_text(self, fn):
        """Extract EPUB data as concatenated HTML"""

        f = open(fn, 'rb')
        self.book = epub.open_epub(f)
        data = self._docheader()
        data += self._catbodies()
        self.book.close()
        return data

proto = rclexecm.RclExecM()
extract = EPUBConcatExtractor(proto)
rclexecm.main(proto, extract)
