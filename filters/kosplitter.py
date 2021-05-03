#!/usr/bin/python3
#################################
# Copyright (C) 2020 J.F.Dockes
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
########################################################

#
# Interface to the konlpy Korean text analyser: we receive text from
# our parent process and have it segmented by the analyser, then
# return the results. The analyser startup is very expensive (several
# seconds), which is why we can't just execute it from the main
# process.
#

import sys
import cmdtalk

# We can either use konlpy, which supports different analysers, or use
# the python-mecab-ko, a direct interface to mecab, with the same
# interface as konlpy https://pypi.org/project/python-mecab-ko/
try:
    import mecab
    usingkonlpy = False
except:
    import konlpy.tag
    usingkonlpy = True

class Processor(object):
    def __init__(self, proto):
        self.proto = proto
        self.tagsOkt = False
        self.tagsMecab = False
        self.tagsKomoran = False

    def _init_tagger(self, taggername):
        global usingkonlpy
        if not usingkonlpy and taggername != "Mecab":
            from konlpy.tag import Okt,Mecab,Komoran
            usingkonlpy = True
        if taggername == "Okt":
            self.tagger = konlpy.tag.Okt()
            self.tagsOkt = True
        elif taggername == "Mecab":
            if usingkonlpy:
                # Use Mecab(dicpath="c:/some/path/mecab-ko-dic") for a
                # non-default location. (?? mecab uses rcfile and dicdir not
                # dicpath)
                self.tagger = konlpy.tag.Mecab()
            else:
                self.tagger = mecab.MeCab()
            self.tagsMecab = True
        elif taggername == "Komoran":
            self.tagger = konlpy.tag.Komoran()
            self.tagsKomoran = True
        else:
            raise Exception("Bad tagger name " + taggername)
        
    def process(self, params):
        if 'data' not in params:
            return {'error':'No data field in parameters'}
        if not (self.tagsOkt or self.tagsMecab or self.tagsKomoran):
            if 'tagger' not in params:
                return {'error':'No "tagger" field in parameters'}
            self._init_tagger(params['tagger']);

        spliteojeol = False
        if spliteojeol:
            data = params['data'].split()
            pos = []
            for d in data:
                pos += self.tagger.pos(d)
        else:
            pos = self.tagger.pos(params['data'])
            
        #proto.log("POS: %s" % pos)
        text = ""
        tags = ""
        for e in pos:
            word = e[0]
            word = word.replace('\t', ' ')
            text += word + "\t"
            tag = e[1]
            if self.tagsOkt:
                pass
            elif self.tagsMecab or self.tagsKomoran:
                tb = tag[0:2]
                if tb[0] == "N":
                    tag = "Noun"
                elif tb == "VV":
                    tag = "Verb"
                elif tb == "VA":
                    tag = "Adjective"
                elif tag == "MAG":
                    tag = "Adverb"
            else:
                pass
            tags += tag + "\t"
        return {'text': text, 'tags': tags}


proto = cmdtalk.CmdTalk()
processor = Processor(proto)
cmdtalk.main(proto, processor)
