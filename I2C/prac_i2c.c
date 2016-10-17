#include "plat/i2c.h"

The very first step in the i2c registeration is to define an array of i2c_board_info struct. This is struct is defined in 
board dependent file.

static struct i2c_board_info am33x_b[] = {
	{
	I2C_BOARD_INFO("24c256", 0x50),
	},
};

 omap_register_i2c_bus(2, 100, am335x_i2c1_boardinfo,
                        ARRAY_SIZE(am335x_i2c1_boardinfo));
This is defined in 
arch/arm/plat-omap/i2c.c
The first step we do here is to register the board info with 
/**
 * i2c_register_board_info - statically declare I2C devices
 * @busnum: identifies the bus to which these devices belong
 * @info: vector of i2c device descriptors
 * @len: how many descriptors in the vector; may be zero to reserve
 *      the specified bus number.
 *
 * Systems using the Linux I2C driver stack can declare tables of board info
 * while they initialize.  This should be done in board-specific init code
 * near arch_initcall() time, or equivalent, before any I2C adapter driver is
 * registered.  For example, mainboard init code could define several devices,
 * as could the init code for each daughtercard in a board stack.
 *
 * The I2C devices will be created later, after the adapter for the relevant
 * bus has been registered.  After that moment, standard driver model tools
 * are used to bind "new style" I2C drivers to the devices.  The bus number
 * for any device declared using this routine is not available for dynamic
 * allocation.
 */
+ drivers/i2c/i2c-boardinfo.c
i2c_register_board_info(bus_id, info, len)

The next step is to fill the platform data 
struct omap_i2c_bus_platform_data {
        u32             clkrate;
        u32             rev;
        u32             flags;
        void            (*set_mpu_wkup_lat)(struct device *dev, long set);
        int             (*device_reset) (struct device *dev);
};

Then we invoke the function:
omap_i2c_add_bus() defined in arch/arm/plat-omap/i2c.c, this in turns invokes the omap2_i2c_add_bus()
  + This does the following things:
     - First it finds the hwmod with the name i2c1. These modules are registered in arch/arm/mach-omap2/omap_hwmod.c by 
       omap_hwmod_register which in turn is invoked in arch/arm/mach-omap2/omap_hwmod_33xx_data.c by am33xx_hwmod_init which
      is invoked with am33xx_hwmods (array of hwmodules). One of them is am33xx_i2c1_hwmod. 
     - Then, we start filling the omap_i2c_platform_data for the corresponding bus. The fields required are dev_attr->flags
      which are nothing but the omap controller attibutes such as shift none, reset regs post idle, register the reset
      routine.
     - After filling the pdata structure, next step is to allocate and initialize the platform device by invoking
       omap_device_build. This takes the following arguments - 
         name - (This is defined as omap_i2c and is the once used for binding the platform device with the platform driver located at drivers/i2c/busses/i2c-omap.c(discussed latter). 
         bus_id - i2c bus number
         oh - omap hw module
         platform_data (pdata - omap_i2c_bus_platform_data)
     - This is turn invokes the omap_device_build_ss. This does the following things:
        + Allocate the platform device and set the name of the device. This is used for binding.
	+ Allocates the omap device with omap_device_alloc. This is where the resource structure is filled as per hw_mod data. This includes the things such as irq, memory mapping and so on. Also, it invokes the platform_device_add_resource to add the resources to the platform device. These resource are passed as an argument to the corresponding platform driver at drivers/i2c/buses/i2c-omap, which maps the virtual address for corresponding i2c bus. 
	+ Add the device data to the driver and the register the platform device with platform_device_add.
       With this we have registered the i2c bus 2 and i2c device in one go.
        + Also as part of omap_device_register, we take the resource info such as memory resouce, irq resource from 
	the oh and add the resources to the platform device.
  Let's discuss the next component which is I2C adapter driver:
      + The I2C adapter driver for am335x is located at drivers/i2c/busses/i2c-omap.c
      + The adapter driver is defined as platform driver with following structure:
      static struct platform_driver omap_i2c_driver = {
      	.probe          = omap_i2c_probe,
      	.remove         = omap_i2c_remove,
      	.driver         = {
      		.name   = "omap_i2c",
      		.owner  = THIS_MODULE,
      		.pm     = OMAP_I2C_PM_OPS,
      	},
      }
	Whenever any bus is registered as seen above, the probe of this driver is invoked. Following are things done in probe:
	 + We get the platform resources such as mem and irq. Request the memory regions. Then we allocate the omap_i2c_device which consists of fields as below:
	    + struct device
	    + void __iomem *base
	      int irq, struct resource *ioarea, 
	      speed, struct i2c_adapter,
	    + we initialize these fields from the pdev which we got as parameter, do the ioremp and set omap_device as drv_data for pdev. We also do the ioremap of the base address for i2c bus.
	    + Next step is get the FIFO depth for this i2C and set the FIFO notification Threshold to the half of fifo size 
	    + Then we invoke the omap_i2c_init to initialize the i2c bus. Register the irq handler. This does the following:
	       + Soft reset the I2C controller
	       + Set the Auto Idle mode for this engine and various other pm features. Then, we set the function clock to 
	       12MHz.
	       + Then take the i2c controller out of reset and enable the interrupts.
	    + Then we register the handler with request_irq function
	    + Next thing is to initialize the adapter device as below:
	      set omap_i2c_device as driver data for adapter and set OWNER and class which is I2C class. 
	      Next thing is to give the name as "OMAP I2c adapter"
	      iniatialize the adap algo with omap_i2c_algo
	      initialize the adapter number and then we finally invoke the i2c_add_numbered_adapter which does the following:
	        + I2C numbered adapter is basically for registering the static adapters i.e. static bus numbers, typically at board initialization time and used for SOC chip adapters. Here, i2c_board_info is used for configuring the i2c devices 
	        + It allocate the IDR (ID to pointer) for adapter. Gets us the pointer from adapter without needing table.
		+ Then we call the i2c_register_adapter which does the following:
		   + Intialize the adap->bus to i2c_bus_type and adap-dev.type = i2c_adap_type and then call the device register to register the adapter driver.
		   + It invokes the i2c_scan_static_board which does the following:
		      + if the board_info bus number matches with the adapter number, it invokes the i2_new_device which in turn creates the client device for the i2c devices registered in the board dependent file with bus number that of this adapter. The client is updated with platform_data, flags, address, irq as passed by i2c_board_info, i2c_adapter.
		      + Then, notify the drivers which are registered for I2c of the availability of the new adapter and invokes the process_new_adapter for that driver (need to understand fuly as it's internally calls the i2c_detect for new i2c subsystem and for legacy it invokes the attach function (what was this attach used for earlier and which driver does it belong to? adapter driver or i2c device driver. 
		      + once the registration of the client is done, the probe of the driver may get invoke if the driver is
		        registered.
	Following is the summary of the probe operation for the omap I2C adapter:
	 + First map the io regions passed by the corresponding platform device. Get the IRQ's and form the virtual address for the memory addresses.
	 + Allocate the omap_i2c_device and assign the virtual address to the base_addr field of the omap device.
	 + Update the speed, irq and device fields accordingly. Set the driver data to the omap2_device.
	 + Check the version of I2C registers and assign the respective array to the dev->regs
	 + Read the FIFO size for I2C engine and update the dev-fifo_size with half of the FIFO size.
	 + Initialize the I2C engine with omap_i2c_init as follows:
	 	+ Disable the I2C controller and reset the I2C controller
		+ set up the various PM parameters such as AUTO IDLE and so on
		+ Configure the wake up sources
		+ Configure the clock rate for the I2C operation
		+ Configure the RX and TX FIFO Thresholds for interrupts
		+ Enable the I2C engine and bring it out of reset
		+ Enable the interrupts such as TX Ready, RX ready, ACk level and so on.
	+ Register the ISR
	+ Next step is to initialize the adapter by assigning the dev-adap to local pointer.
	+ Setting the adapter data to dev and adapter class to HW Mon
	+ Assign the adapter algo
	+ Assign the nr to the adapter and invoke the i2c_add_numbered_adapter, which in turn does the following:
		+ Declares an I2C adapter when its number matters. It's used for SOC I2C's. Read the description at function definition  for more info.
		+ Gets an Idr for an adapter. Then, we invoke the register_i2c_adapter, which in turn does the following.
		+ Set the bus type to i2c_bus_type and dev type to i2c_adapter and then register the adapter device.
		+ Next step is to scan the board_info with i2c_scan_static_board_info, which in turn does the following:
		  + scans the devices for which bus num is equal to the adapter number and invoke i2c_new_device for it. The job of i2c_add_new is to instantiate the client corresponding to the board info and initialize with following:
		  	+ client->adapt, client->dev.platform_data, client->flags, client->addr and irq.
			+ Copy the client name, check the address validity and address duplication.
			+ set the parent, bus and type of dev. Set the client device name to bus_id-address. So, client on bus 1 with address 4 will have an name 1-4. Then, we finally register the device with device_register. After calling this register the probe for the driver may get invoke.
		+ Then notify the client drivers of the availability of the adapter by invoking the bus_for_each_drv with following arguments:
			+ bus_type is i2c_bus_type
			+ start - NULL
			+ data - pointer to adapter
			+ fn - _process_new_adapter
		+ So, for each driver, the process_new_adapter is invoked, which in turn invokes the i2c_do_add_adapter. Then, we invoke the i2c_detect function, but it is not registered by our driver, so it just returns 0.
	So, this finishes the probe for our driver.

	
	Next component of the i2c subsystem is the I2c device driver. For this examples, I am considering the eeprom driver,
	located at drivers/misc/eeprom/at24.c. Below is the structure for the same. It has the name as well as id_table. Our
	board specific eeprom driver uses the id table as below. In the init of the driver, we invoke i2c_add_driver which registers
	the driver with the i2c subsystem. Here, we define the name, probe, remove and id_table. The i2c_add_driver in turns invokes
	the  i2c_register_driver() which registers the i2c driver and invokes the probe for this driver for all the devices.
	+ Then we invoke the i2c_for_each_dev, which iterates through all the registered drivers.
	        {
			                /* Baseboard board EEPROM */
			I2C_BOARD_INFO("24c256", BASEBOARD_I2C_ADDR),
			.platform_data  = &am335x_baseboard_eeprom_info,
		},

	static struct i2c_driver at24_driver = {
		.driver = {
			.name = "at24",
			.owner = THIS_MODULE,
		},
		.probe = at24_probe,
		.remove = __devexit_p(at24_remove),
		.id_table = at24_ids,
	};

	static int __init at24_init(void)
	{
		if (!io_limit) {
		pr_err("at24: io_limit must not be 0!\n");
		return -EINVAL;
	}

		io_limit = rounddown_pow_of_two(io_limit);
		return i2c_add_driver(&at24_driver);
	}
module_init(at24_init);

     Next step is that as a part of i2c_register_driver, the probe of the driver was invoked for all the all the registered clients. The Probe does the following things:
     + Probe is being passed 2 arguments - i2c_client and i2c_device_id. From the board dependent file, we have passed platform data of type at24_platform_data which contains the following.

     static struct at24_platform_data am335x_baseboard_eeprom_info = {
	     .byte_len       = (256*1024) / 8,
	     .page_size      = 64,
	     .flags          = AT24_FLAG_ADDR16,
	     .setup          = am335x_evm_setup,
	     .context        = (void *)NULL,
     };
     + Next, we check if the chip_len and page size is power of 2. Then, we check for the adapter functionality. Next step is to calcualate the number of clients required to fulfill the total address space and then we allocate the memory for that at24_data and those number of client pointers. The at24_data is defined as below:
     struct at24_data {
	     struct at24_platform_data chip;
	     struct memory_accessor macc;
	     int use_smbus;

	     /*
	      *          * Lock protects against activities from other Linux tasks,
	      *                   * but not from changes by other I2C masters.
	      *                            */
	     struct mutex lock;
	     struct bin_attribute bin;

	     u8 *writebuf;
	     unsigned write_max;
	     unsigned num_addresses;

	     /*
	      *          * Some chips tie up multiple I2C addresses; dummy devices reserve
	      *                   * them for us, and we'll use them with SMBus calls.
	      *                            */
	     struct i2c_client *client[];
     };
	Next thing is to init the mutext and initialize the chip with the platform_data and number of addresses calculated.
	+ Next step is to initialize the sysfs bin attributes and give the name "eeprom" to it, set the mode to read only, assign the read operation for it, initialize the size to the byte_len. Then we initialize the macc.read and macc.write operations of at24_data. Then, we allocate the write buffer of 64 bytes. Then assign the client to client[0];
	Then, we create the sysfs entry and assign the at24_data to the client. and finally invoke the setup function.
	+ So, as seen above, we have two functions for read - at24_bin_read and at24_macc_read and 2 for write - at24_macc_write and at24_bin_write. The macc_read and macc_write allow other kernel code to access the eeprom data for eg. ethernet address or other calibration data. Both of these eventually invoke the at24_read and at24_write. 
	+ Let's understand the at24_read first. It takes 4 arguments - at24_data, buf, offset and count. First it takes the mutex lock and then invokes the at24_eeprom_read, this in turn does the following:
	   + we have 2 message buffers - i2c_msg msg[2] - to hold the data and u8 msg_buff[2] - to hold the address.
	   + Then, we get the client index with at24_translate_offset and msg_buff with offset and msg[0].addr, msg[0].buf and msg[0].len with client->addr, msg_buff  and address_size repectively. Fill the msg[1].addr, msg[1].flag, msg[1].buff and msg[1].len with address, I2C_M_RD, pointer to data buffer and length respectively. So, the idea over here is to transfer the eeprom address offset first and then the actual data. Then, we invoke the i2c_tranfer, with client->adapter, msg and 2 as argument. This in turn does the following:
	   	+In this we lock the adapter with i2c_lock_adapter and then invoke the master_xfer with 3 args - adap, msgs and num.
	+ From here, we jump to omap_i2c_xfer in  drivers/i2c/busses/i2c-omap.c which does the following stuff:
	  + First get the omap_i2c_dev from the adapter device. Then we wait for the bus to become idle. Then finally invoke the
	  low level omap_i2c_xfer_msg with 3 args - adapter, i2c_msg and cnt. First we write the SA_REG with the slave address, initialize the dev->buff and dev->buf_len and initialize the CNT register. Clear the FIFO buffers, initialize the completion cmd.
	  + Next step is to enable the controller, set the MST mode and send the start condition and set the transfer mode. Then, wait for command completion with wait_for_completion_timeout.
	  + The real transfer happens in the ISR omap_i2c_isr, where we do the following:
	   + Acknowlege the conditions other than RDR, RRDY, XDR, XRDY. Mark NACK as an error. If the stat is ARDY, RRDR,XRDY or XDR, then signal the completion command and notify the error if any in dev->err.
	   + Suppose the status is RRDY or RDR, this means we are ready to receive the bytes. We take the no. of bytes to fifo size for the recieving or if its RDR, then read the BUFFSTAT to figure out how many bytes to read for draining. Then we start reading the data from the I2C_DATA register and put it in dev->buf and finally acknowledge the read. similar is the thing for transmision.
