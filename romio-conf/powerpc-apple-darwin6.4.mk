######################################################################
#
# Makefile system dependent definitions for LA-MPI ROMIO integration
#
######################################################################

# CONFIG = powerpc-darwin

ROMIOCONFIG         := -noprofile
ROMIOCONFIG_DEBUG   := -noprofile -debug
ROMIOCONFIG_LINKLIB := -Wl,-soname=libmpi.dylib -Wl,--whole-archive romio/libmpio.a -Wl,--no-whole-archive
ROMIOCONFIG_F90     := -f90=f90
