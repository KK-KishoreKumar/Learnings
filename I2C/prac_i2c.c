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
	
	Next component of the i2c subsystem is the I2c device driver. For this examples, I am considering the eeprom driver,
	located at drivers/misc/eeprom/at24.c. Below is the structure for the same. It has the name as well as id_table. Our
	board specific eeprom driver uses the id table as below. In the init of the driver, we invoke i2c_add_driver which registers
	the driver with the i2c subsystem. Here, we define the name, probe, remove and id_table. The i2c_add_driver in turns invokes
	the  i2c_register_driver() which invokes the probe for all the devices.
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


