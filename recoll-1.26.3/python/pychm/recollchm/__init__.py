# Copyright (C) 2003-2006 Rubens Ramos <rubensr@users.sourceforge.net>
#
# pychm is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; see the file COPYING.  If not,
# write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA
#

'''
   chm - A package to manipulate CHM files

   The chm package provides four modules: chm, chmlib, extra and
   _chmlib. _chmlib and chmlib are very low level libraries generated
   from  SWIG interface files, and are simple wrappers around the API
   defined by the C library chmlib.
   The extra module adds full-text search support.
   the chm module provides some higher level classes to simplify
   access to the CHM files information.
'''
__all__ = ["chm", "chmlib", "_chmlib", "extra"]
__version__ = "0.8.4.1+git"
__revision__ = "$Id$"
