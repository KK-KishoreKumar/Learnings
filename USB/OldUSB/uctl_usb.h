#ifndef UCTL_USB_H
#define UCTL_USB_H

#define ENABLE_PROCESS_EPS

#define UCTL_VENDOR_ID 0x04D8
#define UCTL_PRODUCT_ID 0x000A

#define ENABLE_FILE_OPS

#define MAX_INTERFACES 2

#define INTERFACE(m) ((long)(m))
#define TX_INTF 1

#define EP_OUT 0
#define EP_IN 1

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct uctl_device
{
	int ep[2];
	int attrib[2];
	int buf_size[2];
	unsigned char *buf[2];
	struct usb_device *device;
	struct usb_class_driver class;
};

#endif
