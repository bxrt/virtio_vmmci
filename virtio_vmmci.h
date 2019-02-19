/*
 *  Implementation of an OpenBSD VMM control interface for Linux guests
 *  running under an OpenBSD host.
 *
 *  Copyright 2019 Dave Voutila
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define VIRTIO_ID_VMMCI			0xffff	/* matches OpenBSD's private id */

#define PCI_VENDOR_ID_OPENBSD_VMM	0x0b5d
#define PCI_DEVICE_ID_OPENBSD_VMMCI	0x0777

/* Features */
#define VMMCI_F_TIMESYNC		(1<<0)
#define VMMCI_F_ACK			(1<<1)
#define VMMCI_F_SYNCRTC			(1<<2)
