#!/usr/bin/env python2
from string import *
import os, commands, getopt, sys


#  Modifications so that our build of ROMIO has all possible fortran
#  symbols in it.  (only works on systems with support for #pragma weak)
#
#
# Step 1:  Edit ./configure
#
#    Note: for 1.2.4 there is a bug in configure which will prevent 
#    #pragma weak from being enabled with newer versions of gcc.  I've
#    fixed this, and ROMIO has fixed it in verison 1.2.5
#
#    Dont try to compile any fortran programs during configure,
#    just make the default FORTRANUNDERSCORE and remove fortran check.  
#
#    ROMIO will now be build with PMPI_ functions by default, 
#    and (weak) MPI_  
#    Except for fortran under linux with gcc 3:  weak MPI_ symbols will 
#    be missing because function prototypes are missing.
#
# Step 2:  */fortran/*.c:
#     add the following to the beginning of every file, before any 
#     #defines can change the names of the routines.  This will fix the 
#     linux/pragma/fortran problem
#     mentioned above, and also add the rest of the fortran symbols.
#
#     to do this, run this python script on each fortran file:
#     % ./add_fortran_pragmas.py  mpi-io/fortran/*.c
#     % ./add_fortran_pragmas.py  mpi2-other/array/fortran/*.c
#     % ./add_fortran_pragmas.py  mpi2-other/info/fortran/*.c
#
#     Then remove the ROMIO's original
#     #pragma weak mpi_file_open_ pmpi_file_open_
#     This pragma is broken under some versions of gcc because it appears after
#     #define mpi_file_open_ pmpi_file_open_, and thus becomes
#     #pragma weak pmpi_file_open_ pmip_file_open_
#
# Step 3: testing
#     the test code directory will be be "configured" into the lampi
#     build directory.  Makefile LIBS and INCLUDE_DIR needs to be edited
#     a little, and then
#     runtests can be used?
#

def scan_for_subroutine(file):
    fid=open(file)
    line=fid.readline()

    name=""
    while line:
        if (find(line,"void")>-1 and find(line,"(")>-1):
            line=split(line,"(")
            line=line[0]
            line=split(line," ")
            line=line[-1]
            if ("_" == line[-1] and "mpi"==line[0:3]):
                name=line[:-1]
                break
        line=fid.readline()
    fid.close()        
    return name
    

def create_pragma_text(fname):
    text = """


#if defined(HAVE_WEAK_SYMBOLS) && defined(FORTRANUNDERSCORE) 

void pmpi_file_open(void);
void mpi_file_open(void);
/*  void pmpi_file_open_(void);   this is the real function, below */
void mpi_file_open_(void);   
void pmpi_file_open__(void);
void mpi_file_open__(void);
void PMPI_FILE_OPEN(void);
void MPI_FILE_OPEN(void);

#pragma weak PMPI_FILE_OPEN = pmpi_file_open_     
#pragma weak pmpi_file_open = pmpi_file_open_
#pragma weak pmpi_file_open__ = pmpi_file_open_
#pragma weak MPI_FILE_OPEN = pmpi_file_open_     
#pragma weak mpi_file_open = pmpi_file_open_
#pragma weak mpi_file_open_ = pmpi_file_open_  
#pragma weak mpi_file_open__ = pmpi_file_open_
#endif



"""
    # substitude "file_open" for fname
    i=0
    while (1):
        i=find(text,"mpi_file_open",i)
        if (i==-1):
            break
        text=text[:i] + fname + text[(i+13):]
        i=i+len(fname)

    i=0
    while (1):
        i=find(text,"MPI_FILE_OPEN",i)
        if (i==-1):
            break
        text=text[:i] + fname.upper() + text[(i+13):]
        i=i+len(fname)


    return text


def insert_text(file,text):
    # scan for HAVE_WEAK_SYMBOLS, outputting input
    fid=open(file)
    fidout=open(".temp.pragma",'w')
    
    inserted=0
    
    line=fid.readline()
    while line:
        if (0==inserted and find(line,"HAVE_WEAK_SYMBOLS")>-1) :
            if (find(line,"FORTRANUNDERSCORE")>-1):
                #already processed - skip?
                print "It looks like this file has already been 'fixed'"
                print "skipping..."
                return
            fidout.write(text)
            inserted=1  
        fidout.write(line)
        line=fid.readline()
    fid.close()
    if (inserted==1):
        cmd="mv -f "+file+" "+file+".bak"
        print cmd
        os.system(cmd)
        cmd="mv -f .temp.pragma "+file
        print cmd
        os.system(cmd)
    return




for file in sys.argv[1:]:
    fname=scan_for_subroutine(file)
    if (fname==""):
        print "Error parsing file: "+file+" for fortran subroute"
        print "skipping..."
    else:
        print " "
        print "filename: "+file
        print "function name: "+fname
        print "is this the correct FORTRAN function for this file? [y]/n"
        ans=sys.stdin.readline()
        if (1==len(ans) or "y\n"==ans):
            print "processing..."  
            pragma_text=create_pragma_text(fname)
            insert_text(file,pragma_text)


