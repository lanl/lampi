/*
 * pan_fs_client_cw_mode.h
 * 
 * IOCTL interface into the PanFS client for concurrent write services
 *
 * @author  ehogan
 * @version 1.0
 *
 */
/*
 * @PANASAS_COPYRIGHT@
 */
#ifndef _PAN_FS_CLIENT__PAN_FS_CLIENT_CW_MODE_H_
#define _PAN_FS_CLIENT__PAN_FS_CLIENT_CW_MODE_H_


#ifndef KERNEL

#if (__freebsd__ > 0)
#include <sys/ioccom.h>
#endif /* (__freebsd__ > 0) */

#if (__linux__ > 0)
#include <sys/ioctl.h>
#endif /* (__linux__ > 0) */

#if (__solaris__ > 0)
#include <sys/ioccom.h>
#endif /* (__solaris__ > 0) */

#endif /* !defined(KERNEL) */

/*---------------------------------------------------------------*/

/* Files can be opened in concurrent write mode using the
 * following flag to open(). */
#define O_CONCURRENT_WRITE 020000000000

/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

#define PAN_FS_CLIENT_CW_MODE  'C'

#define PAN_FS_CLIENT_CW_MODE_FLUSH_F__NONE        0x00000000
#define PAN_FS_CLIENT_CW_MODE_FLUSH_F__INVAL_BUFS  0x00000001
#define PAN_FS_CLIENT_CW_MODE_FLUSH_F__INVAL_ATTRS 0x00000002

typedef struct pan_fs_client_cw_mode_flush_args_s pan_fs_client_cw_mode_flush_args_t;
struct pan_fs_client_cw_mode_flush_args_s {
  unsigned int     flags;
};

/**
 * An ioctl for flushing cached writes to stable storage. The ioctl can also be
 * used to invalidate the contents of client's buffer cache after flushing.
 * 
 * @author  ehogan
 * @version 1.0
 *
 * @since   1.0
 */
#define PAN_FS_CLIENT_CW_MODE_FLUSH \
  _IOWR((unsigned int)(PAN_FS_CLIENT_CW_MODE),1,pan_fs_client_cw_mode_flush_args_t)

/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

#define PAN_FS_CLIENT_LAYOUT 'L'

#define PAN_FS_CLIENT_CW_MODE_FILE_NAME_LEN_MAX 255

#define PAN_FS_CLIENT_LAYOUT_VERSION  1

enum pan_fs_client_layout_agg_type_e {
  PAN_FS_CLIENT_LAYOUT_TYPE__INVALID      = 0,
  PAN_FS_CLIENT_LAYOUT_TYPE__DEFAULT      = 1,
  PAN_FS_CLIENT_LAYOUT_TYPE__RAID0        = 2,
  PAN_FS_CLIENT_LAYOUT_TYPE__RAID1        = 3,
  PAN_FS_CLIENT_LAYOUT_TYPE__RAID1_5      = 4,
  PAN_FS_CLIENT_LAYOUT_TYPE__RAIDGRP1_5   = 5
};
typedef enum pan_fs_client_layout_agg_type_e pan_fs_client_layout_agg_type_t;

enum pan_fs_client_layout_raidgrp_e {
  PAN_FS_CLIENT_LAYOUT_RAIDGRP__INVALID       = 0,
  PAN_FS_CLIENT_LAYOUT_RAIDGRP__ROUND_ROBIN   = 1
};
typedef enum pan_fs_client_layout_raidgrp_e pan_fs_client_layout_raidgrp_t;

typedef struct pan_fs_client_layout_s pan_fs_client_layout_t;
struct pan_fs_client_layout_s {
  pan_fs_client_layout_agg_type_t  agg_type;
  int                              layout_is_valid;
  union {
    struct {
      unsigned char                    num_comps_data_stripe;
      unsigned int                     stripe_unit;
    } raid0;
    struct {
      unsigned char                    num_comps_data_stripe;
    } raid1;
    struct {
      unsigned char                    num_comps_data_stripe;
      unsigned int                     stripe_unit;
    } raid1_5;
    struct {
      unsigned int                     total_num_comps;
      unsigned int                     stripe_unit;
      unsigned int                     raidgrp_width;
      unsigned int                     raidgrp_depth;
      pan_fs_client_layout_raidgrp_t   raidgrp_layout_policy;
    } raidgrp1_5;
  } u;
};

typedef struct pan_fs_client_layout_create_args_s pan_fs_client_layout_create_args_t;
struct pan_fs_client_layout_create_args_s {
  unsigned short                   version;
  char                             filename[PAN_FS_CLIENT_CW_MODE_FILE_NAME_LEN_MAX+1];
  unsigned int                     mode;
  pan_fs_client_layout_t           layout;
};

typedef struct pan_fs_client_layout_create_dir_args_s pan_fs_client_layout_create_dir_args_t;
struct pan_fs_client_layout_create_dir_args_s {
  unsigned short                   version;
  char                             dirname[PAN_FS_CLIENT_CW_MODE_FILE_NAME_LEN_MAX+1];
  unsigned int                     mode;
  pan_fs_client_layout_t           child_layout;
};

typedef struct pan_fs_client_layout_query_args_s pan_fs_client_layout_query_args_t;
struct pan_fs_client_layout_query_args_s {
  unsigned short                   version;
  pan_fs_client_layout_t           layout;
};

/**
 * An ioctl that can be used to create files of a specified storage layout
 * in a PanFS Filesystem. The target of the ioctl should be a directory in
 * which to create the new file.
 * 
 * @author  ehogan
 * @version 1.0
 *
 * @since   1.0
 */
#define PAN_FS_CLIENT_LAYOUT_CREATE_FILE \
  _IOWR((unsigned int)(PAN_FS_CLIENT_LAYOUT),1,pan_fs_client_layout_create_args_t)

/**
 * An ioctl that can be used to create a dir in which all future child files
 * created will be of a specified layout. This ioctl does not alter the layout
 * of the directory itself.
 * 
 * @author  ehogan
 * @version 1.0
 *
 * @since   1.0
 */
#define PAN_FS_CLIENT_LAYOUT_CREATE_DIR \
  _IOWR((unsigned int)(PAN_FS_CLIENT_LAYOUT),2,pan_fs_client_layout_create_dir_args_t)

/**
 * An ioctl that can be used to query the storage layout of a filesystem object
 * 
 * @author  ehogan
 * @version 1.0
 *
 * @since   1.0
 */
#define PAN_FS_CLIENT_LAYOUT_QUERY_FILE \
  _IOWR((unsigned int)(PAN_FS_CLIENT_LAYOUT),3,pan_fs_client_layout_query_args_t)

#endif /* _PAN_FS_CLIENT__PAN_FS_CLIENT_CW_MODE_H_ */

/* Local Variables:  */
/* indent-tabs-mode: nil */
/* tab-width: 2 */
/* End: */

