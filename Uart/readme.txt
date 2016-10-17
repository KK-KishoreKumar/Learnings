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
Uart driver -
