######################################################################
#
# Makefile system dependent definitions for LA-MPI ROMIO integration
#
######################################################################

# CONFIG = mips-sgi-irix6.5 (SGI O2000, IRIX 6.5)

ROMIOCONFIG         :=
ROMIOCONFIG_DEBUG   := -debug
ROMIOCONFIG_LINKLIB := -Wl,-soname,libmpi.so -Wl,-all romio/libmpio.a -Wl,-none
ROMIOCONFIG_F90     := -f90=f90
