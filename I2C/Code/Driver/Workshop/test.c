#include "i2c_char.h"

int i2c_write(struct omap_i2c_dev *dev, char *buff, size_t len)
{
	//Set the TX FIFO Threshold and clear the FIFO's
	//Set the slave address
	//update the count register
	//update the CON Register to start the transaction with master mode, transmitter
	//Wait for anything interesting to happen on the bus
	//Check for the status - XRDY, then write the data in data register
	//Check if ARDY is come
	u16 w;
	int k = 7;
	int i2c_error = 0, status;
	int idx = 0;
	u16 buf = omap_i2c_read_reg(dev, OMAP_I2C_BUF_REG);
	u8 tx_buf[6] = {0x00, 0x50, 0x44};
	buf &= ~(0x3f);
	buf |= OMAP_I2C_BUF_TXFIF_CLR;
	omap_i2c_write_reg(dev, OMAP_I2C_BUF_REG, buf);

	omap_i2c_write_reg(dev, OMAP_I2C_SA_REG, 0x50);
	omap_i2c_write_reg(dev, OMAP_I2C_CNT_REG, 3);
	

	w = (OMAP_I2C_CON_EN | OMAP_I2C_CON_STT | OMAP_I2C_CON_STP | OMAP_I2C_CON_MST |
			OMAP_I2C_CON_TRX);
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, w);
	while (k--) {
		status = wait_for_event(dev);
		printk("Status = %X\n", status);
		if (!status)
		{
			printk("Timed out waiting for an event\n");
			i2c_error = -ETIMEDOUT;
			goto wr_exit;
		}
		if (status & OMAP_I2C_STAT_XRDY)
		{
			printk("Got XRDY\n");
			omap_i2c_write_reg(dev, OMAP_I2C_DATA_REG, tx_buf[idx++]);
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XRDY);
			continue;
		}
		if (status & OMAP_I2C_STAT_ARDY)
		{
			printk("Got ARDY\n");
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
			break;
		}
	}
	if (k <= 0) {
		printk("Timed out\n");
		i2c_error = -ETIMEDOUT;
	}
wr_exit:	
	flush_fifo(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
	return i2c_error;

}

int i2c_read(struct omap_i2c_dev *dev, char *buff, size_t len)
{	
	//Set the RX FIFO Threshold and clear the FIFO's
	//Set the slave address
	//update the count register
	//update the CON Register to start the transaction with master mode, Reciever
	//Wait for anything interesting to happen on the bus
	//Check for the status - RRDY, then write the data in data register
	//Check if ARDY is come
	u16 w;
	int k = 7;
	int i2c_error = 0, status;
	int idx = 0;
	u16 buf = omap_i2c_read_reg(dev, OMAP_I2C_BUF_REG);
	//u8 tx_buf[6] = {0x00, 0x50, 0x44};
	buf &= ~(0x3f << 8);
	buf |= OMAP_I2C_BUF_RXFIF_CLR;
	omap_i2c_write_reg(dev, OMAP_I2C_BUF_REG, buf);

	omap_i2c_write_reg(dev, OMAP_I2C_SA_REG, 0x50);
	omap_i2c_write_reg(dev, OMAP_I2C_CNT_REG, 3);

	w = (OMAP_I2C_CON_EN | OMAP_I2C_CON_STT | OMAP_I2C_CON_STP | OMAP_I2C_CON_MST);
	omap_i2c_write_reg(dev, OMAP_I2C_CON_REG, w);
	while (k--) {
		status = wait_for_event(dev);
		printk("Status = %X\n", status);
		if (!status)
		{
			printk("Timed out waiting for an event in read\n");
			i2c_error = -ETIMEDOUT;
			goto rd_exit;
		}
		if (status & OMAP_I2C_STAT_RRDY)
		{
			printk("Got RRDY\n");
			buff[idx] = omap_i2c_read_reg(dev, OMAP_I2C_DATA_REG);
			printk("temp %d = %x\n", idx, buff[idx]);
			idx++;
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RRDY);
			continue;
		}
		if (status & OMAP_I2C_STAT_ARDY)
		{
			printk("Got ARDY\n");
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
			break;
		}
	}
	if (k <= 0) {
		printk("Timed out\n");
		i2c_error = -ETIMEDOUT;
	}
rd_exit:	
	flush_fifo(dev);
	omap_i2c_write_reg(dev, OMAP_I2C_STAT_REG, 0XFFFF);
	return i2c_error;
}

