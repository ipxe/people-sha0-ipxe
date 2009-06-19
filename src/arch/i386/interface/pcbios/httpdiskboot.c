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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/httpdisk.h>
#include <gpxe/settings.h>
#include <gpxe/dhcp.h>
#include <gpxe/netdevice.h>
/**
 * TODO: Develop hBFT
 *
 * #include <gpxe/hbft.h>
 */
#include <gpxe/init.h>
#include <gpxe/sanboot.h>
#include <int13.h>
#include <usr/autoboot.h>

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * NOTE: Moved keep-san somewhere where all SAN protocols can get it.
 * AoE is interesting because it can be used from script/CLI without
 * ever establishing IP parameters, thus the keep-san option might
 * not be available without also having been set from script/CLI.
 */

static int httpdiskboot ( const char *root_path ) {
	struct httpdisk_device *httpdisk;
	struct int13_drive *drive;
	int keep_san;
	int rc;

	httpdisk = zalloc ( sizeof ( *httpdisk ) );
	if ( ! httpdisk ) {
		rc = -ENOMEM;
		goto err_alloc_httpdisk;
	}
	drive = zalloc ( sizeof ( *drive ) );
	if ( ! drive ) {
		rc = -ENOMEM;
		goto err_alloc_drive;
	}

	printf ( "HTTPDisk booting from %s\n", root_path );

	if ( ( rc = httpdisk_attach ( httpdisk, root_path ) ) != 0 ) {
		printf ( "Could not attach HTTPDisk device: %s\n",
			 strerror ( rc ) );
		goto err_attach;
	}
	if ( ( rc = init_httpdisk ( httpdisk, 0 ) ) != 0 ) {
		printf ( "Could not initialise HTTPDisk device: %s\n",
			 strerror ( rc ) );
		goto err_init;
	}

	drive->blockdev = &httpdisk->blockdev;

	/**
	 * TODO: Consider filling an hBFT.  Might need to include
	 * IP and/or MAC address in order to be useful.  'httpdisk'
	 * for Windows as well as Michael Brown's 'sanbootconf' could
	 * be hacked to support using an hBFT, except that an 'httpdisk'
	 * is read-only so far...
	 *
	 * hbft_fill_data ( httpdisk );
	 */

	register_int13_drive ( drive );
	printf ( "Registered as BIOS drive %#02x\n", drive->drive );
	printf ( "Booting from BIOS drive %#02x\n", drive->drive );
	rc = int13_boot ( drive->drive );
	printf ( "Boot failed\n" );

	/* Leave drive registered, if instructed to do so */
	keep_san = fetch_intz_setting ( NULL, &keep_san_setting );
	if ( keep_san ) {
		printf ( "Preserving connection to SAN disk\n" );
		shutdown_exit_flags |= SHUTDOWN_KEEP_DEVICES;
		return rc;
	}

	printf ( "Unregistering BIOS drive %#02x\n", drive->drive );
	unregister_int13_drive ( drive );

 err_init:
	httpdisk_detach ( httpdisk );
 err_attach:
	free ( drive );
 err_alloc_drive:
	free ( httpdisk );
 err_alloc_httpdisk:
	return rc;
}

struct sanboot_protocol httpdisk_sanboot_protocol __sanboot_protocol = {
	.prefix = "http:",
	.boot = httpdiskboot,
};
