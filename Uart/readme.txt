https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/
http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf
https://github.com/martinezjavier/ldd3/
http://elinux.org/Device_drivers (As a part of searching for the tty drivers found this page which redirects to the nice links for vivi driver, frame buffer driver, tty driver and so on).
http://free-electrons.com/doc/kernel-architecture.pdf (Explains with imx driver as an example)
http://free-electrons.com/doc/serial-drivers.pdf (Explains the complete framework)
http://free-electrons.com/doc/serial-drivers-lab.pdf (Lab excercise)
http://venkateshabbarapu.blogspot.in/2012/09/serial-subsystemuart.html
http://www.linuxjournal.com/article/6331 (Little Old)
https://yannik520.github.io/tty/tty_driver.html#sec-3 (Nice Diagramtic representation of tty framework)

To begin, its better to start with the tty driver framework stated in free electrons slides. The links are as below:
http://free-electrons.com/doc/serial-drivers.pdf
http://free-electrons.com/doc/serial-drivers-lab.pdf
http://free-electrons.com/doc/kernel-architecture.pdf (Explains with imx driver as an example)

All these slides are downloaded at UART folder. Below is my understanding of the serial driver:
In order for the serial devices to be visible to the user space, they should be registered as the tty devices. The tty driver which helps in registering the uart driver with tty core is serial-core which can be found under driver/tty/serial_core.c.
The UART driver consists of three components: 
uart_driver - data structure representing the driver
uart_port - data structure representing the port
uart_ops - data structure containing the pointer to the uart_ops

The first thing which we do is to register the uart_driver with the serial_core which in turn registers it with the tty layer, which does the following:
  + allocate the tty driver and assign it to the tty_driver field of this driver
  + Assign the owner, driver_name, driver_minor, driver_major from the uart_driver.
  + Initialize the init_termios with tty_std_termios and initialize the c_cflags_ with 9600, 8 data bits
  + ispeed and ospeed to 9600 and set the flags to RAW and set the drv->state to drv.
  + Assign the default uart_ops to the new tty.
  + Then, we go about initialize state for all the 7 ports, initialize the tty port for all. Then we register the tty_driver.
Next thing is to register the platform driver for the atmel_serial, whose probe those the following:
  + We get the atmel_uart_data from the platform data. We get the port number from the pdev->id and register this port with atmel_initport which in turn does the following:
   + Get the atmel_uart_data from pdev and assign the things such as use_rx_dma, use_dma_tx and so on to atmel port which is wrapper around uart_port.
   + Next thing is to initialize the various things such as port->iotype, flags, ops (with atmel_pops), fifo_size (1), map_base and irq.Then, we initialize the tasklet. Then, we initialize the rx_ring. 
   + Next thing is to initialize the clock and the set the mask for interrupts based on the settings from pdata.
   + The atmel_pops consists of several things as below:
    struct uart_ops atmel_pops = {
        .tx_empty       = atmel_tx_empty,
        .set_mctrl      = atmel_set_mctrl,
        .get_mctrl      = atmel_get_mctrl,
        .stop_tx        = atmel_stop_tx,
        .start_tx       = atmel_start_tx,
        .stop_rx        = atmel_stop_rx,
        .enable_ms      = atmel_enable_ms,
        .break_ctl      = atmel_break_ctl,
        .startup        = atmel_startup,
        .shutdown       = atmel_shutdown,
        .flush_buffer   = atmel_flush_buffer,
        .set_termios    = atmel_set_termios,
        .set_ldisc      = atmel_set_ldisc,
        .type           = atmel_type,
        .release_port   = atmel_release_port,
        .request_port   = atmel_request_port,
        .config_port    = atmel_config_port,
        .verify_port    = atmel_verify_port,
        .pm             = atmel_serial_pm,
        .ioctl          = atmel_ioctl,
#ifdef CONFIG_CONSOLE_POLL
        .poll_get_char  = atmel_poll_get_char,
        .poll_put_char  = atmel_poll_put_char,
#endif
};
tx_empty - whether FIFO is empty or not
set_mctrl and get_mctrl - allows to set and get the modem parameters
stop_tx and start_tx
stop_rx
startup and shutdown called when the port is opened/closed
request_port and release_port - request/release IO/Memory regions
set_termios - set port parameters.

Let's discuss how the transmission is implemented:
 + The characters to transmit are stored in a circular buffer:
     
  
