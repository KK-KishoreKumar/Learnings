//http://www.comptechdoc.org/os/linux/programming/c/linux_pgcserial.html
//com /dev/ttyS0 38400 8 1 0 4 
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

void signal_handler_IO (int status);    //definition of signal handler
int wait_flag=TRUE;                     //TRUE while no signal received
char devicename[80];
long Baud_Rate = 38400;         // default Baud Rate (110 through 38400)
long BAUD = B38400;                      // derived baud rate from command line
long DATABITS = CS8;
long STOPBITS = 1;
long PARITYON = 0;
long PARITY = 0;
//int Data_Bits = 8;              // Number of data bits
//int Stop_Bits = 1;              // Number of stop bits
//int Parity = 0;                 // Parity as follows:
                  // 00 = NONE, 01 = Odd, 02 = Even, 03 = Mark, 04 = Space
int Format = 4;
FILE *input;
FILE *output;
int status;
void signal_handler_IO (int status)
{
//    printf("received SIGIO signal.\n");
   wait_flag = 0;
}
main(int Parm_Count, char *Parms[])
{
   int fd, tty, c, res, i, error;
   char In1, Key;
   struct termios oldtio, newtio;       //place for old and new port settings for serial port
   struct termios oldkey, newkey;       //place tor old and new port settings for keyboard teletype
   struct sigaction saio;               //definition of signal action
   char buf[255];                       //buffer for where data is put
       
      //open the device(com port) to be non-blocking (read will return immediately)
      fd = open("/dev/ttytiny0", O_RDWR | O_NOCTTY | O_NONBLOCK);
      if (fd < 0)
      {
         perror(devicename);
         exit(-1);
      }
      //install the serial handler before making the device asynchronous
      saio.sa_handler = signal_handler_IO;
      sigemptyset(&saio.sa_mask);   //saio.sa_mask = 0;
      saio.sa_flags = 0;
      saio.sa_restorer = NULL;
      sigaction(SIGIO,&saio,NULL);

      // allow the process to receive SIGIO
      fcntl(fd, F_SETOWN, getpid());
      // Make the file descriptor asynchronous (the manual page says only
      // O_APPEND and O_NONBLOCK, will work with F_SETFL...)
      fcntl(fd, F_SETFL, FASYNC);

      tcgetattr(fd,&oldtio); // save current port settings 
      // set new port settings for canonical input processing 
      newtio.c_cflag = BAUD | CRTSCTS | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
      newtio.c_iflag = IGNPAR;
      newtio.c_oflag = 0;
      newtio.c_lflag = 0;       //ICANON;
      newtio.c_cc[VMIN]=1;
      newtio.c_cc[VTIME]=0;
      tcflush(fd, TCIFLUSH);
      tcsetattr(fd,TCSANOW,&newtio);
      int cnt = 0;

      // loop while waiting for input. normally we would do something useful here
      while (1)
      {
         // after receiving SIGIO, wait_flag = FALSE, input is available and can be read
         if (!wait_flag)  //if input is available
         {
            res = read(fd,buf,255);
            if (res>0)
            {
		printf("Got the input\n");
		printf("%c %c\n", buf[0], buf[1]);
		for (i = 0; i < res; i++)
			buf[i] += 3;
		write(fd, buf, res);
            	cnt++;   /* wait for new input */
		if (cnt >= 10)
			break;
            //wait_flag = 1;      /* wait for new input */
         }  //end if wait flag == FALSE
	 }
	

      }  //while stop==FALSE
      printf("Here\n");
      // restore old port settings
      tcsetattr(fd,TCSANOW,&oldtio);
      //tcsetattr(tty,TCSANOW,&oldkey);
      //close(tty);
      close(fd);        //close the com port
      printf("Here 1\n");
   //}  //end if command line entrys were correct
#if 0
   else  //give instructions on how to use the command line
   {
      fputs(instr,output);
      fputs(instr1,output);
      fputs(instr2,output);
      fputs(instr3,output);
      fputs(instr4,output);
      fputs(instr5,output);
      fputs(instr6,output);
      fputs(instr7,output);
   }
#endif
   //fclose(input);
   //fclose(output);
}  //end of main

/***************************************************************************
* signal handler. sets wait_flag to FALSE, to indicate above loop that     *
* characters have been received.                                           *
***************************************************************************/



