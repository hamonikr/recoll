# No shebang: this is only used on Windows. We use a shell script on Linux
from __future__ import print_function

import rclexecm
import sys
import os
import shutil
import platform
import subprocess
import glob


def _msg(s):
    rclexecm.logmsg(s)
    

sysplat = platform.system()
if sysplat != "Windows":
    _msg("rcluncomp.py: only for Windows")
    sys.exit(1)

try:
    import msvcrt
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
except Exception as err:
    _msg("setmode binary failed: %s" % str(err))

sevenz = rclexecm.which("7z")
if not sevenz:
    _msg("rcluncomp.py: can't find 7z exe. Maybe set recollhelperpath " \
          "in recoll.conf ?")
    sys.exit(2)

# Params: uncompression program, input file name, temp directory.
# We ignore the uncomp program, and always use 7z on Windows

infile = sys.argv[2]
outdir = sys.argv[3]
# _msg("rcluncomp.py infile [%s], outdir [%s]" % (infile, outdir))

# There is apparently no way to suppress 7z output. Hopefully the
# possible deadlock described by the subprocess module doc can't occur
# here because there is little data printed. AFAIK nothing goes to stderr anyway
try:
    cmd = [sevenz, "e", "-bd", "-y", "-o" + outdir, infile]
    subprocess.check_output(cmd, stderr = subprocess.PIPE)
    # Don't use os.path.join, we always want to use '/'
    outputname = glob.glob(outdir + "/*")
    # There should be only one file in there..
    print(outputname[0])
except Exception as err:
    _msg("%s" % (str(err),))
    sys.exit(4)

sys.exit(0)
