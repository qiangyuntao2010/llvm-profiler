##===- projects/sample/lib/sample/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Give the name of a library.  This will build a dynamic version.
#
LIBRARYNAME=AliasAnalysis
LOADABLE_MODULE = 1
BUILD_RELINKED=1
SHARED_LIBRARY=1

BOOST_ROOT_DIR=$(BOOST_ROOT)
CFlags += -fPIC -I$(BOOST_ROOT_DIR)
EXTRACXXFLAGS += -I$(BOOST_ROOT_DIR)  -fstack-protector-all
#-fsanitize=safe-stack

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common
