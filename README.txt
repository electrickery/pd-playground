wavesel~ is intended to become the cyclone counterpart of Max waveform~ object. It isn't there yet, and it has considerable performance issues.

The makefile system used is Makefile.pdlibbuilder which is intended as a Makefile Template replacement. It tries to find the include files in the regular places, but if this fails, you have to provide a path, like: 

make pdincludepath=../../pd-0.46-6/src/

To install it, you can use make install, but the default install path is usually not desired. An alternate path can be specified like this:

make install pkglibdir=/home/fjkraan/pd-externals/

or copy the files in wavesel to your favourite pd-externals location.

The help patch assumes cyclone is available on your system, but any sample player that understands milli-seconds will do.


fjkraan@xs4all.nl, 2015-07-05
