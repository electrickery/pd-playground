# Makefile for pure data externals GUI1, GUI2, ...

lib.name = GUI

GUI1.class.sources := GUI1.c
GUI2.class.sources := GUI2.c
GUI3.class.sources := GUI3.c
GUI4.class.sources := GUI4.c

datafiles = \
GUI1-help.pd \
GUI2-help.pd \
GUI3-help.pd \
GUI4-help.pd \
LICENSE.txt \
README.md

################################################################################
### pdlibbuilder ###############################################################
################################################################################


# Include Makefile.pdlibbuilder from this directory, or else from central
# externals directory in pd-extended configuration.

include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))
