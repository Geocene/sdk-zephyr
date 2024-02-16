/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <zephyr/fs/fcb.h>
#include "fcb_priv.h"

int
fcb_getnext_in_sector(struct fcb *fcb, struct fcb_entry *loc)
{
	int rc;

	rc = fcb_elem_info(fcb, loc);
	if (rc == 0 || rc == -EBADMSG) {
		do {
			loc->fe_elem_off = loc->fe_data_off +
			  fcb_len_in_flash(fcb, loc->fe_data_len) +
			  fcb_len_in_flash(fcb, FCB_CRC_SZ);
			rc = fcb_elem_info(fcb, loc);
			if (rc != -EBADMSG) {
				break;
			}
		} while (rc == -EBADMSG);
	}
	return rc;
}

int
fcb_getnext_sector_idx(struct fcb *fcb, int sector_idx)
{
	sector_idx++;
	if (sector_idx >= fcb->f_sector_cnt) {
		sector_idx = 0;
	}
	return sector_idx;
}

off_t fcb_getnext_sector_offset(struct fcb *fcb, struct flash_sector *sector)
{
	off_t offset;
	int sector_idx = fcb_get_sector_idx(fcb, sector);
	sector_idx++;
	if (sector_idx >= fcb->f_sector_cnt) {
		sector_idx = 0;
	}
	offset = sector_idx * fcb->f_sector_size;

	return offset;
}

int
fcb_getnext_nolock(struct fcb *fcb, struct fcb_entry *loc)
{
	int rc;

	if (loc->fe_sector.fs_off < 0) {
		/*
		 * Find the first one we have in flash.
		 */
		loc->fe_sector = fcb->f_oldest;
	}
	if (loc->fe_elem_off == 0U) {
		/*
		 * If offset is zero, we serve the first entry from the sector.
		 */
		loc->fe_elem_off = fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
		rc = fcb_elem_info(fcb, loc);
		switch (rc) {
		case 0:
			return 0;
		case -EBADMSG:
			break;
		default:
			goto next_sector;
		}
	} else {
		rc = fcb_getnext_in_sector(fcb, loc);
		if (rc == 0) {
			return 0;
		}
		if (rc == -ENOTSUP) {
			goto next_sector;
		}
	}
	while (rc == -EBADMSG) {
		rc = fcb_getnext_in_sector(fcb, loc);
		if (rc == 0) {
			return 0;
		}

		if (rc != -EBADMSG) {
			/*
			 * Moving to next sector.
			 */
next_sector:
			if (fcb_get_sector_idx(fcb, &loc->fe_sector) == fcb_get_sector_idx(fcb, &fcb->f_active.fe_sector)) {
				return -ENOTSUP;
			}
			
			loc->fe_sector.fs_off = fcb_getnext_sector_offset(fcb, &loc->fe_sector);
			loc->fe_sector.fs_size = fcb->f_sector_size;
			loc->fe_elem_off = fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
			rc = fcb_elem_info(fcb, loc);
			switch (rc) {
			case 0:
				return 0;
			case -EBADMSG:
				break;
			default:
				goto next_sector;
			}
		}
	}

	return 0;
}

int
fcb_getnext(struct fcb *fcb, struct fcb_entry *loc)
{
	int rc;

	rc = k_mutex_lock(&fcb->f_mtx, K_FOREVER);
	if (rc) {
		return -EINVAL;
	}
	rc = fcb_getnext_nolock(fcb, loc);
	k_mutex_unlock(&fcb->f_mtx);

	return rc;
}
