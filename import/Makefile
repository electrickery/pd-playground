# Makefile for pure data external import

lib.name = import


import.class.sources := import.c

datafiles = \
import-help.pd

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

externalsdir = ..

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))

