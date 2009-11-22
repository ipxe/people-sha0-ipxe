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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/uaccess.h>
#include <gpxe/umalloc.h>
#include <gpxe/features.h>
#include <gpxe/uri.h>
#include <gpxe/xfer.h>
#include <gpxe/http.h>
#include <gpxe/httpdisk.h>
#include <gpxe/process.h>

/** @file
 *
 * HTTPDisk devices and protocol
 *
 * HTTPDisk is a read-only SAN protocol (for now)
 */

FEATURE ( FEATURE_PROTOCOL, "HTTPDisk", DHCP_EB_FEATURE_HTTPDISK, 1 );

/****************************************************************************
 *
 * HTTPDisk data interface
 *
 */

/**
 * Handle deliver_raw() event received via data transfer interface
 *
 * @v xfer		Downloader data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int httpdisk_xfer_deliver_iob ( struct xfer_interface *xfer,
					 struct io_buffer *iobuf,
					 struct xfer_metadata *meta ) {
	struct httpdisk_device *httpdisk =
		container_of ( xfer, struct httpdisk_device, xfer );
	size_t len;
	size_t max;
	int rc = 0;

	/* Calculate new buffer position */
	if ( meta->whence != SEEK_CUR )
		httpdisk->pos = 0;
	httpdisk->pos += meta->offset;

	/* Ensure that we have enough buffer space for this data */
	len = iob_len ( iobuf );
	max = ( httpdisk->pos + len );
	DBGC ( httpdisk, "HTTPDisk: pos=%d, max=%d, iob_len=%d, buflen=%d\n",
	       httpdisk->pos, max, len, httpdisk->buflen );
	if ( max > httpdisk->buflen ) {
		/* Copy what we can, but notify */
		len = httpdisk->buflen - httpdisk->pos;
		DBGC ( httpdisk, "HTTPDisk: iobuf would overflow buffer, dropping excess\n" );
		rc = -ENOBUFS;
	}

	/* Copy data to buffer */
	copy_to_user ( httpdisk->buffer, httpdisk->pos,
		       iobuf->data, len );

	/* Update current buffer position */
	httpdisk->pos += len;

	free_iob ( iobuf );
	return rc;
}

/**
 * Handle the data transfer interface closure
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 *
 */
static void httpdisk_xfer_close ( struct xfer_interface *xfer, int rc __unused ) {
	struct httpdisk_device *httpdisk =
		container_of ( xfer, struct httpdisk_device, xfer );
	DBGC ( httpdisk, "HTTPDisk: Closing data interface\n" );
	httpdisk->done = 1;
}

/** HTTPDisk data transfer interface operations */
static struct xfer_interface_operations httpdisk_xfer_operations = {
	.close		= httpdisk_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= httpdisk_xfer_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};



/****************************************************************************
 *
 * HTTPDisk block device interface
 *
 */

/**
 * Read block
 *
 * @v blockdev		Block device
 * @v block		Block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int httpdisk_read ( struct block_device *blockdev, uint64_t block,
			  unsigned long count, userptr_t buffer ) {
	struct httpdisk_device *httpdisk = container_of ( blockdev,
							  struct httpdisk_device,
							  blockdev );
	int rc;
	unsigned long offset = ( block * blockdev->blksize );
	httpdisk->buflen = ( count * blockdev->blksize );
	httpdisk->pos = 0;

	DBGC ( httpdisk, "HTTPDisk %p: Reading [%lx,%lx)\n",
	       httpdisk, offset, (unsigned long)httpdisk->buflen );

	/**
	 * Zero out any previous range header, then fill it in.
	 * httpdisk_attach() is responsible for allocating rangehdr, so there
	 * should not be a chance of it being NULL
	 */
	/* TODO: Find out if sprintf() trails a 0 byte itself; I forget */
	memset ( httpdisk->rangehdr, 0, HTTPDISK_RANGEHDR_BUFSIZE );
	if ( sprintf ( httpdisk->rangehdr, "Range: bytes=%d-%d\r\n",
		       (unsigned int)offset,
		       (unsigned int)( offset + httpdisk->buflen - 1 ) ) < 18 )
		return -EINVAL;
	DBGC ( httpdisk, "HTTPDisk: rangehdr: %s\n", httpdisk->rangehdr );

	/* Point to our buffer and begin the data transfer */
	httpdisk->buffer = buffer;
	if ( ( rc = http_open_filter ( &httpdisk->xfer, httpdisk->uri,
				       httpdisk->rangehdr, UNULL,
				       HTTP_PORT, NULL ) ) != 0 )
		return rc;
	httpdisk->done = 0;
	while ( ! httpdisk->done ) {
		DBGC ( httpdisk, "HTTPDisk: Waiting for xfer %p\n", &httpdisk->xfer );
		step();
	}
	if ( httpdisk->pos != httpdisk->buflen )
		return -EIO;
	return 0;
}

/**
 * Write block (not supported)
 *
 * @v blockdev		Block device
 * @v block		Block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code ("Not Supported")
 */
static int httpdisk_write ( struct block_device *blockdev  __unused,
			   uint64_t block __unused,
			   unsigned long count __unused,
			   userptr_t buffer __unused ) {
	return -ENOTSUP;
}

static struct block_device_operations httpdisk_blockdev_operations = {
	.read	= httpdisk_read,
	.write	= httpdisk_write
};

/****************************************************************************
 *
 * HTTPDisk device routines
 *
 */

/**
 * Free HTTPDisk device
 *
 * @v refcnt		Reference counter
 */
static void httpdisk_free ( struct refcnt *refcnt ) {
	struct httpdisk_device *httpdisk =
		container_of ( refcnt, struct httpdisk_device, refcnt );
	free ( httpdisk->uri );
	free ( httpdisk );
}

/**
 * Attach HTTPDisk interface
 *
 * @v httpdisk		HTTPDisk device
 * @v root_path		Disk URI (e.g. http://etherboot.org/httpdisk.hdd)
 * @ret rc		Return status code
 */
int httpdisk_attach ( struct httpdisk_device *httpdisk, const char *root_path ) {
	httpdisk->refcnt.free = httpdisk_free;
	xfer_init ( &httpdisk->xfer, &httpdisk_xfer_operations, &httpdisk->refcnt );

	/* Set disk URI */
	httpdisk->uri = parse_uri ( root_path );

	/* Sanity checks */
	if ( ! httpdisk->uri ) {
		DBGC ( httpdisk, "HTTPDisk %p: root-path coult not be parsed\n",
		       httpdisk );
		ref_put ( &httpdisk->refcnt );
		return -ENOTSUP;
	}

	httpdisk->rangehdr = malloc ( HTTPDISK_RANGEHDR_BUFSIZE );
	if ( ! httpdisk->rangehdr ) {
		ref_put ( &httpdisk->refcnt );
		free ( httpdisk->uri );
		return -ENOMEM;
	}

	return 0;
}

/**
 * Shut down HTTPDisk device
 *
 * @v httpdisk		HTTPDisk device
 */
void httpdisk_detach ( struct httpdisk_device *httpdisk ) {
	xfer_nullify ( &httpdisk->xfer );
	xfer_close ( &httpdisk->xfer, httpdisk->done );
}

/**
 * Initialize HTTPDisk
 *
 * @v httpdisk		HTTPDisk device
 * @v blksize		Block size for block device interface
 * @ret rc		Return status code
 */
int init_httpdisk ( struct httpdisk_device *httpdisk, unsigned int blksize ) {
	int rc;

	if ( ! blksize )
		blksize = 512;

	httpdisk->blockdev.op = &httpdisk_blockdev_operations;
	httpdisk->blockdev.blksize = blksize;
	/**
	 * TODO: In order to determine the block device's total size, we must
	 * send a dummy request to the HTTP server.  The server will give us
	 * a content-range header in the response, the HTTP interface will
	 * store that away at httpdisk->blockdev_len, and finally we can
	 * hand it to the block device.  Discuss.
	 */
	httpdisk->buffer = umalloc( 1 );	/* Dummy buffer */
	httpdisk->buflen = 1;
	httpdisk->pos = 0;
	if ( httpdisk->buffer == UNULL )
		return -ENOMEM;
	if ( ( rc = http_open_filter ( &httpdisk->xfer, httpdisk->uri,
				       "Range: bytes=0-0\r\n",
				       &httpdisk->blockdev_len,
				       HTTP_PORT, NULL ) ) != 0 )
		return rc;
	httpdisk->done = 0;
	while ( ! httpdisk->done ) {
		DBGC ( httpdisk, "HTTPDisk: Waiting for xfer %p\n",
		       &httpdisk->xfer );
		step();
	}
	ufree ( httpdisk->buffer );
	if ( httpdisk->pos != httpdisk->buflen )
		return -EIO;
	/**
	 * TODO: With any luck, we've successfully received some data from
	 * the HTTP resource, and thus know the total size of the resource.
	 * We store it away for future block device queries.  Discuss.
	 */
	if ( httpdisk->blockdev_len )
		httpdisk->blockdev.blocks = ( httpdisk->blockdev_len /
					      httpdisk->blockdev.blksize );
	return 0;
}
