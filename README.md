LA-MPI: Los Alamos Message Passing Interface
--------------------------------------------

LA-MPI is a portable, end-to-end, network-fault-tolerant
implementation of the MPI 1.2 standard (with some extensions).

LA-MPI has been approved for release with associated LA-CC Number
LA-CC-02-41.


BUILDING THE LIBRARY
--------------------

See the file INSTALL

LICENSE
-------
 
See the file LICENSE

VERSION
-------

See the file configure.ac

DEVELOPER'S NOTES
-----------------

To add source files to the build, add the file name to the file
mpirun.mk or libmpi.mk in its directory.

To add a new directory to the build, add the directory name to the
list macros MODULES_MPIRUN or MODULES_LIBMPI in configure.ac and then
run autoconf.
