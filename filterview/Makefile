# Makefile for pure data external filterview

lib.name = filterview


filterview.class.sources := filterview.c

datafiles = \
filterview-help.pd \
filterview-meta.pd \
README.txt \
LICENSE.txt

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

externalsdir = ..

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))

