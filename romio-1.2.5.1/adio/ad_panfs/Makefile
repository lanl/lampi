# Generated automatically from Makefile.in by configure.
CC          = gcc
AR          = ar crl
LIBNAME     = /home/taylorm/lampi/src/lampi/romio-1.2.5.1/objects/romio.a
srcdir      = /home/taylorm/lampi/src/lampi/romio-1.2.5.1/adio/ad_panfs

INCLUDE_DIR = -I/home/taylorm/lampi/src/lampi/src/include -I${srcdir}/../include -I../include
CFLAGS      =  -g -O -Wall -Wstrict-prototypes -Wmissing-prototypes -DFORTRANUNDERSCORE -DHAVE_ROMIOCONF_H $(INCLUDE_DIR)

VPATH=.:$(srcdir)

AD_PANFS_OBJECTS = ad_panfs_close.o ad_panfs_read.o \
      ad_panfs_open.o ad_panfs_write.o ad_panfs_done.o \
      ad_panfs_fcntl.o ad_panfs_iread.o ad_panfs_iwrite.o ad_panfs_wait.o \
      ad_panfs_resize.o ad_panfs_hints.o\
      ad_panfs.o


default: $(LIBNAME)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(LIBNAME): $(AD_PANFS_OBJECTS)
	$(AR) $(LIBNAME) $(AD_PANFS_OBJECTS)

clean:
	@rm -f *.o
