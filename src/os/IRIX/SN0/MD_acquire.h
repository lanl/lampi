/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/



#ifndef _MD_ACQUIRE_H_
#define _MD_ACQUIRE_H_
extern "C" {
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>  // for socket stuff
#include <netinet/in.h>  // for socket stuff
#include <net/if.h>      // for socket stuff
#include <net/soioctl.h> // for socket stuff
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <malloc.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
}

#include "ulm/ulm.h"   // error macros
#include "os/IRIX/SN0/fetchAndOp.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "os/IRIX/SN0/MD_acquireGlobals.h"

class
acquire : public MI_acquire {
public:
    // constructors (should probably have a copy constructor, too)
    acquire() {
        nummlds = 0;
        free_mld = false;
        mld = NULL;
        mldset = (pmo_handle_t) -1; // hope this is okay
        numpms = 0;
        pm = NULL;
        MI_init(0);
        useO2kFetchOpHardware=true;
    }
    acquire(int p) {
        free_mld = false;
        if (init_base(p, false, NULL, 0, NULL)) {
            numpms = 0;
            pm = NULL;
            MI_init(p);
        }
        useO2kFetchOpHardware=true;
    }
    acquire(int p, bool cpu2node) {
        free_mld = false;
        if (init_base(p, cpu2node, NULL, 0, NULL)) {
            numpms = 0;
            pm = NULL;
            MI_init(p);
        }
        useO2kFetchOpHardware=true;
    }
    acquire(int nummlds, pmo_handle_t *mldlist) {
        free_mld = false;
        if (init_base(nummlds, true, mldlist, 0, NULL)) {
            numpms = 0;
            pm = NULL;
            MI_init(nummlds);
        }
        useO2kFetchOpHardware=true;
    }
    acquire(int p, int actual_numres, resource_info *ri);

    acquire(int p, int numraff, raff_info_t *rafflist) {
        if (init_base(p, false, NULL, numraff, rafflist)) {
            numpms = 0;
            pm = NULL;
            MI_init(p);
        }
        useO2kFetchOpHardware=true;
    }

    // destructors
    ~acquire() {
        int i;
        ulm_delete(mldnum);
        ulm_delete(cpunum);
        if (free_mld) {
            for (i = 0; i < nummlds; i++) {
                mld_destroy(mld[i]);
            }
#ifdef _DONT_USE_MMAP_
            ulm_delete(mld);
#else
            munmap(mld, nummlds*sizeof(pmo_handle_t));
#endif
        }
        mldset_destroy(mldset);
        if (pm != NULL) {
            for (i = 0; i < numpms; i++) {
                pm_destroy(pm[i]);
            }
            ulm_delete(pm);
        }
    }

    //
    // functions to mmap memory
    //
    // for now, allocate "maxlen"
    //
    // alloc_shared(): mmap a shared memory region
    inline void *
        alloc_shared(int maxlen) {
        return(alloc_mem(maxlen, MAP_SHARED));
    }
    //
    // alloc_private(): mmap a a private region
    inline void *
        alloc_private(int maxlen) {
        return(alloc_mem(maxlen, MAP_PRIVATE));
    }
    //
    // alloc_mem(): mmap a memory region
    inline void *
        alloc_mem(int maxlen, int flags) {
        void *ptr;
        int fd;
        fd = open("/dev/zero", O_RDWR);
        if (fd < 0) {
            close(fd);
            return(0);
        }
        ptr = mmap(0, maxlen, PROT_READ|PROT_WRITE, flags, fd, 0);
        close(fd);
        if (((unsigned long) ptr) == -1) {
            return(0);
        }
        return ptr;
    }

    //
    // attach a policy to an address range
    //
    // attach the entire address range to one mld
    bool set_policy(void *ptr, int len, int cpu);
    //
    // attach the address range in a round robin fashion with given
    //  stride to all the mlds
    inline bool
        set_policy(void *ptr, int len, int stride, bool cpu2node) {
        int i, *cpus;
        bool retval;
        cpus = ulm_new(int, numcpus);
        for (i = 0; i < numcpus; i++) {
            cpus[i] = i;
        }
        retval = set_policy(ptr, len, stride, cpus, numcpus);
        ulm_delete(cpus);
        return(retval);
    }
    //
    // attach the address range in a round robin fashion with given
    //  stride to the list of cpus
    inline bool
        set_policy(void *ptr, int len, int stride, int *cpus, int numcpus) {
        int i, c, l;
        void *p, *next, *max;
        policy_set_t ps;
        // default values for policy set
        pm_filldefault(&ps);
        // fix the placement of the memory
        ps.placement_policy_name = "PlacementFixed";

        if (pm != NULL) {
            for (i = 0; i < numpms; i++) {
                pm_destroy(pm[i]);
            }
            if (numpms < numcpus) {
                ulm_delete(pm);
                numpms = numcpus;
                pm = ulm_new(pmo_handle_t, numpms);
            }
        }
        else {
            numpms = numcpus;
            pm = ulm_new(pmo_handle_t, numpms);
        }

        if (numcpus == 1) {
            // short cut for 1 cpu
            ps.placement_policy_args = (void *) mld[mldnum[*cpus]];
            pm[0] = pm_create(&ps);
            // attach the policy to the ENTIRE virtual address range
            if (pm_attach(pm[0], ptr, len) < 0) { return(false); }
            return(true);
        }

        /*** WARNING *** WARNING ***/
        /*** THIS WILL BREAK IF sizeof(char) > 1 BYTE ***/
        if (!(sizeof(char)==1)) {
            ulm_exit(("Aborting\n"));
        }
        max = static_cast<char *>(ptr) + len;
        for (c = 0, p = ptr; p < max; c = ++c % numcpus, p = next) {
            ps.placement_policy_args = (void *) mld[mldnum[cpus[c]]];
            next = static_cast<char *>(p) + stride;
            l = ((next < max) ?
                 stride :
                 static_cast<char *>(max) - static_cast<char *>(next));
            pm[c] = pm_create(&ps);
            // attach the policy to this virtual address range
            if (pm_attach(pm[c], p, l) < 0) { return(false); }
        }
        return(true);
    }

    // initialize parent process
    inline bool init_parent() {
        schedctl(SETHINTS);
        return(true);
    }

    // initialize child process (from parent)
    inline bool
        parent_init_child(int cpu) {
        return(true);
    }

    // initialize child process (from child)
    inline bool
        child_init_child(int cpu) {
        schedctl(SETHINTS);
        //! fix processor resource, if required
        if(placementSucceeded) {
            int ErrorCode = process_cpulink(getpid(), mld[mldnum[cpu]],
                                            cpunum[cpu], RQMODE_MANDATORY) ;
            if (ErrorCode <0 ) {
                return(false);
            }
        }
        return(true);
    }

    // acquire requested resources
    //
    // NOTE: the machine will go down when a cpu goes down and perhaps
    //       also when a node goes down, so we'll assume that the hardware
    //       paths give more or less accurate info about available
    //       cpus and nodes (memory).
    //
    bool acquire_resource(int numres, resource_info *ri) {
        int i;
        bool retval = true;
        for (i = 0; i < numres; i++) {
            switch(ri->get_type()) {
            case R_HIPPI:
                // ping the hippi device
                retval |= ping_hippi(ri[i].get_num());
                break;
            default:
                break;
            }
        }
        return(retval);
    }

private:
    bool init_base(int p, bool cpu2node, pmo_handle_t *mldlist,
                   int numraffs, raff_info_t *rafflist);

    //
    // ping a hippi device (stripped from init_hippi800.cc)
    //
    inline bool
        ping_hippi(int hippi_num) {
        char pstr[256], str[3][256];
        int rcvd = 0, pos = 0;
        FILE *pFP;
        int hippiSoc;
        struct ifreq hippiNetInfo;
        unsigned int ipaddr;

        hippiSoc = socket(AF_INET, SOCK_DGRAM, 0);
        if (hippiSoc < 0) {
            cerr << "Warning: unable to open socket to contact hippi device " << hippi_num << endl;
            return(false);
        }
        hippiNetInfo.ifr_addr.sa_family = AF_INET;
        sprintf(hippiNetInfo.ifr_name, "hip%d", hippi_num);
        if (ioctl(hippiSoc, SIOCGIFADDR, &hippiNetInfo) < 0) {
            cerr << "Warning: unable to query hippi device " << hippi_num << endl;
            close(hippiSoc);
            return(false);
        }

        ipaddr = (unsigned int) ((struct sockaddr_in *) (&(hippiNetInfo.ifr_addr)))->sin_addr.s_addr;
        sprintf(pstr, "/usr/etc/ping -c 1 %u", ipaddr);
        if ((pFP = popen(pstr, "r")) == 0) {
            cerr << "Warning: unable to ping hippi device " << hippi_num << endl;
            close(hippiSoc);
            return(false);
        }

        /*** sungeun *** yuck is all i can say ***/
        while (fscanf(pFP, "%s", str[pos%3]) != EOF) {
            if (!(strlen(str[pos%3])<256)) {
                ulm_exit(("Aborting\n"));
            }
            // looking for the "received" string to indicate success
            if (strncmp(str[pos%3], "received", 8) == 0) {
                rcvd = atoi(str[(pos-2)%3]);
                break;
            }
            pos++;
        }
        pclose(pFP);
        if (rcvd == 0) {
            cerr << "Warning: hippi device " << hippi_num << " is down" << endl;
            close(hippiSoc);
            return(false);
        }

        close(hippiSoc);
        return(true);
    }

    // function to extract node list from the cpu list
    //
    // This routine takes in a cpu resource affinity list, computes
    //   the number of nodes in use, and returns this number.
    //   A rafflist for the nodes is created.
    //

    int nodeListFromCpuList(int nCpus, int *nNodes, raff_info_t **nodeRaffList);

//!
//!  This routine is used to allocate a pool of O2k atomic fetch-
//!    and-op variables.
//!

    int allocO2kFetchOpbarrierPools();

    // functions for mapping a cpu number to an mld (virtual node)
    //  and a cpu associated with that mld, while attempting to
    //  avoid conditionals, in case anyone cares
    //
    // mldnum(): if 1 cpu per node, return cpu
    //           if 2 cpus per node, return (cpu+1)/2
    // inline int
    // mldnum(int cpu) { return((cpu+cpu_offset)/cpus_per_node); }
    //
    // cpunum(): return cpu (0 or 1) on a node
    //           use Rich's shortcut for % because it is defined for 0%2
    //           assume the compiler does CSE
    // inline int
    // cpunum(int cpu) { return((cpu*cpu_offset)-((cpu*cpu_offset)/2)*2); }

public:
    int nummlds;         // number of memory locality domains
    pmo_handle_t *mld;   // list of memory locality domains
    int nCpusPerNode;    // number of cpus per node to use
    bool useResourceAffinity; // apply resource affinity
    bool useDefaultAffinity;  // use os defined MLD
    bool affinityMandatory;   // fail if can't use resource affinity

private:
    bool free_mld;       // did i allocate mld?  then i must free it
    int *mldnum;         // which mld to use
    pmo_handle_t mldset; // set of memory locality domains

    int numcpus;         // number of cpus
    int *cpunum;         // which cpu to use
    int cpus_per_node;   // number of cpus per node to use
    int cpu_offset;      // offset for calculating cpu number

    int numpms;          // number of policy modules used
    pmo_handle_t *pm;    // policy modules;
    raff_info_t *nodeRaffList ;   // cpu node resource affinity list
    bool useO2kFetchOpHardware ;  // use useO2kFetchOpHardware

public:
    int nCpuNodes;       // number of cpu nodes in use
    atomicHWBarrierPool hwBarrier;
};

#endif /*** _MD_ACQUIRE_H_ ***/
