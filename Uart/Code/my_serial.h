#ifndef MY_SERIAL_H
#define MY_SERIAL_H

struct uart_omap_dma {
	u8			uart_dma_tx;
	u8			uart_dma_rx;
	int			rx_dma_channel;
	int			tx_dma_channel;
	dma_addr_t		rx_buf_dma_phys;
	dma_addr_t		tx_buf_dma_phys;
	unsigned int		uart_base;
	/*
	 * Buffer for rx dma.It is not required for tx because the buffer
	 * comes from port structure.
	 */
	unsigned char		*rx_buf;
	unsigned int		prev_rx_dma_pos;
	int			tx_buf_size;
	int			tx_dma_used;
	int			rx_dma_used;
	spinlock_t		tx_lock;
	spinlock_t		rx_lock;
	/* timer to poll activity on rx dma */
	struct timer_list	rx_timer;
	unsigned int		rx_buf_size;
	unsigned int		rx_poll_rate;
	unsigned int		rx_timeout;
};

struct uart_omap_port {
	struct uart_port	port;
	struct uart_omap_dma	uart_dma;
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

	int			use_dma;
	/*
	 * Some bits in registers are cleared on a read, so they must
	 * be saved whenever the register is read but the bits will not
	 * be immediately processed.
	 */
	unsigned int		lsr_break_flag;
	unsigned char		msr_saved_flags;
	char			name[20];
	unsigned long		port_activity;
	int			context_loss_cnt;
	u32			errata;
	u8			wakeups_enabled;
	u32			features;

	int			DTR_gpio;
	int			DTR_inverted;
	int			DTR_active;

	struct serial_rs485	rs485;
	int			rts_gpio;

	struct pm_qos_request	pm_qos_request;
	u32			latency;
	u32			calc_latency;
	struct work_struct	qos_work;
	bool			is_suspending;
	char *rx_buff;
	size_t rx_size;
};

//void transmit_chars(struct uart_omap_port *up, char ch);
int serial_read(struct uart_omap_port *up, unsigned char *t, size_t len);
int fcd_init(struct uart_omap_port *up);
void fcd_exit(void);
int serial_open(struct uart_port *port);
void serial_omap_shutdown(struct uart_port *port);
void serial_omap_start_tx(struct uart_port *port);
int serial_omap_startup(struct uart_port *port);

#endif
