#!/usr/bin/env python3
"""Try to guess a text's language and character set by checking how it matches lists of
common words. This is not a primary method of detection because it's slow and unreliable, but it
may be a help in discrimating, for exemple, before european languages using relatively close
variations of iso-8859.
This is used in association with a zip file containing a number of stopwords list: rcllatinstops.zip

As a note, I am looking for a good iso-8859-7 stop words list for greek, the only ones I found
were utf-8 and there are errors when transcoding to iso-8859-7. I guess that there is something
about Greek accents that I don't know and would enable fixing this (some kind of simplification
allowing transliteration from utf-8 to iso-8859-7). An exemple of difficulty is the small letter
epsilon with dasia (in unicode but not iso). Can this be replaced by either epsilon or epsilon
with acute accent ?
"""

from __future__ import print_function

import sys
PY3 = sys.version > '3'
if not PY3:
    import string
import glob
import os
import os.path
from zipfile import ZipFile


class European8859TextClassifier:
    def __init__(self, langzip=""):
        """langzip contains text files. Each text file is named like lang_code.txt
        (ie: french_cp1252.txt) and contains an encoded stop word list for the language"""

        if langzip == "":
            langzip = os.path.join(os.path.dirname(__file__), 'rcllatinstops.zip')
            
        self.readlanguages(langzip)

        # Table to translate from punctuation to spaces
        self.punct = b'''0123456789<>/*?[].@+-,#_$%&={};.,:!"''' + b"'\n\r"
        spaces = len(self.punct) * b' '
        if PY3:
            self.spacetable = bytes.maketrans(self.punct, spaces)
        else:
            self.spacetable = string.maketrans(self.punct, spaces)

    def readlanguages(self, langzip):
        """Extract the stop words lists from the zip file.
        We build a merge dictionary from the lists.
        The keys are the words from all the files. The
        values are a list of the (lang,code) origin(s) for the each word.
        """
        zip = ZipFile(langzip)
        langfiles = zip.namelist()
        self.allwords = {}
        for fn in langfiles:
            langcode = os.path.basename(fn)
            langcode = os.path.splitext(langcode)[0]
            (lang,code) = langcode.split('_')
            text = zip.read(fn)
            words = text.split()
            for word in words:
                if word in self.allwords:
                    self.allwords[word].append((lang, code))
                else:
                    self.allwords[word] = [(lang, code)]

    def classify(self, rawtext):
        # Note: we can't use an re-based method to split the data because it
        # should be considered binary, not text.

        # Limit to reasonable size.
        if len(rawtext) > 10000:
            i = rawtext.find(b' ', 9000)
            if i == -1:
                i = 9000
            rawtext = rawtext[0:i]

        # Remove punctuation
        rawtext = rawtext.translate(self.spacetable)

        # Make of list of all text words, order it by frequency, we only
        # use the ntest most frequent words.
        ntest = 20
        words = rawtext.split()
        dict = {}
        for w in words:
            dict[w] = dict.get(w, 0) + 1
        lfreq = [a[0] for a in sorted(dict.items(), \
                       key=lambda entry: entry[1], reverse=True)[0:ntest]]
        #print(lfreq)

        # Build a dict (lang,code)->matchcount
        langstats = {}
        for w in lfreq:
            lcl = self.allwords.get(w, [])
            for lc in lcl:
                langstats[lc] = langstats.get(lc, 0) + 1

        # Get a list of (lang,code) sorted by match count
        lcfreq = sorted(langstats.items(), \
                        key=lambda entry: entry[1], reverse=True)
        #print(lcfreq[0:3])
        if len(lcfreq) != 0:
            lc,maxcount = lcfreq[0]
            maxlang = lc[0]
            maxcode = lc[1]
        else:
            maxcount = 0

        # If the match is too bad, default to most common. Maybe we should
        # generate an error instead, but the caller can look at the count
        # anyway.
        if maxcount == 0:
            maxlang,maxcode = ('english', 'cp1252')

        return (maxlang, maxcode, maxcount)


if __name__ == "__main__":
    f = open(sys.argv[1], "rb")
    rawtext = f.read()
    f.close()

    classifier = European8859TextClassifier()

    lang,code,count = classifier.classify(rawtext)
    if count > 0:
        print("%s %s %d" % (code, lang, count))
    else:
        print("UNKNOWN UNKNOWN 0")
        
