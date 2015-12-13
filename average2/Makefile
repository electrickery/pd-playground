# Makefile for pure data externals in average2

lib.name = average2

cflags = -Ishared


# common sources for sickle types

ssic := \
shared/sickle/sic.c \
shared/common/loud.c

# sickle classes

average2~.class.sources := sickle/average2.c $(ssic)



datafiles = \
$(wildcard help/*-help.pd) \
LICENSE.txt \
README.md


################################################################################
### pdlibbuilder ###############################################################
################################################################################


include $(firstword $(wildcard Makefile.pdlibbuilder \
  $(externalsdir)/Makefile.pdlibbuilder))




