#!/usr/bin/env python3

# Python-based Image Tag extractor for Recoll. This is less thorough
# than the Perl-based rclimg script, but useful if you don't want to
# have to install Perl (e.g. on Windows).
#
# Uses pyexiv2. Also tried Pillow, found it useless for tags.
#
from __future__ import print_function

import sys
import os
import rclexecm
import re
from rclbasehandler import RclBaseHandler

try:
    import pyexiv2
except:
    print("RECFILTERROR HELPERNOTFOUND python3:pyexiv2")
    sys.exit(1);

khexre = re.compile('.*\.0[xX][0-9a-fA-F]+$')

pyexiv2_titles = {
    'Xmp.dc.subject',
    'Xmp.lr.hierarchicalSubject',
    'Xmp.MicrosoftPhoto.LastKeywordXMP',
    }

# Keys for which we set meta tags
meta_pyexiv2_keys = {
    'Xmp.dc.subject',
    'Xmp.lr.hierarchicalSubject',
    'Xmp.MicrosoftPhoto.LastKeywordXMP',
    'Xmp.digiKam.TagsList',
    'Exif.Photo.DateTimeDigitized',
    'Exif.Photo.DateTimeOriginal',
    'Exif.Image.DateTime',
    }

exiv2_dates = ['Exif.Photo.DateTimeOriginal',
               'Exif.Image.DateTime', 'Exif.Photo.DateTimeDigitized']

class ImgTagExtractor(RclBaseHandler):
    def __init__(self, em):
        super(ImgTagExtractor, self).__init__(em)

    def html_text(self, filename):
        ok = False

        metadata = pyexiv2.ImageMetadata(filename)
        metadata.read()
        keys = metadata.exif_keys + metadata.iptc_keys + metadata.xmp_keys
        mdic = {}
        for k in keys:
            # we skip numeric keys and undecoded makernote data
            if k != 'Exif.Photo.MakerNote' and not khexre.match(k):
                mdic[k] = str(metadata[k].raw_value)

        docdata = b'<html><head>\n'

        ttdata = set()
        for k in pyexiv2_titles:
            if k in mdic:
                ttdata.add(self.em.htmlescape(mdic[k]))
        if ttdata:
            title = ""
            for v in ttdata:
                v = v.replace('[', '').replace(']', '').replace("'", "")
                title += v + " "
            docdata += rclexecm.makebytes("<title>" + title + "</title>\n")

        for k in exiv2_dates:
            if k in mdic:
                # Recoll wants: %Y-%m-%d %H:%M:%S.
                # We get 2014:06:27 14:58:47
                dt = mdic[k].replace(":", "-", 2)
                docdata += b'<meta name="date" content="' + \
                           rclexecm.makebytes(dt) + b'">\n'
                break

        for k,v in mdic.items():
            if k ==  'Xmp.digiKam.TagsList':
                docdata += b'<meta name="keywords" content="' + \
                           rclexecm.makebytes(self.em.htmlescape(mdic[k])) + \
                           b'">\n'

        docdata += b'</head><body>\n'
        for k,v in mdic.items():
            docdata += rclexecm.makebytes(k + " : " + \
                                     self.em.htmlescape(mdic[k]) + "<br />\n")
        docdata += b'</body></html>'

        return docdata


if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = ImgTagExtractor(proto)
    rclexecm.main(proto, extract)
