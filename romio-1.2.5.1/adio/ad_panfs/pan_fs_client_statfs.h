/*
 * pan_fs_client_statfs.h
 * 
 * IOCTL interface into the PanFS client for extended statfs information
 *
 * @author  ehogan
 * @version 1.0
 *
 */
/*
 * @PANASAS_COPYRIGHT@
 */
#ifndef _PAN_FS_CLIENT__PAN_FS_CLIENT_STATFS_H_
#define _PAN_FS_CLIENT__PAN_FS_CLIENT_STATFS_H_

#ifndef KERNEL

#if (__freebsd__ > 0 || __solaris__ > 0)
#include <sys/ioccom.h>
#endif /* (__freebsd__ > 0 || __solaris__ > 0) */

#if (__linux__ > 0)
#include <sys/ioctl.h>
#endif /* (__linux__ > 0) */

#endif /* !defined(KERNEL) */

#if (__freebsd__ > 0)
typedef unsigned long long    pan_fs_client_statfs_uint64_t;
#elif ((__linux__ > 0) && (__i386__ > 0))
typedef unsigned long long    pan_fs_client_statfs_uint64_t;
#elif ((__linux__ > 0) && (__ia64__ > 0))
typedef unsigned long         pan_fs_client_statfs_uint64_t;
#else /* not fbsd or linux */
typedef unsigned long long    pan_fs_client_statfs_uint64_t;
#endif /* not fbsd or linux */

#define PAN_FS_CLIENT_STATFS_IOCTL                    ((unsigned int)0x24)

#define PAN_FS_CLIENT_STATFS_MAX_MOUNT_NAME_LEN       256

#define PAN_FS_CLIENT_STATFS_VERSION_V1               1

typedef struct pan_fs_client_statfs_extended_res_v1_s pan_fs_client_statfs_extended_res_v1_t;
typedef pan_fs_client_statfs_extended_res_v1_t        pan_fs_client_statfs_extended_res_t;

/*
 * extended statfs() ioctl results structure
 *
 * struct_version
 *    Version number for results struct.  Currently must be 1.
 *
 * mount_from_name
 *    The device from where we mount the panfs file system.
 *
 * mount_from_name_len
 *    The actual length of the above field.
 *
 * volume_id
 *    Opaque volume identifier.  Two objects in the same volume are
 *    guaranteed to have the same volume_id.  Two objects in different
 *    volumes are guaranteed to have different volume_ids.
 *
 * bladeset_id
 *    Opaque bladeset identifier.  Two objects in the same BladeSet
 *    are guaranteed to have the same bladeset_id.  Two objects in
 *    different BladeSets are guaranteed to have different
 *    bladeset_ids.
 *
 * bladeset_storageblade_count
 *    number of StorageBlades in this object's BladeSet.
 *
 * bladeset_total_bytes
 *    Total number of bytes in the bladeset, including all reserved
 *    space, space used by this and other volumes, and all free space.
 *
 * bladeset_free_bytes
 *    Number of bytes in the bladeset not currently in use.  Includes
 *    space that is not in use but is reserved for reconstruction
 *    spares.
 *
 * bladeset_unreserved_total_bytes
 *    Total number of bytes in the bladeset available for user data,
 *    not including space reserved for reconstruction spares.
 *
 * bladeset_unreserved_free_bytes
 *    Number of bytes in the bladeset not currently in use and
 *    available for user data.  Does not include space that is
 *    reserved for reconstruction spares (used or free).
 *
 * bladeset_recon_spare_total_bytes
 *    Total number of bytes in the bladeset reserved for
 *    reconstruction spares.
 *
 * volume_live_bytes_used
 *    Number of bytes used by this volume's "live" data (i.e., not
 *    including snapshots).
 *
 * volume_snapshot_bytes_used
 *    Number of bytes used by this volume's snapshot(s).  This field
 *    is currently not supported and will always be returned as 0.
 *
 * volume_hard_quota_bytes
 *    Hard quota for this volume, in bytes.  If the volume does not
 *    have a hard quota this field will be 0.
 *
 * volume_soft_quota_bytes
 *    Soft quota for this volume, in bytes.  If the volume does not
 *    have a soft quota this field will be 0.
 *
 * filler
 *    Reserved for future expansion.
 *
 * Note that the amount of space available in a volume for new files
 * is limited both by the volume's hard quota, and by the space
 * remaining in the bladeset.  The amount of space remaining in the
 * volume that is available for users can be computed as:
 *
 * if(volume_hard_quota_bytes > 0) {
 *   free_space = MIN(bladeset_free_bytes,
 *                    volume_hard_quota_bytes - volume_live_bytes_used);
 * }
 * else {
 *   free_space = bladeset_free_bytes;
 * } 
 *
 * Due to metadata & parity overhead, the amount of additional file
 * data that can be written into the volume will generally be less
 * than this figure.
 */
struct pan_fs_client_statfs_extended_res_v1_s {
  unsigned int                    struct_version;

  char                            mount_from_name[PAN_FS_CLIENT_STATFS_MAX_MOUNT_NAME_LEN];
  int                             mount_from_name_len;

  unsigned long                   volume_id;
  unsigned long                   bladeset_id;

  unsigned long                   bladeset_storageblade_count;
  pan_fs_client_statfs_uint64_t   bladeset_total_bytes;
  pan_fs_client_statfs_uint64_t   bladeset_free_bytes;
  pan_fs_client_statfs_uint64_t   bladeset_unreserved_total_bytes;
  pan_fs_client_statfs_uint64_t   bladeset_unreserved_free_bytes;
  pan_fs_client_statfs_uint64_t   bladeset_recon_spare_total_bytes;

  pan_fs_client_statfs_uint64_t   volume_live_bytes_used;
  pan_fs_client_statfs_uint64_t   volume_snapshot_bytes_used;
  pan_fs_client_statfs_uint64_t   volume_hard_quota_bytes;
  pan_fs_client_statfs_uint64_t   volume_soft_quota_bytes;

  unsigned long                   filler[16];
};

/**
 * An ioctl used to collect extended statfs information about a mountpoint
 * 
 * @author  ehogan
 * @version 1.0
 *
 * @since   1.0
 */
#define PAN_FS_CLIENT_STATFS_EXTENDED \
  _IOWR(PAN_FS_CLIENT_STATFS_IOCTL,80,pan_fs_client_statfs_extended_res_t)

#endif /* _PAN_FS_CLIENT__PAN_FS_CLIENT_STATFS_H_ */

/* Local Variables:  */
/* indent-tabs-mode: nil */
/* tab-width: 2 */
/* End: */

