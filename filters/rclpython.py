#!/usr/bin/python3

# Rclpython is based on "colorize.py" from:
# http://chrisarndt.de/en/software/python/colorize.html
#
# Based on the code from Jürgen Herman, the following changes where made:
#
# Mike Brown <http://skew.org/~mike/>:
# - make script callable as a CGI and a Apache handler for .py files.
#
# Christopher Arndt <http://chrisarndt.de>:
# - make script usable as a module
# - use class tags and style sheet instead of <style> tags
# - when called as a script, add HTML header and footer
#
# TODO:
#
# - parse script encoding and allow output in any encoding by using unicode
#   as intermediate


__version__ = '0.3'
__date__ = '2005-07-04'
__license__ = 'GPL'

__author__ = 'Jürgen Hermann, Mike Brown, Christopher Arndt'

# Imports
import rclexecm
from rclbasehandler import RclBaseHandler
import string, sys, os
import html
import io
import keyword, token, tokenize

#############################################################################
### Python Source Parser (does Highlighting)
#############################################################################

_KEYWORD = token.NT_OFFSET + 1
_TEXT    = token.NT_OFFSET + 2

_css_classes = {
    token.NUMBER:       'number',
    token.OP:           'operator',
    token.STRING:       'string',
    tokenize.COMMENT:   'comment',
    token.NAME:         'name',
    token.ERRORTOKEN:   'error',
    _KEYWORD:           'keyword',
    _TEXT:              'text',
}

_HTML_HEADER = """\
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
  "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
  <title>%%(title)s</title>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta name="Generator" content="colorize.py (version %s)">
</head>
<body>
""" % __version__

_HTML_FOOTER = """\
</body>
</html>
"""

_STYLESHEET = """\
<style type="text/css">
pre.code {
    font-style: Lucida,"Courier New";
}

.number {
    color: #0080C0;
}
.operator {
    color: #000000;
}
.string {
    color: #008000;
}
.comment {
    color: #808080;
}
.name {
    color: #000000;
}
.error {
    color: #FF8080;
    border: solid 1.5pt #FF0000;
}
.keyword {
    color: #0000FF;
    font-weight: bold;
}
.text {
    color: #000000;
}

</style>

"""

class Parser:
    """ Send colored python source.
    """

    stylesheet = _STYLESHEET

    def __init__(self, raw, out=sys.stdout):
        """ Store the source text.
        """
        self.raw = raw.expandtabs().strip()
        self.out = out

    def format(self):
        """ Parse and send the colored source.
        """
        # store line offsets in self.lines
        self.lines = [0, 0]
        pos = 0
        while 1:
            pos = self.raw.find(b'\n', pos) + 1
            if not pos: break
            self.lines.append(pos)
        self.lines.append(len(self.raw))

        # parse the source and write it
        self.pos = 0
        text = io.BytesIO(self.raw)
        self.out.write(rclexecm.makebytes(self.stylesheet))
        self.out.write(b'<pre class="code">\n')
        try:
            for a,b,c,d,e in tokenize.tokenize(text.readline):
                self(a,b,c,d,e)
        except Exception as ex:
            # There are other possible exceptions, for example for a
            # bad encoding line (->SyntaxError). e.g. # -*- coding: lala -*-
            self.out.write(("<h3>ERROR: %s</h3>\n" % ex).encode('utf-8'))
        self.out.write(b'\n</pre>')

    def __call__(self, toktype, toktext, startpos, endpos, line):
        """ Token handler.
        """
        if 0:
            print("type %s %s text %s start %s %s end %s %s<br>\n" % \
                  (toktype, token.tok_name[toktype], toktext, \
                   srow, scol,erow,ecol))
        srow, scol = startpos
        erow, ecol = endpos
        # calculate new positions
        oldpos = self.pos
        newpos = self.lines[srow] + scol
        self.pos = newpos + len(toktext)

        # handle newlines
        if toktype in [token.NEWLINE, tokenize.NL]:
            self.out.write(b'\n')
            return

        # send the original whitespace, if needed
        if newpos > oldpos:
            self.out.write(self.raw[oldpos:newpos])

        # skip indenting tokens
        if toktype in [token.INDENT, token.DEDENT]:
            self.pos = newpos
            return

        # map token type to a color group
        if token.LPAR <= toktype and toktype <= token.OP:
            toktype = token.OP
        elif toktype == token.NAME and keyword.iskeyword(toktext):
            toktype = _KEYWORD
        css_class = _css_classes.get(toktype, 'text')

        # send text
        self.out.write(rclexecm.makebytes('<span class="%s">' % (css_class,)))
        self.out.write(rclexecm.makebytes(html.escape(toktext)))
        self.out.write(b'</span>')


def colorize_file(file=None, outstream=sys.stdout, standalone=True):
    """Convert a python source file into colorized HTML.

    Reads file and writes to outstream (default sys.stdout). file can be a
    filename or a file-like object (only the read method is used). If file is
    None, act as a filter and read from sys.stdin. If standalone is True
    (default), send a complete HTML document with header and footer. Otherwise
    only a stylesheet and a <pre> section are written.
    """

    from os.path import basename
    if hasattr(file, 'read'):
        sourcefile = file
        file = None
        try:
            filename = basename(file.name)
        except:
            filename = 'STREAM'
    elif file is not None:
        try:
            sourcefile = open(file, 'rb')
            filename = basename(file)
        except IOError:
            raise SystemExit("File %s unknown." % file)
    else:
        sourcefile = sys.stdin
        filename = 'STDIN'
    source = sourcefile.read()

    if standalone:
        outstream.write(rclexecm.makebytes(_HTML_HEADER % {'title': filename}))
    Parser(source, out=outstream).format()
    if standalone:
        outstream.write(rclexecm.makebytes(_HTML_FOOTER))

    if file:
        sourcefile.close()


class PythonDump(RclBaseHandler):
    def __init__(self, em):
        super(PythonDump, self).__init__(em)

    def html_text(self, fn):
        preview = os.environ.get("RECOLL_FILTER_FORPREVIEW", "no")
        if preview == "yes":
            out = io.BytesIO()
            colorize_file(fn, out)
            return out.getvalue()
        else:
            self.outputmimetype = "text/plain"
            return open(fn, 'rb').read()

if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = PythonDump(proto)
    rclexecm.main(proto, extract)
