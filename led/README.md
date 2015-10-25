This the 'led' object intended as a virtual led. As it is a clone of the 
led object in Max/MSP, a good place for it would be cyclone. The 
current implementation however is a rip-off of the toggle object from 
pd-vanilla, and original an iem_gui object. As such it depends on the 
g_all_guis library and not at all on the cyclone/hammer framework.

The makefile system used is Makefile.pdlibbuilder which is intended as a 
Makefile Template replacement. It tries to find the include files in the 
regular places, but if this fails, you have to provide a path, like: 

make pdincludepath=../../pd-0.46-6/src/

To install it, you can use make install, but the default install path 
is usually not desired. An alternate path can be specified like this:

make install pkglibdir=/home/fjkraan/pd-externals/

or copy the files in wavesel to your favourite pd-externals location.


Fred Jan Kraan, 2015-10-25



