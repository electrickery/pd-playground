# Makefile for pure data external 

lib.name = mouseCanvas

cflags = -Isrc/shared


mouseCanvas.class.sources := mouseCanvas.c

datafiles = receivecanvas-help.pd

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))
