This the 'import' object isolated from the pd-extended core. It is build
using pdlibbuilder allowing it to be used with pd-vanilla. The code is 
exactly as it was written by Hans Cristoph Steiner for pd-extended 0.44.

The makefile system used is Makefile.pdlibbuilder which is intended as a 
Makefile Template replacement. It tries to find the include files in the 
regular places, but if this fails, you have to provide a path, like: 

make pdincludepath=../../pd-0.46-6/src/

To install it, you can use make install, but the default install path 
is usually not desired. An alternate path can be specified like this:

make install pkglibdir=/home/fjkraan/pd-externals/

or copy the files from ./import to your favourite pd-externals location.


Fred Jan Kraan, 2016-01-29



