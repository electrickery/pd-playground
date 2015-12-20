# Makefile for pure data external wavesel~

lib.name = wavesel~

cflags = -Isrc/shared

hws := \
src/shared/hammer/gui.c \
src/shared/common/loud.c \
src/shared/unstable/forky.c

wavesel~.class.sources := src/wavesel~.c $(hws)

datafiles = \
wavesel~-help.pd \
../LICENSE.txt \
README.md \
voice.wav \
voice2.wav 

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))
