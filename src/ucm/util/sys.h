/**
 * Copyright (C) Mellanox Technologies Ltd. 2001-2016.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */


#ifndef UCM_UTIL_SYS_H_
#define UCM_UTIL_SYS_H_

#include <stddef.h>

/**
 * @brief Get the size of a shared memory segment, attached with shmat()
 *
 * @param [in]  shmaddr  Segment pointer.
 * @return Segment size, or 0 if not found.
 */
size_t ucm_get_shm_seg_size(const void *shmaddr);


#endif
