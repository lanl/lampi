######################################################################
#
# Makefile system dependent definitions for LA-MPI ROMIO integration
#
######################################################################

# CONFIG = alphaev6-dec-osf5.1 (Tru64, alpha, ev6, cxx)

ROMIOCONFIG         :=
ROMIOCONFIG_DEBUG   := -debug
ROMIOCONFIG_LINKLIB := -soname libmpi.so -all romio/libmpio.a -none -laio
ROMIOCONFIG_F90     := -f90=f90
