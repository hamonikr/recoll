Recoll KIO slave
================

Usage
=====

Depending on the protocol name used, the search results will be
returned either as HTML pages (looking quite like a normal Recoll
result list), or as directory entries.

The HTML mode only works with Konqueror, not Dolphin. The directory
mode is available with both browsers, and also application open dialog
(ie Kate).

The HTML mode is much more usable than the directory mode at this point

More detailed help/explanations can be found a document accessible
from the slave:

To try things out, after building and installing, enter "recoll:/" in
a Konqueror URL entry. Depending on the KDE version, this will bring
you either to an HTML search form, or to a directory listing, where
you should READ THE HELP FILE.


Building and installing:
=======================

The main Recoll installation shares its prefix with the KIO slave,
which needs to use the KDE one. This means that, if KDE lives in /usr,
Recoll must be configured with --prefix=/usr, not /usr/local. Else
you'll have run-time problems, the slave will not be able to find the
Recoll configuration.

Recipe:

 - Make sure the KF5 core and KIO devel packages and cmake are
   installed. You probably need the kio-devel and extra-cmake-modules
   packages.

 - Extract the Recoll source.

 - IF Recoll is not installed yet: configure recoll with 
   --prefix=/usr (or wherever KDE lives), build and install 
   Recoll.

 - In the Recoll source, go to kde/kioslave/kio_recoll, then build and
   install the kio slave:

mkdir builddir
cd builddir
cmake .. -DCMAKE_INSTALL_PREFIX=/usr 
make
sudo make install

 - You should have a look at where "make install" copies things,
   because misconfigured distribution, generating wrong targets, are
   frequent. Especially, you should check that kio_recoll.so is copied
   to the right place, meaning among the output of "kde4-config --path
   module". As an additional check, there should be many other
   kio_[xxx].so in there. Same for the protocol file, check that it's
   not alone in its directory (really, this sounds strange, but, to
   this point, I've seen more systems with broken cmake/KDE configs
   than correct ones).

You need to build/update the index with recollindex, the KIO slave
doesn't deal with indexing for now.


