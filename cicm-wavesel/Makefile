# Makefile for pure data external wavesel~


lib.name = cicm-wavesel

cflags = -IThirdParty/CicmWrapper/Sources 

includepath = -lCicmWrapper

#c.blackboard.class.sources := Sources/c.blackboard.cpp c.library.cpp
c.wavesel.class.sources := Sources/c.wavesel.cpp c.library.cpp

datafiles = \
Helps/c.wavesel-help.pd \
Helps/voice.wav \
Helps/voice2.wav \
../LICENSE.txt \
README.txt

#Helps/c.blackboard-help.pd \


################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

externalsdir = ..

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))
