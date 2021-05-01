#!/usr/bin/python2
#
# wnck does not have a python3 binding as far as I can see (or at
# least it's not packaged by, e.g. Debian. So python2 only for now.
#
# This script should be linked to a keyboard shortcut. Under gnome,
# you can do this from the main preferences menu, or directly execute
# "gnome-keybinding-properties"
#
# Make the script executable. Install it somewhere in the executable
# path ("echo $PATH" to check what's in there), and then just enter
# its name as the action to perform, or copy it anywhere and copy the
# full path as the action.

import gtk
import wnck
import os
import sys
from optparse import OptionParser

def main():
    parser = OptionParser()
    parser.add_option("-m", "--move-away", action="store_true", default=False,
                      dest="clear_workspace", 
                      help="iconify to other workspace to avoid crowding panel")
    (options, args) = parser.parse_args()

    screen = wnck.screen_get_default()
    while gtk.events_pending():
        gtk.main_iteration()

    recollMain = ""
    recollwins = [];
    for window in screen.get_windows():
        if window.get_class_group().get_name() == "Recoll":
            if window.get_name() == "Recoll":
                recollMain = window
            recollwins.append(window)

    if not recollMain:
        os.system("recoll&")
        sys.exit(0)

    # Check the main window state, and either activate or minimize all
    # recoll windows.
    workspace = screen.get_active_workspace()
    if not recollMain.is_visible_on_workspace(workspace):
        for win in recollwins:
            win.move_to_workspace(workspace)
            if win != recollMain:
                win.unminimize(gtk.get_current_event_time())
        recollMain.activate(gtk.get_current_event_time())
    else:
        otherworkspace = None
        if options.clear_workspace:
            # We try to minimize to another workspace
            wkspcs = screen.get_workspaces()
            for wkspc in wkspcs:
                if wkspc.get_number() != workspace.get_number():
                    otherworkspace = wkspc
                    break
        for win in recollwins:
            if otherworkspace:
                win.move_to_workspace(otherworkspace)
            win.minimize()

if __name__ == '__main__':
  main()
  
