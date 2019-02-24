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

#include <linux/module.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/timekeeping.h>
#include <linux/time64.h>
#include "virtio_vmmci.h"

u8 debug = 0;

#define debug(fmt, ...) \
	do { if (debug) pr_info("virtio_vmmci: [%s] " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#define log(fmt, ...) pr_info("virtio_vmmci: " fmt, ##__VA_ARGS__)

static const char *QNAME = "vmmci-wq";
static const s64 MAX_DRIFT_SEC = 5;
static const int DELAY_10s = HZ * 5;
static const int DELAY_1s = HZ / HZ;

struct virtio_vmmci {
	struct virtio_device *vdev;
	struct workqueue_struct *clock_wq;
	struct delayed_work clock_work;
};

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_VMMCI, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {
	VMMCI_F_TIMESYNC, VMMCI_F_ACK, VMMCI_F_SYNCRTC,
};

static int vmmci_validate(struct virtio_device *vdev)
{
	debug("not implemented");
	return 0;
}

static void clock_work_func(struct work_struct *work)
{
	struct virtio_vmmci *vmmci;
	struct timespec64 host, guest, diff;
	u64 sec, usec;

	debug("starting clock synchronization\n");

	// My god this container_of stuff seems...messy? Oh, Linux...
	vmmci = container_of((struct delayed_work *) work, struct virtio_vmmci, clock_work);

	vmmci->vdev->config->get(vmmci->vdev, VMMCI_CONFIG_TIME_SEC, &sec, sizeof(sec));
	vmmci->vdev->config->get(vmmci->vdev, VMMCI_CONFIG_TIME_USEC, &usec, sizeof(usec));
	getnstimeofday64(&guest);

	debug("guest clock: %lld.%ld, host clock: %lld.%ld",
	    sec, (long) usec * NSEC_PER_USEC, guest.tv_sec, (long) guest.tv_nsec);

	host.tv_sec = sec;
	host.tv_nsec = usec * NSEC_PER_USEC;

	diff = timespec64_sub(host, guest);

	debug("current time delta: %lld.%ld\n", diff.tv_sec, diff.tv_nsec);

	if (diff.tv_sec < -MAX_DRIFT_SEC || diff.tv_sec > MAX_DRIFT_SEC) {
		log("detected drift greater than %lld seconds, synchronizing clock\n",
		    MAX_DRIFT_SEC);
		if(do_settimeofday64(&host)) {
			printk(KERN_ERR "error setting system clock to host!\n");
			// XXX: not sure how we'd reach here other than `diff`
			// being malformed
		}
	}

	queue_delayed_work(vmmci->clock_wq, &vmmci->clock_work, DELAY_10s);
	debug("clock synchronization routine finished\n");
}

static int vmmci_probe(struct virtio_device *vdev)
{
	struct virtio_vmmci *vmmci;

	debug("initializing vmmci device\n");

	vdev->priv = vmmci = kzalloc(sizeof(*vmmci), GFP_KERNEL);
	if (!vmmci) {
		printk(KERN_ERR "vmmci_probe: failed to alloc vmmci struct\n");
		return -ENOMEM;
	}
	vmmci->vdev = vdev;

	if (debug && virtio_has_feature(vdev, VMMCI_F_TIMESYNC))
		debug("...found feature TIMESYNC\n");
	if (debug && virtio_has_feature(vdev, VMMCI_F_ACK))
		debug("...found feature ACK\n");
	if (debug && virtio_has_feature(vdev, VMMCI_F_SYNCRTC))
		debug("...found feature SYNCRTC\n");

	vmmci->clock_wq = create_singlethread_workqueue(QNAME);
	if (vmmci->clock_wq == NULL) {
		printk(KERN_ERR "vmmci_probe: failed to alloc workqueue\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&vmmci->clock_work, clock_work_func);
	queue_delayed_work(vmmci->clock_wq, &vmmci->clock_work, DELAY_1s);

	log("started VMM Control Interface driver\n");
	return 0;
}

static void vmmci_remove(struct virtio_device *vdev)
{
	struct virtio_vmmci *vmmci = vdev->priv;
	debug("removing device\n");

	cancel_delayed_work(&vmmci->clock_work);
	flush_workqueue(vmmci->clock_wq);
	destroy_workqueue(vmmci->clock_wq);
	debug("cancelled, flushed, and destroyed work queues\n");

	vdev->config->reset(vdev);
        debug("reset device\n");

	kfree(vmmci);

	log("removed device\n");
}

static void vmmci_changed(struct virtio_device *vdev)
{
	debug("not implemented\n");
}

#ifdef CONFIG_PM_SLEEP
static int vmmci_freeze(struct virtio_device *vdev)
{
	debug("not implemented\n");
	return 0;
}

static int vmmci_restore(struct virtio_device *vdev)
{
	debug("not implemented\n");
	return 0;
}
#endif

static struct virtio_driver virtio_vmmci_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = 	KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = 	id_table,
	.validate = 	vmmci_validate,
	.probe = 	vmmci_probe,
	.remove = 	vmmci_remove,
	.config_changed = vmmci_changed,
#ifdef CONFIG_PM_SLEEP
	.freeze = 	vmmci_freeze,
	.restore = 	vmmci_restore,
#endif
};

module_virtio_driver(virtio_vmmci_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OpenBSD VMM Control Interface");
MODULE_AUTHOR("Dave Voutila <voutilad@gmail.com>");
