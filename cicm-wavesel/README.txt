c.wavesel~ is intended to become the cyclone counterpart of Max 
waveform~ object. It is based om the https://github.com/CICM/CicmWrapper
framework.

The makefile system used is Makefile.pdlibbuilder which is intended as a 
Makefile Template replacement. It tries to find the include files in the 
regular places, but if this fails, you have to provide a path, like: 

make pdincludepath=../../pd-0.46-6/src/

To install it, you can use make install, but the default install path is 
usually not desired. An alternate path can be specified like this:

make install pkglibdir=/home/fjkraan/pd-externals/

or copy the files in wavesel to your favourite pd-externals location.

Note this does not compile the CicmWrapper framework, as this has its own
auto-tools based build system.



fjkraan@xs4all.nl, 2015-07-05
