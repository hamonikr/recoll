Recoll KIO slave
================

An experiment with a recoll KIO slave.

Caveat: I am only currently testing this with a production, but very
recent, version of KDE 4.1, and I don't intend to really support
older versions. The most usable aspects work under KDE 4.0 though. As
a reference, my test system is an up to date (2009-01) Kubuntu 8.10.

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

Only tested with KDE 4.1 and later.

The main Recoll installation shares its prefix with the KIO slave,
which needs to use the KDE one. This means that, if KDE lives in /usr,
Recoll must be configured with --prefix=/usr, not /usr/local. Else
you'll have run-time problems, the slave will not be able to find the
Recoll configuration.

!!*Notice: You cannot share a build directory between recoll and kio_recoll
because they use different configure options for the main lib, but build it
in the same place. The main lib "configure" is run at "cmake" time for
kio_recoll, the build is done at "make" time.


Recipe:
 - Make sure the KDE4 core devel packages and cmake are installed.

 - Extract the Recoll source.

 - IF Recoll is not installed yet: configure recoll with 
   --prefix=/usr (or wherever KDE lives), build and install 
   Recoll.

 - In the Recoll source, go to kde/kioslave/recoll, then build and
   install the kio slave:

mkdir builddir
cd builddir
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DQT_QMAKE_EXECUTABLE=/usr/bin/qmake-qt4
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


Misc build problems:
===================

KUBUNTU 8.10 (updated to 2008-27-11)
------------------------------------
cmake generates a bad dependency on
      /build/buildd/kde4libs-4.1.2/obj-i486-linux-gnu/lib/libkdecore.so 
inside CMakeFiles/kio_recoll.dir/build.make 

Found no way to fix this. You need to edit the line and replace the
/build/[...]/lib with /usr/lib. This manifests itself with the
following error message:

   make[2]: *** No rule to make target `/build/buildd/kde4libs-4.1.2/obj-i486-linux-gnu/lib/libkdecore.so', needed by `lib/kio_recoll.so'.  Stop.
