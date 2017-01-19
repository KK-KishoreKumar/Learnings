#ifndef I2C_CHAR_H
#define I2C_CHAR_H

#include <linux/i2c.h>

struct omap_i2c_dev {
	spinlock_t		lock;		/* IRQ synchronization */
	struct device		*dev;
	void __iomem		*base;		/* virtual */
	int			irq;
	u32			speed;		/* Speed of bus in kHz */
	u32			flags;
	u8			*buf;
	u8			*regs;
	size_t			buf_len;
	u8			threshold;
	u8			fifo_size;	/* use as flag and value
						 * fifo_size==0 implies no fifo
						 * if set, should be trsh+1
						 */
	u16			iestate;	/* Saved interrupt register */
	u16			pscstate;
	u16			scllstate;
	u16			sclhstate;
	u16			syscstate;
	u16			westate;
	u16			errata;
	/* for providing a character device access */
	struct class *i2c_class;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
};
int omap_i2c_write_msg(struct omap_i2c_dev *dev, struct i2c_msg *msg, int stop);
int omap_i2c_wait_for_bb(struct omap_i2c_dev *dev);
int omap_i2c_read_msg(struct omap_i2c_dev *dev, struct i2c_msg *msg, int stop);
int fcd_init(struct omap_i2c_dev *i2c_dev);
void fcd_exit(struct omap_i2c_dev *i2c_dev);

#endif
