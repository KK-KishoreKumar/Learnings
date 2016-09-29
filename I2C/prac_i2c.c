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
	+ Allocates the omap device with omap_device_alloc
	+ Add the device data to the driver and the register the platform device with platform_device_add.
       With this we have registered the i2c bus 2 and i2c device in one go.
        + Also as part of omap_device_register, we take the resource info such as memory resouce, irq resource from 
	the oh and add the resources to the platform device.
