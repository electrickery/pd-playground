# Makefile for pure data external wavesel~

lib.name = wavesel~

cflags = -Isrc/shared

hgui := \
src/shared/hammer/gui.c \
src/shared/common/loud.c

wavesel~.class.sources := src/wavesel~.c $(hgui)

datafiles = \
wavesel~-help.pd \
../LICENSE.txt \
README.txt \
voice.wav \
voice2.wav 

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

externalsdir = ..

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))
