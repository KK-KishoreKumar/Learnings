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
	return serial_omap_startup(&omap_port->port);
}
static int my_close(struct inode *i, struct file *f)
{
	return 0;
}

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	char *str = omap_port->rx_buff;
	if (*off == 0)
	{
		if (len > 10)
			len = 10;
		len = serial_read(omap_port, len);
		if (len <= 0)
			return -EAGAIN;
		if (copy_to_user(buf, str, len))
		{
			return -EFAULT;
		}
		*off += 1;
		return len;
	}
	else
		return 0;
}
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	char *str = omap_port->tx_buff;
	int count = ((len > omap_port->tx_size)? omap_port->tx_size : len);
	omap_port->port.icount.tx = count;
	if (copy_from_user(str, buf, count))
	{
		return -EFAULT;
	}
	serial_omap_start_tx(&omap_port->port);
	*off = 0;
	return len;
}

static struct file_operations driver_fops =
{
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_close,
	.read = my_read,
	.write = my_write
};

int fcd_init(struct uart_omap_port *up)
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

void fcd_exit(void)
{
	device_destroy(cl, dev);
	class_destroy(cl);
	cdev_del(&c_dev);
	unregister_chrdev_region(dev, MINOR_CNT);
	serial_omap_shutdown(&omap_port->port);
}

