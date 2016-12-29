#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void
main(void) {
	int file;
	char filename[40];
	//const gchar *buffer;
	int addr = 0x51; //0b00101001;        // The I2C address of the ADC
	sprintf(filename,"/dev/i2c-0");
	if ((file = open(filename,O_RDWR)) < 0) {
		printf("Failed to open the bus.");
		/* ERROR HANDLING; you can check errno to see what went wrong */
		exit(1);
	}

	if (ioctl(file,I2C_SLAVE_FORCE,0x50) < 0) {
		printf("Failed to acquire bus access and/or talk to slave.\n");
		/* ERROR HANDLING; you can check errno to see what went wrong */
		exit(1);
	}

	char buf[100] = {0XAA, 0X55, 0XAA, 0X55};
	float data;
	char channel;
	int i;
	buf[0] = 0;
	buf[1] = 0;
	//for(i = 0; i<4; i++) {
		// Using I2C Read
		if (write(file,buf,4) != 4) {
			/* ERROR HANDLING: i2c transaction failed */
			printf("Failed to read from the i2c bus.\n");
			//buffer = g_strerror(errno);
			//printf(buffer);
			printf("\n\n");
		} else {
			//data = (float)((buf[0] & 0b00001111)<<8)+buf[1];
			//data = data/4096*5;
			//channel = ((buf[0] & 0b00110000)>>4);
			//printf("Channel %02d Data:  %04f\n",channel,data);
			sleep(1);
			buf[0] = 0X00;
			buf[1] = 0X00;
			write(file, buf, 2);
#if 1
			read(file, buf, 32);
			for (i = 0; i < 32; i++)
			{
				if (!(i % 15))
					printf("\n");
				printf("%d\t", buf[i]);
			}
#endif
		}
	//}
#if 0
	//unsigned char reg = 0x10; // Device register to access
	//    //buf[0] = reg;
	buf[0] = 0b11110000;

	if (write(file,buf,1) != 1) {
		/* ERROR HANDLING: i2c transaction failed */
		printf("Failed to write to the i2c bus.\n");
		buffer = g_strerror(errno);
		printf(buffer);
		printf("\n\n");
	}
#endif
}
