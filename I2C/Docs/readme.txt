What is start byte mode?
MODE_ALIAS is used for automatic loading of the driver when user space requests the access. 
https://lwn.net/Articles/47412/
http://stackoverflow.com/questions/22778879/what-is-module-alias-in-linux-device-driver-code

Let's have a look at the registers required to program the I2C interface. 
+ One of the register is the I2C_CON which is the configuration register for I2C module. It contains the bits such as Start condtion, I2CEN for enabling the I2C module, operation mode, fast standard. 
 - Configuring the master bit
 - TRX - When this bit is cleared, the I2C controller is in the receiver mode.
 - STP - for sending the stop condition on the bus. This condition is generated when COUNT bit reaches 0. When this bit is not set to
   1 by the master before the end of the transfer (DCOUNT = 0), the stop condition is not generated and the SCL line is hold to 0 by
  the master, which can restart a new transfer by setting the STT bit.
 - STT - Start condition
   + Set by the master to generate the start condition and reset to 0 by the hw when the start condtion is generated. start/stop
     can be configured to generate the different transfer formats:
     STT = 1 STP = 0, conditions = start, Bus activities = S-A-D
     STT = 0, STP = 1, condtions = stop, Bus activity = P
     STT = 1 STP =1, condtion - S-A-D-(n)-P
 + I2C_DATA - data register. entry/recieve points for the data
 + I2C_CNT - 16 bit countdown counter decrements by 1 for every byte recieved or sent through the i2C interface. read returns the
   number of bytes yet to be recieved or transmit. When DCOUNT reaches 0, the core generates a STOP condition if the stop condition
   was specified and the ARDY status flag is set to 1. If the stop bit is never set to 1, then the ARDY is raised and DCOUNT is 
  re-loaded. If we set the start condition now, the its a restart condition.
 + I2C_BUF - Enables the DMA transfers and allows the configuration of FIFO thresholds. 
    - RXFIFO_CLR - when set, recieve FIFO is cleared, automatically cleared by the h/w
    - RXTRSH - Recieve FIFO threshold value for data recieve transfers
    - TXFIFO_CLR - Tx FIFO threshold value for data transfer. trigger level for data transfers. 
 + I2C_IRQENABLE_SET - XDR, RDR, ROVR, XRDY, RRDY, BF, ARDY, NACK_IE, AL_IE. 
 + I2C_IRQ_STATUS - BB, RDR, XDR, XRDY, RRDY, ARDY


I2c-tools
i2cdetect -y -r 1
cat /sys/bus/i2c/devices/0-0050/modalias (driver name)
i2cdetect -y -r 0
i2cget -y -f 0 0x50 0x50
od -x /sys/bus/i2c/devices/0-0050/eeprom
