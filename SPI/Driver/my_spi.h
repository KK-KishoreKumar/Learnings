#ifndef MY_SPI_H
#define MY_SPI_H
#include <linux/spi/spi.h>
#if 0
struct uart_omap_port {
	struct uart_port	port;
	struct device		*dev;

	unsigned char		ier;
	unsigned char		lcr;
	unsigned char		mcr;
	unsigned char		fcr;
	unsigned char		efr;
	unsigned char		dll;
	unsigned char		dlh;
	unsigned char		mdr1;
	unsigned char		scr;
	unsigned char		wer;

	/*
	 * Some bits in registers are cleared on a read, so they must
	 * be saved whenever the register is read but the bits will not
	 * be immediately processed.
	 */
	char			name[20];
	u8			wakeups_enabled;
	u32			features;

	u32			latency;
	u32			calc_latency;
	char *rx_buff;
	size_t rx_size;
	char *tx_buff;
	size_t tx_size;
};

int serial_read(struct uart_omap_port *up, size_t len);
#endif

struct omap2_mcspi_regs {
	u32 modulctrl;
	u32 wakeupenable;
	//struct list_head cs;
};

struct omap2_mcspi {
	//struct spi_master	*master;
	/* Virtual base address of the controller */
	void __iomem		*base;
	unsigned long		phys;
	/* SPI1 has 4 channels, while SPI2 has 2 */
	struct device		*dev;
	struct omap2_mcspi_regs ctx;
	int			word_len;
	u32			chconf0;
	int			fifo_depth;
	unsigned int		pin_dir:1;
};

int fcd_init(struct omap2_mcspi *mcspi);
void fcd_exit(void);
int omap2_mcspi_setup_transfer(struct omap2_mcspi *mcspi, struct spi_transfer *t);
void omap2_mcspi_cleanup(struct omap2_mcspi *mcspi);
int omap2_mcspi_setup(struct omap2_mcspi *mcspi);
int omap2_mcspi_transfer_one_message(struct omap2_mcspi *mcspi,
		struct spi_transfer *t);
#if 0
int serial_open(struct uart_port *port);
void serial_omap_shutdown(struct uart_port *port);
void serial_omap_start_tx(struct uart_port *port);
int serial_omap_startup(struct uart_port *port);
#endif

#endif
