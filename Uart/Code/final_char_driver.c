#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/platform_data/serial-omap.h>
#include "my_serial.h"

#define FIRST_MINOR 0
#define MINOR_CNT 1

static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static struct uart_omap_port *omap_port;

static int my_open(struct inode *i, struct file *f)
{
	return 0;
}
static int my_close(struct inode *i, struct file *f)
{
	return 0;
}

static char c = 'A';

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	if (*off == 0)
	{
		serial_read(omap_port, &c);
		if (copy_to_user(buf, &c, 1))
		{
			return -EFAULT;
		}
		*off += 1;
		return 1;
	}
	else
		return 0;
}
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	if (copy_from_user(&c, buf + len - 1, 1))
	{
		return -EFAULT;
	}
	transmit_chars(omap_port, c);
	return 1;
}

static struct file_operations driver_fops =
{
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.read = my_read,
	.write = my_write
};

static int fcd_init(struct uart_omap_port *up)
{
	int ret;
	struct device *dev_ret;
	omap_port = up;

	if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "uart_driver")) < 0)
	{
		return ret;
	}

	cdev_init(&c_dev, &driver_fops);

	if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
	{
		unregister_chrdev_region(dev, MINOR_CNT);
		return ret;
	}
	
	if (IS_ERR(cl = class_create(THIS_MODULE, "uart")))
	{
		cdev_del(&c_dev);
		unregister_chrdev_region(dev, MINOR_CNT);
		return PTR_ERR(cl);
	}
	if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "uart%d", FIRST_MINOR)))
	{
		class_destroy(cl);
		cdev_del(&c_dev);
		unregister_chrdev_region(dev, MINOR_CNT);
		return PTR_ERR(dev_ret);
	}

	return 0;
}

static void fcd_exit(void)
{
	device_destroy(cl, dev);
	class_destroy(cl);
	cdev_del(&c_dev);
	unregister_chrdev_region(dev, MINOR_CNT);
}

#if 0
module_init(fcd_init);
module_exit(fcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sysplay Workshops");
MODULE_DESCRIPTION("A Character Driver");
#endif
