######################################################################
#
# Makefile system dependent definitions for LA-MPI ROMIO integration
#
######################################################################

# CONFIG = ia64-unknown-linux-gnu (linux, ia64, gcc)

ROMIOCONFIG         := -noprofile
ROMIOCONFIG_DEBUG   := -noprofile -debug
ROMIOCONFIG_LINKLIB := -Wl,-soname=libmpi.so -Wl,--whole-archive romio/libmpio.a -Wl,--no-whole-archive
ROMIOCONFIG_F90     := -f90=f90
