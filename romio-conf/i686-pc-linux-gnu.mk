######################################################################
#
# Makefile system dependent definitions for LA-MPI ROMIO integration
#
######################################################################

# CONFIG = i686-pc-linux-gnu (linux, i686, gcc)

ROMIOCONFIG         := -noprofile
ROMIOCONFIG_DEBUG   := -noprofile -debug
ROMIOCONFIG_LINKLIB := -Wl,-soname=libmpi.so -Wl,--whole-archive romio/libmpio.a -Wl,--no-whole-archive
ROMIOCONFIG_F90     := -f90=f90
