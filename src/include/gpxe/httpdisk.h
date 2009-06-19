#ifndef _GPXE_HTTPDISK_H
#define _GPXE_HTTPDISK_H

/*
 * Copyright (C) 2009 Shao Miller <shao.miller@yrdsb.edu.on.ca>.
 * Derived from works (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/** @file
 *
 * HTTPDisk devices and protocol
 *
 * HTTPDisk is a read-only SAN protocol (for now)
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/refcnt.h>
#include <gpxe/xfer.h>
#include <gpxe/blockdev.h>

/* FIXME: This isn't really right */
/* "Range: bytes=123456789ABCDEFG-123456789ABCDEFG\r\n" */
#define HTTPDISK_RANGEHDR_BUFSIZE	13 + 16 + 1 + 16 + 2

/** An HTTPDisk device */
struct httpdisk_device {
	/** Reference counter */
	struct refcnt refcnt;

	/** Data transfer interface */
	struct xfer_interface xfer;

	/** Block device interface */
	struct block_device blockdev;
	/** Block device total length, filled-in by HTTP response */
	unsigned long blockdev_len;

	/** URI for disk image */
	struct uri *uri;

	/** Buffer */
	userptr_t buffer;
	/** Buffer length */
	size_t buflen;
	/** Current buffer position */
	size_t pos;

	/** Range header passed in HTTP request */
	char *rangehdr;

	/** Completion status */
	int done;
};

extern int httpdisk_attach ( struct httpdisk_device *httpdisk, const char *root_path );
extern void httpdisk_detach ( struct httpdisk_device *httpdisk );
extern int init_httpdisk ( struct httpdisk_device *httpdisk, unsigned int blksize );

#endif /* _GPXE_HTTPDISK_H */
