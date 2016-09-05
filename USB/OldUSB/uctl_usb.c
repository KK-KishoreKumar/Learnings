#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>

#include <linux/usb.h>

#include "uctl_usb.h"

static struct uctl_device uctl_dev[MAX_INTERFACES];
#ifdef ENABLE_PROCESS_EPS
static int is_active[MAX_INTERFACES];
#endif
static int buf_left_over[MAX_INTERFACES][2];

#ifdef ENABLE_FILE_OPS
int uctl_open(struct inode *i, struct file *f)
{
	if (!is_active[INTERFACE(iminor(i))])
	{
		printk("USB connection lost. Failing to open\n");
		return -1;
	}
	buf_left_over[INTERFACE(iminor(i))][EP_IN] = 0;
	buf_left_over[INTERFACE(iminor(i))][EP_OUT] = 0;
	f->private_data = (void *)INTERFACE(iminor(i));
	return 0;
}
int uctl_close(struct inode *i, struct file *f)
{
	if (!is_active[INTERFACE(iminor(i))])
	{
		printk("USB connection lost. Close cleanly\n");
	}
	return 0;
}
ssize_t uctl_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
	long intf_no = (long)(f->private_data);
	struct uctl_device *dev = &uctl_dev[intf_no];
	int read_size, read_cnt;
	int retval;
	int i;

	if (!is_active[intf_no])
	{
		printk("USB connection lost. Failing to read\n");
		return -ENODEV;
	}
	if ((dev->attrib[EP_IN] != USB_ENDPOINT_XFER_BULK) || !dev->ep[EP_IN])
	{
		return -EINVAL;
	}
	printk("Read request for %ld bytes\n", cnt);
	/* Check for left over data */
	if (buf_left_over[TX_INTF][EP_IN])
	{
		read_size = buf_left_over[TX_INTF][EP_IN];
		buf_left_over[TX_INTF][EP_IN] = 0;
	}
	else
	{
		/* Read the data in the bulk port */
		/* Using buf may cause sync issues */
		retval = usb_bulk_msg(dev->device, usb_rcvbulkpipe(dev->device, dev->ep[EP_IN]),
			dev->buf[EP_IN], dev->buf_size[EP_IN], &read_size, 0);
		if (retval)
		{
			printk("Bulk message returned %d\n", retval);
			return retval;
		}
	}
	if (read_size <= cnt)
	{
		read_cnt = read_size;
	}
	else
	{
		read_cnt = cnt;
	}
	if (copy_to_user(buf, dev->buf[EP_IN], read_cnt))
	{
		buf_left_over[TX_INTF][EP_IN] = read_size;
		return -EFAULT;
	}
	for (i = cnt; i < read_size; i++)
	{
		dev->buf[EP_IN][i - cnt] = dev->buf[EP_IN][i];
	}
	if (cnt < read_size)
	{
		buf_left_over[TX_INTF][EP_IN] = read_size - cnt;
	}
	else
	{
		buf_left_over[TX_INTF][EP_IN] = 0;
	}
	printk("Actually read %d bytes (Sent to user: %d)\n", read_size, read_cnt);

	return read_cnt;
}
ssize_t uctl_write(struct file *f, const char __user *buf, size_t cnt, loff_t *off)
{
	long intf_no = (long)(f->private_data);
	struct uctl_device *dev = &uctl_dev[intf_no];
	int write_size, wrote_size, wrote_cnt;
	int retval;

	if (!is_active[intf_no])
	{
		printk("USB connection lost. Failing to write\n");
		return -ENODEV;
	}
	if ((dev->attrib[EP_OUT] != USB_ENDPOINT_XFER_BULK) || !dev->ep[EP_OUT])
	{
		return -EINVAL;
	}
	wrote_cnt = 0;
	while (wrote_cnt < cnt)
	{
		write_size = MIN(dev->buf_size[EP_OUT], cnt - wrote_cnt /* Remaining */);
		/* Using buf may cause sync issues */
		if (copy_from_user(dev->buf[EP_OUT], buf + wrote_cnt, write_size))
		{
			return -EFAULT;
		}
		/* Send the data out the bulk port */
		retval = usb_bulk_msg(dev->device, usb_sndbulkpipe(dev->device, dev->ep[EP_OUT]),
			dev->buf[EP_OUT], write_size, &wrote_size, 0);
		if (retval)
		{
			printk("Bulk message returned %d\n", retval);
			return retval;
		}
		wrote_cnt += wrote_size;
		printk("Wrote %d bytes\n", wrote_size);
	}

	return wrote_cnt;
}
#endif

#ifdef ENABLE_PROCESS_EPS
static struct file_operations fops =
{
#ifdef ENABLE_FILE_OPS
	.open = uctl_open,
	.release = uctl_close,
	.read = uctl_read,
	.write = uctl_write,
#endif
};
#endif

static void uctl_uninit(struct usb_interface *interface)
{
	printk("uCtl USB %d now uninited\n", interface->minor);
#ifdef ENABLE_PROCESS_EPS
	is_active[INTERFACE(interface->minor)] = 0;
	/* Prevent uctl_open() from racing uctl_disconnect() */
	lock_kernel();

	/* Give back our minor */
	usb_deregister_dev(interface, &uctl_dev[INTERFACE(interface->minor)].class);

	unlock_kernel();

	if (uctl_dev[INTERFACE(interface->minor)].buf[EP_OUT])
	{
		kfree(uctl_dev[INTERFACE(interface->minor)].buf[EP_OUT]);
	}
	if (uctl_dev[INTERFACE(interface->minor)].buf[EP_IN])
	{
		kfree(uctl_dev[INTERFACE(interface->minor)].buf[EP_IN]);
	}
#endif
	memset(&uctl_dev[INTERFACE(interface->minor)], 0, sizeof(uctl_dev[INTERFACE(interface->minor)]));
	/* TODO */
	usb_set_intfdata(interface, NULL);
}

int uctl_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_host_interface *iface_desc;
#ifdef ENABLE_PROCESS_EPS
	struct usb_endpoint_descriptor *endpoint;
	int i;
	int retval;
#endif
	static int minor = 0;

	// Minor needs to be updated
	interface->minor = minor++;
	printk("uCtl USB now probed: (%04X:%04X); Minor: %d\n", id->idVendor, id->idProduct, interface->minor);
	printk("Alt Setting: (%d, %p)\n", interface->num_altsetting, interface->altsetting);
	printk("Current Alt Setting: (%p)\n", interface->cur_altsetting);

	//usb_set_intfdata(interface, &uctl_dev[INTERFACE(interface->minor)]);

	iface_desc = interface->cur_altsetting;
	printk("ID->bLength: %02X\n", iface_desc->desc.bLength);
	printk("ID->bDescriptorType: %02X\n", iface_desc->desc.bDescriptorType);
	printk("ID->bInterfaceNumber: %02X\n", iface_desc->desc.bInterfaceNumber);
	printk("ID->bAlternateSetting: %02X\n", iface_desc->desc.bAlternateSetting);
	printk("ID->bNumEndpoints: %02X\n", iface_desc->desc.bNumEndpoints);
	printk("ID->bInterfaceClass: %02X\n", iface_desc->desc.bInterfaceClass);
	printk("ID->bInterfaceSubClass: %02X\n", iface_desc->desc.bInterfaceSubClass);
	printk("ID->bInterfaceProtocol: %02X\n", iface_desc->desc.bInterfaceProtocol);
	printk("ID->iInterface: %02X\n", iface_desc->desc.iInterface);

#ifdef ENABLE_PROCESS_EPS
	/* Set up the endpoint information */
	/* Use only the first in and out endpoints */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
	{
		endpoint = &iface_desc->endpoint[i].desc;
		printk("ED->bLength: %02X\n", endpoint->bLength);
		printk("ED->bDescriptorType: %02X\n", endpoint->bDescriptorType);

		printk("ED->bEndpointAddress: %02X\n", endpoint->bEndpointAddress);
		printk("ED->bmAttributes: %02X\n", endpoint->bmAttributes);
		printk("ED->wMaxPacketSize: %04X (%d)\n", endpoint->wMaxPacketSize, endpoint->wMaxPacketSize);
		printk("ED->bInterval: %02X\n", endpoint->bInterval);

		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT)
		{
			if (endpoint->bEndpointAddress & USB_DIR_IN)
			{
				if (!uctl_dev[INTERFACE(interface->minor)].ep[EP_IN])
				{
					uctl_dev[INTERFACE(interface->minor)].ep[EP_IN] = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
					uctl_dev[INTERFACE(interface->minor)].attrib[EP_IN] = USB_ENDPOINT_XFER_INT;
				}
			}
			else
			{
				if (!uctl_dev[INTERFACE(interface->minor)].ep[EP_OUT])
				{
					uctl_dev[INTERFACE(interface->minor)].ep[EP_OUT] = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
					uctl_dev[INTERFACE(interface->minor)].attrib[EP_OUT] = USB_ENDPOINT_XFER_INT;
				}
			}
		}
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)
		{
			if (endpoint->bEndpointAddress & USB_DIR_IN)
			{
				if (!uctl_dev[INTERFACE(interface->minor)].ep[EP_IN])
				{
					uctl_dev[INTERFACE(interface->minor)].ep[EP_IN] = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
					uctl_dev[INTERFACE(interface->minor)].attrib[EP_IN] = USB_ENDPOINT_XFER_BULK;
					uctl_dev[INTERFACE(interface->minor)].buf_size[EP_IN] = endpoint->wMaxPacketSize;
					uctl_dev[INTERFACE(interface->minor)].buf[EP_IN] = kmalloc(endpoint->wMaxPacketSize, GFP_KERNEL);
					if (!uctl_dev[INTERFACE(interface->minor)].buf[EP_IN])
					{
						err("Not able to get memory for in ep buffer of this device.");
						uctl_uninit(interface);
						return -ENOMEM;
					}
					buf_left_over[INTERFACE(interface->minor)][EP_IN] = 0;
				}
			}
			else
			{
				if (!uctl_dev[INTERFACE(interface->minor)].ep[EP_OUT])
				{
					uctl_dev[INTERFACE(interface->minor)].ep[EP_OUT] = endpoint->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
					uctl_dev[INTERFACE(interface->minor)].attrib[EP_OUT] = USB_ENDPOINT_XFER_BULK;
					uctl_dev[INTERFACE(interface->minor)].buf_size[EP_OUT] = endpoint->wMaxPacketSize;
					uctl_dev[INTERFACE(interface->minor)].buf[EP_OUT] = kmalloc(endpoint->wMaxPacketSize, GFP_KERNEL);
					if (!uctl_dev[INTERFACE(interface->minor)].buf[EP_OUT])
					{
						err("Not able to get memory for out ep buffer of this device.");
						uctl_uninit(interface);
						return -ENOMEM;
					}
					buf_left_over[INTERFACE(interface->minor)][EP_OUT] = 0;
				}
			}
		}
	}

	uctl_dev[INTERFACE(interface->minor)].device = interface_to_usbdev(interface);

	uctl_dev[INTERFACE(interface->minor)].class.name = "usb/uctl%d";
	uctl_dev[INTERFACE(interface->minor)].class.fops = &fops;
	retval = usb_register_dev(interface, &uctl_dev[INTERFACE(interface->minor)].class);
	if (retval)
	{
		/* Something prevented us from registering this driver */
		err("Not able to get a minor for this device.");
		uctl_uninit(interface);
		return retval;
	}
	is_active[INTERFACE(interface->minor)] = 1;
#endif

	return 0;
}

void uctl_disconnect(struct usb_interface *interface)
{
	printk("uCtl USB #%d now disconnected\n", interface->minor);
	printk("Alt Setting: (%d, %p)\n", interface->num_altsetting, interface->altsetting);
	printk("Current Alt Setting: (%p)\n", interface->cur_altsetting);
	uctl_uninit(interface);
}

/* Table of devices that work with this driver */
static struct usb_device_id uctl_table[] =
{
	{
		USB_DEVICE(UCTL_VENDOR_ID, UCTL_PRODUCT_ID)
	},
	{} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, uctl_table);

static struct usb_driver uctl_driver =
{
	.name = "uctl",
	.probe = uctl_probe,
	.disconnect = uctl_disconnect,
	.id_table = uctl_table,
};

static int __init uctl_init(void)
{
	int result;

	/* Register this driver with the USB subsystem */
	if ((result = usb_register(&uctl_driver)))
	{
		err("usb_register failed. Error number %d", result);
	}
	info("uCtl usb_registered");
	return result;
}

static void __exit uctl_exit(void)
{
	/* Deregister this driver with the USB subsystem */
	usb_deregister(&uctl_driver);
	info("uCtl usb_deregistered");
}

module_init(uctl_init);
module_exit(uctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia");
MODULE_DESCRIPTION("USB Device Driver for the uCtl based Board");
