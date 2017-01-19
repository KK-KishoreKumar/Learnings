/*
 * TI OMAP I2C master mode driver
 *
 * Copyright (C) 2003 MontaVista Software, Inc.
 * Copyright (C) 2005 Nokia Corporation
 * Copyright (C) 2004 - 2007 Texas Instruments.
 *
 * Originally written by MontaVista Software, Inc.
 * Additional contributions by:
 *	Tony Lindgren <tony@atomide.com>
 *	Imre Deak <imre.deak@nokia.com>
 *	Juha Yrjölä <juha.yrjola@solidboot.com>
 *	Syed Khasim <x0khasim@ti.com>
 *	Nishant Menon <nm@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/i2c-omap.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include "i2c_char.h"


/* timeout waiting for the controller to respond */
#define OMAP_I2C_TIMEOUT (msecs_to_jiffies(1000))


/* For OMAP3 I2C_IV has changed to I2C_WE (wakeup enable) */
enum {
	OMAP_I2C_REV_REG = 0,
	OMAP_I2C_IE_REG,
	OMAP_I2C_STAT_REG,
	OMAP_I2C_IV_REG,
	OMAP_I2C_WE_REG,
	OMAP_I2C_SYSS_REG,
	OMAP_I2C_BUF_REG,
	OMAP_I2C_CNT_REG,
	OMAP_I2C_DATA_REG,
	OMAP_I2C_SYSC_REG,
	OMAP_I2C_CON_REG,
	OMAP_I2C_OA_REG,
	OMAP_I2C_SA_REG,
	OMAP_I2C_PSC_REG,
	OMAP_I2C_SCLL_REG,
	OMAP_I2C_SCLH_REG,
	OMAP_I2C_SYSTEST_REG,
	OMAP_I2C_BUFSTAT_REG,
	/* only on OMAP4430 */
	OMAP_I2C_IP_V2_REVNB_LO,
	OMAP_I2C_IP_V2_REVNB_HI,
	OMAP_I2C_IP_V2_IRQSTATUS_RAW,
	OMAP_I2C_IP_V2_IRQENABLE_SET,
	OMAP_I2C_IP_V2_IRQENABLE_CLR,
};

/* I2C Interrupt Enable Register (OMAP_I2C_IE): */
#define OMAP_I2C_IE_XDR		(1 << 14)	/* TX Buffer drain int enable */
#define OMAP_I2C_IE_RDR		(1 << 13)	/* RX Buffer drain int enable */
#define OMAP_I2C_IE_XRDY	(1 << 4)	/* TX data ready int enable */
#define OMAP_I2C_IE_RRDY	(1 << 3)	/* RX data ready int enable */
#define OMAP_I2C_IE_ARDY	(1 << 2)	/* Access ready int enable */
#define OMAP_I2C_IE_NACK	(1 << 1)	/* No ack interrupt enable */
#define OMAP_I2C_IE_AL		(1 << 0)	/* Arbitration lost int ena */

/* I2C Status Register (OMAP_I2C_STAT): */
#define OMAP_I2C_STAT_XDR	(1 << 14)	/* TX Buffer draining */
#define OMAP_I2C_STAT_RDR	(1 << 13)	/* RX Buffer draining */
#define OMAP_I2C_STAT_BB	(1 << 12)	/* Bus busy */
#define OMAP_I2C_STAT_ROVR	(1 << 11)	/* Receive overrun */
#define OMAP_I2C_STAT_XUDF	(1 << 10)	/* Transmit underflow */
#define OMAP_I2C_STAT_AAS	(1 << 9)	/* Address as slave */
#define OMAP_I2C_STAT_AD0	(1 << 8)	/* Address zero */
#define OMAP_I2C_STAT_XRDY	(1 << 4)	/* Transmit data ready */
#define OMAP_I2C_STAT_RRDY	(1 << 3)	/* Receive data ready */
#define OMAP_I2C_STAT_ARDY	(1 << 2)	/* Register access ready */
#define OMAP_I2C_STAT_NACK	(1 << 1)	/* No ack interrupt enable */
#define OMAP_I2C_STAT_AL	(1 << 0)	/* Arbitration lost int ena */

/* I2C WE wakeup enable register */
#define OMAP_I2C_WE_XDR_WE	(1 << 14)	/* TX drain wakup */
#define OMAP_I2C_WE_RDR_WE	(1 << 13)	/* RX drain wakeup */
#define OMAP_I2C_WE_AAS_WE	(1 << 9)	/* Address as slave wakeup*/
#define OMAP_I2C_WE_BF_WE	(1 << 8)	/* Bus free wakeup */
#define OMAP_I2C_WE_STC_WE	(1 << 6)	/* Start condition wakeup */
#define OMAP_I2C_WE_GC_WE	(1 << 5)	/* General call wakeup */
#define OMAP_I2C_WE_DRDY_WE	(1 << 3)	/* TX/RX data ready wakeup */
#define OMAP_I2C_WE_ARDY_WE	(1 << 2)	/* Reg access ready wakeup */
#define OMAP_I2C_WE_NACK_WE	(1 << 1)	/* No acknowledgment wakeup */
#define OMAP_I2C_WE_AL_WE	(1 << 0)	/* Arbitration lost wakeup */

#define OMAP_I2C_WE_ALL		(OMAP_I2C_WE_XDR_WE | OMAP_I2C_WE_RDR_WE | \
				OMAP_I2C_WE_AAS_WE | OMAP_I2C_WE_BF_WE | \
				OMAP_I2C_WE_STC_WE | OMAP_I2C_WE_GC_WE | \
				OMAP_I2C_WE_DRDY_WE | OMAP_I2C_WE_ARDY_WE | \
				OMAP_I2C_WE_NACK_WE | OMAP_I2C_WE_AL_WE)

/* I2C Buffer Configuration Register (OMAP_I2C_BUF): */
#define OMAP_I2C_BUF_RDMA_EN	(1 << 15)	/* RX DMA channel enable */
#define OMAP_I2C_BUF_RXFIF_CLR	(1 << 14)	/* RX FIFO Clear */
#define OMAP_I2C_BUF_XDMA_EN	(1 << 7)	/* TX DMA channel enable */
#define OMAP_I2C_BUF_TXFIF_CLR	(1 << 6)	/* TX FIFO Clear */

/* I2C Configuration Register (OMAP_I2C_CON): */
#define OMAP_I2C_CON_EN		(1 << 15)	/* I2C module enable */
#define OMAP_I2C_CON_BE		(1 << 14)	/* Big endian mode */
#define OMAP_I2C_CON_OPMODE_HS	(1 << 12)	/* High Speed support */
#define OMAP_I2C_CON_STB	(1 << 11)	/* Start byte mode (master) */
#define OMAP_I2C_CON_MST	(1 << 10)	/* Master/slave mode */
#define OMAP_I2C_CON_TRX	(1 << 9)	/* TX/RX mode (master only) */
#define OMAP_I2C_CON_XA		(1 << 8)	/* Expand address */
#define OMAP_I2C_CON_RM		(1 << 2)	/* Repeat mode (master only) */
#define OMAP_I2C_CON_STP	(1 << 1)	/* Stop cond (master only) */
#define OMAP_I2C_CON_STT	(1 << 0)	/* Start condition (master) */

/* I2C SCL time value when Master */
#define OMAP_I2C_SCLL_HSSCLL	8
#define OMAP_I2C_SCLH_HSSCLH	8

/* I2C System Test Register (OMAP_I2C_SYSTEST): */
#ifdef DEBUG
#define OMAP_I2C_SYSTEST_ST_EN		(1 << 15)	/* System test enable */
#define OMAP_I2C_SYSTEST_FREE		(1 << 14)	/* Free running mode */
#define OMAP_I2C_SYSTEST_TMODE_MASK	(3 << 12)	/* Test mode select */
#define OMAP_I2C_SYSTEST_TMODE_SHIFT	(12)		/* Test mode select */
#define OMAP_I2C_SYSTEST_SCL_I		(1 << 3)	/* SCL line sense in */
#define OMAP_I2C_SYSTEST_SCL_O		(1 << 2)	/* SCL line drive out */
#define OMAP_I2C_SYSTEST_SDA_I		(1 << 1)	/* SDA line sense in */
#define OMAP_I2C_SYSTEST_SDA_O		(1 << 0)	/* SDA line drive out */
#endif

/* OCP_SYSSTATUS bit definitions */
#define SYSS_RESETDONE_MASK		(1 << 0)

/* OCP_SYSCONFIG bit definitions */
#define SYSC_CLOCKACTIVITY_MASK		(0x3 << 8)
#define SYSC_SIDLEMODE_MASK		(0x3 << 3)
#define SYSC_ENAWAKEUP_MASK		(1 << 2)
#define SYSC_SOFTRESET_MASK		(1 << 1)
#define SYSC_AUTOIDLE_MASK		(1 << 0)

#define SYSC_IDLEMODE_SMART		0x2
#define SYSC_CLOCKACTIVITY_FCLK		0x2


#define OMAP_I2C_IP_V2_INTERRUPTS_MASK	0x6FFF

static struct class *i2c_class;

static const u8 reg_map_ip_v2[] = {
	[OMAP_I2C_REV_REG] = 0x04,
	[OMAP_I2C_IE_REG] = 0x2c,
	[OMAP_I2C_STAT_REG] = 0x28,
	[OMAP_I2C_IV_REG] = 0x34,
	[OMAP_I2C_WE_REG] = 0x34,
	[OMAP_I2C_SYSS_REG] = 0x90,
	[OMAP_I2C_BUF_REG] = 0x94,
	[OMAP_I2C_CNT_REG] = 0x98,
	[OMAP_I2C_DATA_REG] = 0x9c,
	[OMAP_I2C_SYSC_REG] = 0x10,
	[OMAP_I2C_CON_REG] = 0xa4,
	[OMAP_I2C_OA_REG] = 0xa8,
	[OMAP_I2C_SA_REG] = 0xac,
	[OMAP_I2C_PSC_REG] = 0xb0,
	[OMAP_I2C_SCLL_REG] = 0xb4,
	[OMAP_I2C_SCLH_REG] = 0xb8,
	[OMAP_I2C_SYSTEST_REG] = 0xbC,
	[OMAP_I2C_BUFSTAT_REG] = 0xc0,
	[OMAP_I2C_IP_V2_REVNB_LO] = 0x00,
	[OMAP_I2C_IP_V2_REVNB_HI] = 0x04,
	[OMAP_I2C_IP_V2_IRQSTATUS_RAW] = 0x24,
	[OMAP_I2C_IP_V2_IRQENABLE_SET] = 0x2c,
	[OMAP_I2C_IP_V2_IRQENABLE_CLR] = 0x30,
};

static inline void omap_i2c_write_reg(struct omap_i2c_dev *i2c_dev,
				      int reg, u16 val)
{
	__raw_writew(val, i2c_dev->base + i2c_dev->regs[reg]);
}

static inline u16 omap_i2c_read_reg(struct omap_i2c_dev *i2c_dev, int reg)
{
	return __raw_readw(i2c_dev->base + i2c_dev->regs[reg]);
}

static inline void
omap_i2c_ack_stat(struct omap_i2c_dev *dev, u16 stat)
{
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, stat);
}
/*
 * Waiting on Bus Busy
 */
int omap_i2c_wait_for_bb(struct omap_i2c_dev *dev)
{
	unsigned long timeout;

	timeout = jiffies + OMAP_I2C_TIMEOUT;
	while (omap_i2c_read_reg(dev, OMAP_I2C_STAT_REG) & OMAP_I2C_STAT_BB) {
		if (time_after(jiffies, timeout)) {
			printk("timeout waiting for bus ready\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}

	return 0;
}

static void flush_fifo(struct omap_i2c_dev *dev)
{
	unsigned long timeout;
	u32 status;

	timeout = jiffies + OMAP_I2C_TIMEOUT;
	while ((status = omap_i2c_read_reg(dev, OMAP_I2C_STAT_REG)) & OMAP_I2C_STAT_RRDY) {
		omap_i2c_read_reg(dev, OMAP_I2C_DATA_REG);
		omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RRDY);
		if (time_after(jiffies, timeout)) {
			dev_warn(dev->dev, "timeout waiting for bus ready\n");
			break;
		}
		msleep(1);
	}
}

static void __omap_i2c_init(struct omap_i2c_dev *dev)
{

	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, 0);

	/* Setup clock prescaler to obtain approx 12MHz I2C module clock: */
	omap_i2c_write_reg(dev, OMAP_I2C_PSC_REG, dev->pscstate);

	/* SCL low and high time values */
	omap_i2c_write_reg(dev, OMAP_I2C_SCLL_REG, dev->scllstate);
	omap_i2c_write_reg(dev, OMAP_I2C_SCLH_REG, dev->sclhstate);
	omap_i2c_write_reg(dev, OMAP_I2C_WE_REG, dev->westate);

	/* Take the I2C module out of reset: */
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, OMAP_I2C_CON_EN);
	/*
	 * Don't write to this register if the IE state is 0 as it can
	 * cause deadlock.
	 */
	if (dev->iestate)
		omap_i2c_write_reg(dev, OMAP_I2C_IE_REG, dev->iestate);
	flush_fifo(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
	omap_i2c_wait_for_bb(dev);
	
}

static u16 wait_for_event(struct omap_i2c_dev *dev)
{
	unsigned long timeout = jiffies + OMAP_I2C_TIMEOUT;
	u16 status;

	while (!((status = omap_i2c_read_reg(dev, OMAP_I2C_STAT_REG)) & 
				(OMAP_I2C_STAT_ROVR | OMAP_I2C_STAT_XUDF | 
				 OMAP_I2C_STAT_XRDY | OMAP_I2C_STAT_RRDY | 
				 OMAP_I2C_STAT_ARDY | OMAP_I2C_STAT_NACK | 
				 OMAP_I2C_STAT_AL))) {
		if (time_after(jiffies, timeout)) {
			printk("time-out waiting for event\n");
			omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
			return 0;
		}
		mdelay(1);
	}
	return status;
}

static void omap_i2c_set_speed(struct omap_i2c_dev *dev)
{
	u16 psc = 0, scll = 0, sclh = 0;
	u16 fsscll = 0, fssclh = 0, hsscll = 0, hssclh = 0;
	unsigned long fclk_rate = 12000000;
	unsigned long internal_clk = 0;
	struct clk *fclk;
	
	/*
	 * HSI2C controller internal clk rate should be 19.2 Mhz for
	 * HS and for all modes on 2430. On 34xx we can use lower rate
	 * to get longer filter period for better noise suppression.
	 * The filter is iclk (fclk for HS) period.
	 */
	if (dev->speed > 400 ||
			dev->flags & OMAP_I2C_FLAG_FORCE_19200_INT_CLK)
		internal_clk = 19200;
	else if (dev->speed > 100)
		internal_clk = 9600;
	else
		internal_clk = 4000;
	fclk = clk_get(dev->dev, "fck");
	fclk_rate = clk_get_rate(fclk) / 1000;
	clk_put(fclk);

	/* Compute prescaler divisor */
	psc = fclk_rate / internal_clk;
	psc = psc - 1;

	/* If configured for High Speed */
	if (dev->speed > 400) {
		unsigned long scl;

		/* For first phase of HS mode */
		scl = internal_clk / 400;
		fsscll = scl - (scl / 3) - 7;
		fssclh = (scl / 3) - 5;

		/* For second phase of HS mode */
		scl = fclk_rate / dev->speed;
		hsscll = scl - (scl / 3) - 7;
		hssclh = (scl / 3) - 5;
	} else if (dev->speed > 100) {
		unsigned long scl;

		/* Fast mode */
		scl = internal_clk / dev->speed;
		fsscll = scl - (scl / 3) - 7;
		fssclh = (scl / 3) - 5;
	} else {
		/* Standard mode */
		fsscll = internal_clk / (dev->speed * 2) - 7;
		fssclh = internal_clk / (dev->speed * 2) - 5;
	}
	scll = (hsscll << OMAP_I2C_SCLL_HSSCLL) | fsscll;
	sclh = (hssclh << OMAP_I2C_SCLH_HSSCLH) | fssclh;

	dev->pscstate = psc;
	dev->scllstate = scll;
	dev->sclhstate = sclh;
}

static int omap_i2c_init(struct omap_i2c_dev *dev)
{
	omap_i2c_set_speed(dev);
	__omap_i2c_init(dev);

	return 0;
}

static void omap_i2c_resize_fifo(struct omap_i2c_dev *dev, u8 size, bool is_rx)
{
	u16 buf;

	if (dev->flags & OMAP_I2C_FLAG_NO_FIFO)
		return;

	/*
	 * Set up notification threshold based on message size. We're doing
	 * this to try and avoid draining feature as much as possible. Whenever
	 * we have big messages to transfer (bigger than our total fifo size)
	 * then we might use draining feature to transfer the remaining bytes.
	 */

	dev->threshold = clamp(size, (u8) 1, dev->fifo_size);
	printk("thr = %d\n", dev->threshold);

	buf = omap_i2c_read_reg(dev, OMAP_I2C_BUF_REG);

	if (is_rx) {
		/* Clear RX Threshold */
		buf &= ~(0x3f << 8);
		buf |= ((dev->threshold - 1) << 8) | OMAP_I2C_BUF_RXFIF_CLR;
	} else {
		/* Clear TX Threshold */
		buf &= ~0x3f;
		buf |= (dev->threshold - 1) | OMAP_I2C_BUF_TXFIF_CLR;
	}

	omap_i2c_write_reg(dev, OMAP_I2C_BUF_REG, buf);
}

static int omap_i2c_transmit_data(struct omap_i2c_dev *dev, u8 num_bytes,
		bool is_xdr)
{
	u16		w;

	while (num_bytes--) {
		w = *dev->buf++;
		dev->buf_len--;

		/*
		 * Data reg in 2430, omap3 and
		 * omap4 is 8 bit wide
		 */
		if (dev->flags & OMAP_I2C_FLAG_16BIT_DATA_REG) {
			w |= *dev->buf++ << 8;
			dev->buf_len--;
		}

		omap_i2c_write_reg(dev, OMAP_I2C_DATA_REG, w);
	}

	return 0;
}

static void omap_i2c_receive_data(struct omap_i2c_dev *dev, u8 num_bytes,
		bool is_rdr)
{
	u16		w;

	while (num_bytes--) {
		w = omap_i2c_read_reg(dev, OMAP_I2C_DATA_REG);
		*dev->buf++ = w;
		dev->buf_len--;

		/*
		 * Data reg in 2430, omap3 and
		 * omap4 is 8 bit wide
		 */
		if (dev->flags & OMAP_I2C_FLAG_16BIT_DATA_REG) {
			*dev->buf++ = w >> 8;
			dev->buf_len--;
		}
	}
}

/*
 * Low level master read/write transaction.
 */
int omap_i2c_write_msg(struct omap_i2c_dev *dev,
			     struct i2c_msg *msg, int stop)
{
	u16 w;
	u16 status;
	int k = 10, i2c_error = 0;

	dev_dbg(dev->dev, "addr: 0x%04x, len: %d, flags: 0x%x, stop: %d\n",
		msg->addr, msg->len, msg->flags, stop);

	if (msg->len == 0)
		return -EINVAL;

	omap_i2c_resize_fifo(dev, msg->len, 0);

	omap_i2c_write_reg(dev, OMAP_I2C_SA_REG, msg->addr);

	/* REVISIT: Could the STB bit of I2C_CON be used with probing? */
	dev->buf = msg->buf;
	dev->buf_len = msg->len;

	/* make sure writes to dev->buf_len are ordered */
	barrier();

	omap_i2c_write_reg(dev, OMAP_I2C_CNT_REG, dev->buf_len);
	/* Clear the FIFO Buffers */
	w = omap_i2c_read_reg(dev, OMAP_I2C_BUF_REG);
	w |= OMAP_I2C_BUF_RXFIF_CLR | OMAP_I2C_BUF_TXFIF_CLR;
	omap_i2c_write_reg(dev, OMAP_I2C_BUF_REG, w);

	w = OMAP_I2C_CON_EN | OMAP_I2C_CON_MST | OMAP_I2C_CON_STT | OMAP_I2C_CON_TRX | OMAP_I2C_CON_STP;
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, w);
	while (k--) {
		status = wait_for_event(dev);
		printk("status = %x\n", status);
		if (status & OMAP_I2C_STAT_XDR) {
			u8 num_bytes = 1;
			int ret;

			if (dev->fifo_size)
				num_bytes = dev->buf_len;

			printk("Transmitting XDR\n");
			ret = omap_i2c_transmit_data(dev, num_bytes, true);
			if (ret < 0)
				break;

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XDR);
			break;
		}
		if (status & OMAP_I2C_STAT_XRDY) {
			u8 num_bytes = 1;
			int ret;

			printk("Transmitting XRDY\n");
			if (dev->threshold)
				num_bytes = dev->threshold;

			ret = omap_i2c_transmit_data(dev, num_bytes, false);
			if (ret < 0) {
				i2c_error = ret;
				break;
			}

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XRDY);
			continue;
		}
		if (status & OMAP_I2C_STAT_ARDY) {
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
			break;
		}
	}
	if (k <= 0)
	{
		printk("Timed out\n");
		i2c_error = -ETIMEDOUT;
		goto wr_exit;
	}
wr_exit:
	flush_fifo(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
	return i2c_error;
}


/*
 * Low level master read/write transaction.
 */
int omap_i2c_read_msg(struct omap_i2c_dev *dev, struct i2c_msg *msg, int stop)
{
	u16 w;
	u16 status;
	u16 addr = 0X5000;
	int k = 10, i2c_error = 0;

	dev_dbg(dev->dev, "addr: 0x%04x, len: %d, flags: 0x%x, stop: %d\n",
		msg->addr, msg->len, msg->flags, stop);

	if (msg->len == 0)
		return -EINVAL;

	omap_i2c_resize_fifo(dev, 2, 0);

	omap_i2c_write_reg(dev, OMAP_I2C_SA_REG, msg->addr);

	/* REVISIT: Could the STB bit of I2C_CON be used with probing? */
	dev->buf = (char *)&addr;
	dev->buf_len = 2;

	/* make sure writes to dev->buf_len are ordered */
	barrier();

	omap_i2c_write_reg(dev, OMAP_I2C_CNT_REG, 2);
	/* Clear the FIFO Buffers */
	w = omap_i2c_read_reg(dev, OMAP_I2C_BUF_REG);
	w |= OMAP_I2C_BUF_RXFIF_CLR | OMAP_I2C_BUF_TXFIF_CLR;
	omap_i2c_write_reg(dev, OMAP_I2C_BUF_REG, w);

	w = OMAP_I2C_CON_EN | OMAP_I2C_CON_MST | OMAP_I2C_CON_STT;

	/* High speed configuration */
	if (dev->speed > 400)
		w |= OMAP_I2C_CON_OPMODE_HS;

		w |= OMAP_I2C_CON_TRX | OMAP_I2C_CON_STP;
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, w);
	while (k--) {
		status = wait_for_event(dev);
		printk("status = %x\n", status);
		if (status & OMAP_I2C_STAT_XDR) {
			u8 num_bytes = 1;
			int ret;

			if (dev->fifo_size)
				num_bytes = dev->buf_len;

			printk("Transmitting XDR\n");
			ret = omap_i2c_transmit_data(dev, num_bytes, true);
			if (ret < 0)
				break;

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XDR);
			break;
		}
		if (status & OMAP_I2C_STAT_XRDY) {
			u8 num_bytes = 1;
			int ret;

			printk("Transmitting XRDY\n");
			if (dev->threshold)
				num_bytes = dev->threshold;

			ret = omap_i2c_transmit_data(dev, num_bytes, false);
			if (ret < 0) {
				i2c_error = ret;
				break;
			}

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XRDY);
			continue;
		}
		if (status & OMAP_I2C_STAT_ARDY) {
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
			break;
		}
	}
	if (k <= 0)
	{
		printk("Timed out\n");
		i2c_error = -ETIMEDOUT;
		goto rd_exit;
	}

	/* Perform the read operation */
	k = 10;
	dev->buf = msg->buf;
	dev->buf_len = msg->len;
	omap_i2c_resize_fifo(dev, 32, 1);
	omap_i2c_write_reg(dev, OMAP_I2C_SA_REG, msg->addr);
	omap_i2c_write_reg(dev, OMAP_I2C_CNT_REG, dev->buf_len);
	w = OMAP_I2C_CON_EN | OMAP_I2C_CON_MST | OMAP_I2C_CON_STT | OMAP_I2C_CON_STP;
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, w);

	while (k--) {
		status = wait_for_event(dev);
		printk("Status TX = %x\n", status);
		if (status == OMAP_I2C_STAT_XRDY) {
			i2c_error = 2;
			printk("i2c_read (data phase): pads on bus probably not configured (status=0x%x)\n",
					status);
			goto rd_exit;
		}
		if (status == 0 || (status & OMAP_I2C_STAT_NACK)) {
			i2c_error = 1;
			printk("NACK\n");
			goto rd_exit;
		}
		if (status & OMAP_I2C_STAT_ARDY) {
			printk("ARDY\n");
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
			break;
		}
		if (status & OMAP_I2C_STAT_RDR) {
			u8 num_bytes = 1;

			if (dev->fifo_size)
				num_bytes = dev->buf_len;

			omap_i2c_receive_data(dev, num_bytes, true);

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RDR);
			continue;
		}

		if (status & OMAP_I2C_STAT_RRDY) {
			u8 num_bytes = 1;

			if (dev->threshold)
				num_bytes = dev->threshold;

			omap_i2c_receive_data(dev, num_bytes, false);
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RRDY);
			continue;
		}
	}
	if (k <= 0) {
		printk("Timed out\n");
		i2c_error = -ETIMEDOUT;
		goto rd_exit;
	}
rd_exit:
	flush_fifo(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
	return i2c_error;
}

#ifdef CONFIG_OF
static struct omap_i2c_bus_platform_data omap3_pdata = {
	.rev = OMAP_I2C_IP_VERSION_1,
	.flags = OMAP_I2C_FLAG_BUS_SHIFT_2,
};

static struct omap_i2c_bus_platform_data omap4_pdata = {
	.rev = OMAP_I2C_IP_VERSION_2,
};

static const struct of_device_id omap_i2c_of_match[] = {
	{
		.compatible = "ti,omap4-i2c",
		.data = &omap4_pdata,
	},
	{
		.compatible = "ti,omap3-i2c",
		.data = &omap3_pdata,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, omap_i2c_of_match);
#endif

static int omap_i2c_probe(struct platform_device *pdev)
{
	struct omap_i2c_dev	*dev;
	struct resource		*mem;
	const struct omap_i2c_bus_platform_data *pdata =
		dev_get_platdata(&pdev->dev);
	struct device_node	*node = pdev->dev.of_node;
	const struct of_device_id *match;
	int init_result;
	//static int idx = 0;
	u16 s;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct omap_i2c_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "Menory allocation failed\n");
		return -ENOMEM;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	match = of_match_device(of_match_ptr(omap_i2c_of_match), &pdev->dev);
	if (match) {
		u32 freq = 100000; /* default to 100000 Hz */

		pdata = match->data;
		dev->flags = pdata->flags;

		of_property_read_u32(node, "clock-frequency", &freq);
		/* convert DT freq value in Hz into kHz for speed */
		dev->speed = freq / 1000;
	} else if (pdata != NULL) {
		dev->speed = pdata->clkrate;
		dev->flags = pdata->flags;
	}

	dev->dev = &pdev->dev;

	spin_lock_init(&dev->lock);

	platform_set_drvdata(pdev, dev);

	dev->regs = (u8 *)reg_map_ip_v2;

	/* Set up the fifo size - Get total size */
	s = (omap_i2c_read_reg(dev, OMAP_I2C_BUFSTAT_REG) >> 14) & 0x3;
	dev->fifo_size = 0x8 << s;

	/*
	 * Set up notification threshold as half the total available
	 * size. This is to ensure that we can handle the status on int
	 * call back latencies.
	 */

	dev->fifo_size = (dev->fifo_size / 2);

	/* reset ASAP, clearing any IRQs */
	omap_i2c_init(dev);

	/* Char interface related initialization */
	dev->i2c_class = i2c_class;
	fcd_init(dev);

	return 0;
}

static int omap_i2c_remove(struct platform_device *pdev)
{
	struct omap_i2c_dev *dev = platform_get_drvdata(pdev);
	fcd_exit(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, 0);
	return 0;
}

static struct platform_driver omap_i2c_driver = {
	.probe		= omap_i2c_probe,
	.remove		= omap_i2c_remove,
	.driver		= {
		.name	= "omap_i2c",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(omap_i2c_of_match),
	},
};

/* I2C may be needed to bring up other drivers */
static int __init omap_i2c_init_driver(void)
{
	if ((i2c_class = class_create(THIS_MODULE, "i2cdrv")) == NULL)
	{
		printk( KERN_ALERT "Class creation failed\n" );
		return -1;
	}
	return platform_driver_register(&omap_i2c_driver);
}

module_init(omap_i2c_init_driver);

static void __exit omap_i2c_exit_driver(void)
{
	platform_driver_unregister(&omap_i2c_driver);
	class_destroy(i2c_class);
}
module_exit(omap_i2c_exit_driver);

MODULE_AUTHOR("MontaVista Software, Inc. (and others)");
MODULE_DESCRIPTION("TI OMAP I2C bus adapter");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap_i2c");
