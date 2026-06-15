/********************************************************************************************/
/*		Project		:	VPU						    */
/*		Filename	:	UART.c						    */
/*		Functionality	:	Uart2/Uart3 Process				    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include<dirent.h>

#include "../SystemDefines.h"
/********************************************************************************************/
/*                          Defines	                                                    */
/********************************************************************************************/
#define UART1	0
#define UART2	1
#define UART3	2
#define DEBUG_PRINT printf

				
/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/ 
//char MTime[7];
char ptimecode[7];
unsigned char CoreTemp;
//int PpathBootloaderState;

static int CmdSent;
static int NanoResetCmd=0;


static int Cport[3],error,RxState=-1,UART3RxState=-1;
static struct termios new_port_settings,old_port_settings[3];
static char comports[3][16]=
{
	"/dev/tty0",
	"/dev/ttyTHS1",// vineeth
	"/dev/ttyS3" // sudheesh
};
static char UART2Txbuffer[4*1024];
static char UART2Rxbuffer[4*1024];
static CIRCULAR_FIFO UART2TxFifo;
static CIRCULAR_FIFO UART2RxFifo;
//static char UART3Txbuffer[32*1024];
//static char UART3Rxbuffer[32*1024];
//static CIRCULAR_FIFO UART3TxFifo;
//static CIRCULAR_FIFO UART3RxFifo;
static XMEGA_INTERFACE XmegaCtrl;
static int TimeOk=-1;
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
static int SerialOpenComport(int, int, const char *);
static int SerialPollComport(int);
static int SerialSendByte(int, unsigned char);
static int SerialSendBuf(int, unsigned char *, int);
static int SerialIsDCDEnabled(int);
static int SerialIsCTSEnabled(int);
static int SerialIsDSREnabled(int);
static void SerialCloseComport(int);
static void SerialCPuts(int, const char *);
static void SerialEnableDTR(int);
static void SerialDisableDTR(int);
static void SerialEnableRTS(int);
static void SerialDisableRTS(int);
static void SerialTransmit(void);
static void SerialReceive(void);
static void SerialRead(void);
static void SerialWrite(unsigned char command,unsigned char *macid,char *message,unsigned short length);
static int SerialReadComport(int comport_number, unsigned char *buf, int size);

static void ReadNanoTemperature(void);
//static void UART3Read(int pipefd,AVRCTRL *tmpctrl);
static void UART2Read(void);
//static void UART3Receive(void);
static void UART2Receive(void);
//static void UART3Transmit(int pipefd);
static void UART2Transmit(void);
static void UART2SDSHWrite(unsigned char cmd,unsigned char dat);

static int RTCDataCheck(int Second,int Minute,int Hour,int Date,int Month,int Year);
//static void MyDisplayBuffer2 (unsigned char * source, unsigned short buffer_length);
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
static int SerialOpenComport(int comport_number, int baudrate, const char *mode)
{
	int baudr,status;
	int cbits=CS8,cpar=0,ipar=IGNPAR,bstop=0;

	if((comport_number>2)||(comport_number<0))
	{
		printf("SERIAL:Illegal comport number\n");
		return(1);
	}

	switch(baudrate)
	{
		case    2400 : 	baudr = B2400;
			   	break;
		case    4800 : 	baudr = B4800;
			   	break;
		case    9600 : 	baudr = B9600;
			   	break;
		case   19200 : 	baudr = B19200;
			   	break;
		case   38400 : 	baudr = B38400;
			   	break;
		case   57600 : 	baudr = B57600;
			   	break;
		case  115200 : 	baudr = B115200;
			   	break;
		case  230400 : 	baudr = B230400;
			   	break;
		case  460800 : 	baudr = B460800;
			   	break;
		case  500000 : 	baudr = B500000;
			   	break;
		case  576000 : 	baudr = B576000;
			   	break;
		case  921600 : 	baudr = B921600;
			   	break;
		case 1000000 : 	baudr = B1000000;
			   	break;
		case 1152000 : 	baudr = B1152000;
			   	break;
		case 1500000 : 	baudr = B1500000;
			   	break;
		case 2000000 : 	baudr = B2000000;
			   	break;
		case 2500000 : 	baudr = B2500000;
			   	break;
		case 3000000 : 	baudr = B3000000;
			   	break;
		case 3500000 : 	baudr = B3500000;
			   	break;
		case 4000000 : 	baudr = B4000000;
			   	break;
		default      : 	printf("SERIAL:Invalid baudrate\n");
			   	return(1);
			   	break;
	}

	switch(mode[0])
	{
		case '8': 	cbits = CS8;
		      		break;
		case '7': 	cbits = CS7;
		      		break;
		case '6': 	cbits = CS6;
		      		break;
		case '5': 	cbits = CS5;
		      		break;
		default : 	printf("SERIAL:Invalid number of data-bits '%c'\n", mode[0]);
				return(1);
		      		break;
	}

	switch(mode[1])
	{
		case 'N':
		case 'n': 	cpar = 0;
		      		ipar = IGNPAR;
		      		break;
		case 'E':
		case 'e': 	cpar = PARENB;
		      		ipar = INPCK;
		      		break;
		case 'O':
		case 'o': 	cpar = (PARENB | PARODD);
		      		ipar = INPCK;
		      		break;
		default : 	printf("SERIAL:Invalid parity '%c'\n", mode[1]);
		      		return(1);
		      		break;
	}

	switch(mode[2])
	{
		case '1': 	bstop = 0;
		      		break;
		case '2': 	bstop = CSTOPB;
		      		break;
		default : 	printf("SERIAL:Invalid number of stop bits '%c'\n", mode[2]);
		      		return(1);
		      		break;
	}

	/*
	http://pubs.opengroup.org/onlinepubs/7908799/xsh/termios.h.html

	http://man7.org/linux/man-pages/man3/termios.3.html
	*/

	Cport[comport_number] = open(comports[comport_number], O_RDWR | O_NOCTTY | O_NDELAY);
	if(Cport[comport_number]==-1)
	{
		perror("SERIAL:Unable to open comport\n");
		return(1);
	}

	fcntl(Cport[comport_number],F_SETFL,O_NONBLOCK);

	error = tcgetattr(Cport[comport_number], old_port_settings + comport_number);
	if(error==-1)
	{
		close(Cport[comport_number]);
		perror("SERIAL:Unable to read portsettings\n");
		return(1);
	}
	memset(&new_port_settings, 0, sizeof(new_port_settings));  /* clear the new struct */

	new_port_settings.c_cflag = cbits | cpar | bstop | CLOCAL | CREAD;
	new_port_settings.c_iflag = ipar;
	new_port_settings.c_oflag = 0;
	new_port_settings.c_lflag = 0;
	new_port_settings.c_cc[VMIN] = 0;      /* block untill n bytes are received */
	new_port_settings.c_cc[VTIME] = 0;     /* block untill a timer expires (n * 100 mSec.) */

	cfsetispeed(&new_port_settings, baudr);
	cfsetospeed(&new_port_settings, baudr);

	error = tcsetattr(Cport[comport_number], TCSANOW, &new_port_settings);
	if(error==-1)
	{
		close(Cport[comport_number]);
		perror("SERIAL:Unable to adjust portsettings\n");
		return(1);
	}

	/*if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("unable to get portstatus");
		return(1);
	}*/

	//status |= TIOCM_DTR;    /* turn on DTR */
	//status |= TIOCM_RTS;    /* turn on RTS */

	/*if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("unable to set portstatus");
		return(1);
	}*/

	return(0);
}
 
 
static int SerialPollComport(int comport_number)//, unsigned char *buf, int size)
{
	int n=0;

	ioctl(Cport[comport_number], TIOCINQ, &n);
	if (n > 0) 
	{
		return n;
	}

	return 0;
	
}
 
static int SerialReadComport(int comport_number, unsigned char *buf, int size)
{
	int nRet;

	nRet=read(Cport[comport_number], buf, size);

	return nRet;
}

static int SerialSendByte(int comport_number, unsigned char byte)
{
	int n;
	//printf("%c\n",byte);
	n = write(Cport[comport_number], &byte, 1);
	if(n<0)  
	{
		return(1);
	}

	return(0);
}
 
 
static int SerialSendBuf(int comport_number, unsigned char *buf, int size)
{
  	return(write(Cport[comport_number], buf, size));
}
 
 
static void SerialCloseComport(int comport_number)
{
	int status;

	if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("SERIAL:Unable to get portstatus\n");
	}

	status &= ~TIOCM_DTR;    /* turn off DTR */
	status &= ~TIOCM_RTS;    /* turn off RTS */

	if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("SERIAL:Unable to set portstatus\n");
	}

	tcsetattr(Cport[comport_number], TCSANOW, old_port_settings + comport_number);
	close(Cport[comport_number]);
}
 
/*
Constant  Description
TIOCM_LE        DSR (data set ready/line enable)
TIOCM_DTR       DTR (data terminal ready)
TIOCM_RTS       RTS (request to send)
TIOCM_ST        Secondary TXD (transmit)
TIOCM_SR        Secondary RXD (receive)
TIOCM_CTS       CTS (clear to send)
TIOCM_CAR       DCD (data carrier detect)
TIOCM_CD        see TIOCM_CAR
TIOCM_RNG       RNG (ring)
TIOCM_RI        see TIOCM_RNG
TIOCM_DSR       DSR (data set ready)
 
http://man7.org/linux/man-pages/man4/tty_ioctl.4.html
*/ 
static int SerialIsDCDEnabled(int comport_number)
{
	int status;

	ioctl(Cport[comport_number], TIOCMGET, &status);

	if(status&TIOCM_CAR) 
	{
		return(1);
	}
	else 
	{
		return(0);
	}
}
 
static int SerialIsCTSEnabled(int comport_number)
{
	int status;

	ioctl(Cport[comport_number], TIOCMGET, &status);

	if(status&TIOCM_CTS) 
	{
		return(1);
	}
	else 
	{
		return(0);
	}
}
 
static int SerialIsDSREnabled(int comport_number)
{
	int status;

	ioctl(Cport[comport_number], TIOCMGET, &status);

	if(status&TIOCM_DSR) 
	{
		return(1);
	}
	else 
	{
		return(0);
	}
}
 
static void SerialEnableDTR(int comport_number)
{
	int status;

	if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("SERIAL:Unable to get portstatus\n");
	}

	status |= TIOCM_DTR;    /* turn on DTR */

	if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("SERIAL:unable to set portstatus\n");
	}
}
 
static void SerialDisableDTR(int comport_number)
{
	int status;

	if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("SERIAL:Unable to get portstatus\n");
	}

	status &= ~TIOCM_DTR;    /* turn off DTR */

	if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("SERIAL:unable to set portstatus\n");
	}
}
 
static void SerialEnableRTS(int comport_number)
{
	int status;

	if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("SERIAL:Unable to get portstatus\n");
	}

	status |= TIOCM_RTS;    /* turn on RTS */

	if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("SERIAL:unable to set portstatus\n");
	}
}
 
static void SerialDisableRTS(int comport_number)
{
	int status;

	if(ioctl(Cport[comport_number], TIOCMGET, &status) == -1)
	{
		perror("SERIAL:Unable to get portstatus\n");
	}

	status &= ~TIOCM_RTS;    /* turn off RTS */

	if(ioctl(Cport[comport_number], TIOCMSET, &status) == -1)
	{
		perror("SERIAL:unable to set portstatus\n");
	}
}
 
 
 
static void SerialCPuts(int comport_number, const char *text)  /* sends a string to serial port */
{
	while(*text != 0)   
	{
		SerialSendByte(comport_number, *(text++));
	}
}


/*
 * VPU Specific Routines start here:
 */


/*
 * Configure UART2
 */
static void ConfigureSDSHUART2(void)
{

	int  bdrate=115200;    	
	char mode[]={'8','N','1',0}; 
	int i=0,j=0,k=0;
	unsigned short calc_cs=0;

	if(SerialOpenComport(UART2, bdrate, mode))
	{
		printf("UART2:Can not open comport\n");
		//DEBUG_PRINT("UART2:Can not open comport\n");		
	}
	FifoInit(&UART2TxFifo,4*1024,(unsigned char *)UART2Txbuffer);
	FifoInit(&UART2RxFifo,4*1024,(unsigned char *)UART2Rxbuffer);


	RxState=-1;
	
	/*for(i=0;i<1000;i++)
	{
		SerialSendByte(UART2,i);
	}*/
	
	printf("UART2 Init Done\n");
	//DEBUG_PRINT("UART2 Init Done\n");

}


static unsigned char RTCWriteBuffer[16];
/*
 * Fill UART2 Tx buffer with UART2(Vineeth) protocol packets
 */
static void UART2SDSHWrite(unsigned char cmd,unsigned char dat)
{
	unsigned char buffer[5];
	unsigned char tempbuffer[32];	
	int rt,i,calc_checksum=0;
	
	buffer[0]=0x5A;
	buffer[1]=0x7E;
	buffer[2]=cmd;
	if(cmd==NANO_CORE_TEMP)
	{
		//ReadNanoTemperature(pipefd);
		buffer[3]=XmegaCtrl.ToXmegaNanoCoreTemp;
		printf("^^^^^^^^^^^^^^^^>>>******Core Temp = %x \n",buffer[3]);
		//DEBUG_PRINT("^^^^^^^^^^^^^^^^>>>******Core Temp = %x \n",buffer[3]);
	}
	else
	{
		if(cmd==WRITE_RTC_CMD)
		{
			tempbuffer[0]=0x5A;
			tempbuffer[1]=0x7E;
			tempbuffer[2]=cmd;
			mempcpy(&tempbuffer[3],RTCWriteBuffer,14);
		}
		else
		{
			buffer[3]=dat;
		}
	}
	
	if(cmd==WRITE_RTC_CMD)
	{
		for(i=0;i<17;i++)
		{
			calc_checksum+=tempbuffer[i];
		}
		tempbuffer[17]=calc_checksum;	
		rt=FifoWrite(&UART2TxFifo,(unsigned char *)tempbuffer,18);
		printf("WRITE_RTC_CMD:\n");
		//MyDisplayBuffer(pipefd,(unsigned char *)tempbuffer,32);	

	}
	else
	{
		buffer[4]= 0x5A + 0x7E + cmd +buffer[3];
		rt=FifoWrite(&UART2TxFifo,(unsigned char *)buffer,5);
		printf("UART2Write: %x %x %x %x %x\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
		//DEBUG_PRINT("UART2Write: %x %x %x %x %x\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
	}
		
	CmdSent=0;
}

/*
 * Tx UART2 buffer
 */
static void UART2Transmit(void)
{
	unsigned char val;
	int len;

	if(UART2TxFifo.filled_length)
	{	
		len = FifoRead(&UART2TxFifo,&val,1);
		SerialSendByte(UART2,val);
		//printf("txed %d %x\n",(int)UART2TxFifo.filled_length,val);
	}

}
/*
 * Check if Tx UART2 buffer is empty
 */
static int CheckUART2SDSHTxBuffEmpty(void)
{
	if(UART2TxFifo.filled_length)
	{
		return 0;
	}
	
	return 1;
}


/*
 * Rx UART2 in buffer
 */
static void UART2Receive(void)
{
	char so_buff[1024];
	unsigned short len;
	int i;

	len=SerialPollComport(UART2);		
	if(len > 0)
	{
		
		i=SerialReadComport(UART2,(unsigned char *)so_buff,1024);
		len = FifoWrite(&UART2RxFifo,(unsigned char *)so_buff,i);
		//MyDisplayBuffer((unsigned char *)UART2Rxbuffer,1024);						
	}
	
}


/*
 * Read UART2 buffer.Check protocol and parse the data.
 */
static void UART2Read(void)
{
	unsigned char val=0,cmd=0,cmddat=0,rx_checksum=0,calc_checksum=0;
	unsigned char rtcdat[32];
	int i=0;
	static unsigned char rtc_pass=0;
	static long rtc_pass_cnt=0;	
	static int rtc_read_fail_cnt=0;	
	static int rtc_write_fail_cnt=0;
	struct timeval 		tval, tvalrtc;
	struct tm		brokentime, brokentimertc;
	
	DEFINE_FILE
	
	/*
	 *Timeout for rtc pass var
	 */	
	if(rtc_pass_cnt>1000000)
	{
		rtc_pass_cnt=0;
		rtc_pass=0;
		printf("READ_RTC_CMD timeout\n");
		UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
		//DEBUG_PRINT("READ_RTC_CMD timeout\n");
	}
	rtc_pass_cnt++;
	if(UART2RxFifo.filled_length>=5)
	{
		//printf("SERIAL:Fifo Data available.%d\n",SerialRxFifo.filled_length);	
		//DEBUG_PRINT("SERIAL:Fifo Data available.%d\n",SerialRxFifo.filled_length);			
		
		/*
		 *RTC functions added to Vinneth. RTC read ack and write commands have different data lengths.
		 *If read data length already matches in one iteration, it works  as usual.
		 *If not, To accomodate them and maintain the existing protocol, rtc_pass variable has been introduced to 
		 *iterate the data read process and reach the required data length. 
		 */
		if(!rtc_pass) 
			FifoRead(&UART2RxFifo,&val,1);	
		if((val==0x5A)||(rtc_pass))
		{
			if(!rtc_pass)
				FifoRead(&UART2RxFifo,&val,1);	
			if((val==0x7E)||(rtc_pass))
			{
				printf("Header Received\n");			
				if(!rtc_pass)
					FifoRead(&UART2RxFifo,&cmd,1);	
				if((cmd==READ_RTC_ACK)||(rtc_pass))
				{
					rtc_pass=1;					
					if(UART2RxFifo.filled_length>17)
					{
						rtc_pass=0;
						rtc_pass_cnt=0;
						FifoRead(&UART2RxFifo,rtcdat,17);
						calc_checksum = (unsigned char)(0x5A + 0x7E + READ_RTC_ACK);
						for(i=0;i<17;i++)
						{
							calc_checksum+=rtcdat[i];
						}
						FifoRead(&UART2RxFifo,&rx_checksum,1);
						//MyDisplayBuffer2(rtcdat,24);
						if(rx_checksum==calc_checksum)
						{
							printf("READ_RTC_ACK cmd OSF:%x BF:%x BLF:%x\n",rtcdat[14],rtcdat[15],rtcdat[16]);
							if(rtcdat[16])
							{
								LOGTOFILE("BATLOW","BATLOW",6);
							}
							if(rtcdat[14])
							{
								LOGTOFILE("OSF","OSF",3);
							}
							//DEBUG_PRINT("READ_RTC_ACK \n");
							printf("UART2 RTC Time = %d:%d:%d %d:%d:%d\n",rtcdat[2],rtcdat[1],rtcdat[0],rtcdat[3],rtcdat[5],rtcdat[6]);
							//DEBUG_PRINT("UART2 RTC Time = %d:%d:%d %d:%d:%d\n",rtcdat[2],rtcdat[1],rtcdat[0],rtcdat[3],rtcdat[5],rtcdat[6]);
							
							
							if(RTCDataCheck(BCD_2_BIN00(rtcdat[0]),BCD_2_BIN00(rtcdat[1]),BCD_2_BIN00(rtcdat[2]),
									BCD_2_BIN00(rtcdat[3]),BCD_2_BIN00(rtcdat[5])-1,BCD_2_BIN00(rtcdat[6])+2000)==1)
							{
								printf("UART2:RTC Read Data Check OK %ld\n",rtc_pass_cnt);
								//DEBUG_PRINT("UART2:RTC Read Data Check OK\n");
								
								/*tmpctrl->RTCTimeCode[0]=BCD_2_BIN00(rtcdat[0]);
								tmpctrl->RTCTimeCode[1]=BCD_2_BIN00(rtcdat[1]);
								tmpctrl->RTCTimeCode[2]=BCD_2_BIN00(rtcdat[2]);
								tmpctrl->RTCTimeCode[3]=BCD_2_BIN00(rtcdat[3]);
								tmpctrl->RTCTimeCode[4]=BCD_2_BIN00(rtcdat[5]);
								tmpctrl->RTCTimeCode[5]=BCD_2_BIN00(rtcdat[6]);

								tmpctrl->BatteryLowFlag=rtcdat[16];
								tmpctrl->OscillatorStopFlag=rtcdat[14];
									
								tmpctrl->RTCCmd=3;*/

								brokentime.tm_sec = BCD_2_BIN00(rtcdat[0]);//seconds
								brokentime.tm_min = BCD_2_BIN00(rtcdat[1]);//minutes
								brokentime.tm_hour= BCD_2_BIN00(rtcdat[2]);//hour
								brokentime.tm_mday= BCD_2_BIN00(rtcdat[3]);//date
								brokentime.tm_mon = BCD_2_BIN00(rtcdat[5])-1;//month
								brokentime.tm_year= BCD_2_BIN00(rtcdat[6])+100;//year
								printf("Linux Set Time = %d:%d:%d %d:%d:%d\n",BCD_2_BIN00(rtcdat[2]),
														BCD_2_BIN00(rtcdat[1]),BCD_2_BIN00(rtcdat[0]),
											BCD_2_BIN00(rtcdat[3]),BCD_2_BIN00(rtcdat[5])-1,BCD_2_BIN00(rtcdat[6])+100);								
								tval.tv_sec = mktime(&brokentime);
								tval.tv_usec = 0;
								if(settimeofday(&tval, NULL) != 0)
								{
									printf("set time error\n");
								}
								usleep(250);
								sync();
											
								TimeOk=1;
							}
							else
							{
								printf("UART2:RTC Read Data Check Fail %ld\n",rtc_pass_cnt);
								//DEBUG_PRINT("UART2:RTC Read Data Check Fail\n");
								//tmpctrl->RTCCmd=5;
								//UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
								rtc_pass_cnt=0;
								rtc_pass=0;	
								
								brokentime.tm_sec = 0;//BCD_2_BIN00(rtcdat[0]);//seconds
								brokentime.tm_min = 0;//BCD_2_BIN00(rtcdat[1]);//minutes
								brokentime.tm_hour= 0;//BCD_2_BIN00(rtcdat[2]);//hour
								brokentime.tm_mday= 1;//BCD_2_BIN00(rtcdat[3]);//date
								brokentime.tm_mon = 0;//BCD_2_BIN00(rtcdat[5])-1;//month
								brokentime.tm_year= 10+100;//year
								printf("Linux Set Time = %d:%d:%d %d:%d:%d\n",0,0,0,1,0,10+100);								
								tval.tv_sec = mktime(&brokentime);
								tval.tv_usec = 0;
								if(settimeofday(&tval, NULL) != 0)
								{
									printf("set time error\n");
								}
								usleep(250);
								sync();
								TimeOk=1;
																	
							}
						}
						else
						{
							printf("UART2:RTC Read ACK Checksum Error rx:%x calc:%x %ld\n",rx_checksum,calc_checksum,rtc_pass_cnt);
							//DEBUG_PRINT("UART2:RTC Read ACK Checksum Error rx:%x calc:%x\n",rx_checksum,calc_checksum);
							//tmpctrl->RTCCmd=5;
							UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
							rtc_pass_cnt=0;
							rtc_pass=0;							
						}
					}
				}
				else
				{
					FifoRead(&UART2RxFifo,&cmddat,1);		
					FifoRead(&UART2RxFifo,&rx_checksum,1);
					calc_checksum= 0x5A + 0x7E + cmd + cmddat;
					printf("cmd=%x data=%x cs=%x calc_checksum=%x\n",cmd,cmddat,rx_checksum,calc_checksum);
					//DEBUG_PRINT("cmd=%x data=%x cs=%x calc_checksum=%x\n",cmd,cmddat,rx_checksum,calc_checksum);		
					if(rx_checksum==calc_checksum)
					{
						if(cmd==READ_RTC_NAK)
						{
							rtc_read_fail_cnt++;	
							printf("READ_RTC_NAK cmd %d\n",rtc_read_fail_cnt);	
							//DEBUG_PRINT("READ_RTC_NAK cmd %d\n",rtc_read_fail_cnt);
							//tmpctrl->RTCCmd=5;
							UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
							rtc_pass_cnt=0;
							rtc_pass=0;							
							
							/*
							 *Send RTC Read CMD to UART2 Vineeth
							 */
							//UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);								
						}					
						
					}
					else
					{
						printf("UART2:Checksum Error rx:%x calc:%x %ld\n",rx_checksum,calc_checksum,rtc_pass_cnt);
						//DEBUG_PRINT("UART2:Checksum Error rx:%x calc:%x\n",rx_checksum,calc_checksum);
						UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
						rtc_pass_cnt=0;
						rtc_pass=0;							
						
					}	
				}
			}
		}
	}				
}

/*
 *UART2 Loop process
 */												
static void UART2SDSHProcess(void)
{
	unsigned char dat[4];

	UART2Transmit();
	UART2Receive();
	UART2Read();
	
}


static int RTCDataCheck(int Second,int Minute,int Hour,int Date,int Month,int Year)
{
	
	
	if((Date<1)||(Second<0)||(Minute<0)||(Hour<0)||(Month<0)||(Year<0))
	{
	    	printf("2.RTC returned invalid negative data \n");  
	    	printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
		//DEBUG_PRINT("2.RTC returned invalid negative data \n");
		return -1;
	}  
	else
	{
		if((Second>59)||(Minute>59)||(Hour>23)||(Month>11))
		{
		    	printf("3.RTC returned invalid negative data \n");  
	    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
	    		//DEBUG_PRINT("3.RTC returned invalid negative data \n");
			return -1;
		}
		else
		{      
			if(((Month==3)||(Month==5)||(Month==8)||(Month==10))&&(Date>30))
			{
			    	printf("4.RTC returned invalid negative data \n");  
		    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
				//DEBUG_PRINT("4.RTC returned invalid negative data \n");
				return -1;
			}
			else
			{
				if(((Month==0)||(Month==2)||(Month==4)||(Month==6)||(Month==7)||(Month==9)||(Month==11))&&(Date>31))
				{
				    	printf("5.RTC returned invalid negative data \n");  
			    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
					//DEBUG_PRINT("5.RTC returned invalid negative data \n");
					return -1;
				}
				else
				{
					if(Year%100 == 0)
					{
						if(Year%400 == 0)
						{
							if((Month==1)&&(Date>29))
							{
							    	printf("6.RTC returned invalid negative data \n");  
						    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
								//DEBUG_PRINT("6.RTC returned invalid negative data \n");
								return -1;
							}
						}
					}
					else
					{					
						if(Year%4 == 0)
						{
							if((Month==1)&&(Date>29))
							{
							    	printf("7.RTC returned invalid negative data \n");  
						    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
								//DEBUG_PRINT("7.RTC returned invalid negative data \n");
								return -1;
							}
						}
						else
						{
							if((Month==1)&&(Date>28))
							{
							    	printf("8.RTC returned invalid negative data \n");  
						    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
								//DEBUG_PRINT("8.RTC returned invalid negative data \n");
								return -1;
							}
						}      
					}
				}
			}
		}
	}

	return 1;
}

int main()
{

	ConfigureSDSHUART2();
	UART2SDSHWrite(NANO_INIT,NULL_RTC_DAT);	
	sleep(1);
	printf("NANO INIT Sent \n");
	printf("RTC Read Cmd Sent \n");    
	UART2SDSHWrite(READ_RTC_CMD,NULL_RTC_DAT);	
	while(1)
	{
		UART2SDSHProcess();
		if(TimeOk==1)
		{
			SerialCloseComport(UART2);
			break;
		}
	}
	
	return 0;
}

/*
static void MyDisplayBuffer2 (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j,k;
	char c;

	k=buffer_length%8;
	printf("******Debug Print**********\n");
	//DEBUG_PRINT("******Debug Print**********\n");
	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		//DEBUG_PRINT("%04d :",i*8);
		for(j = 0; j < 8; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
			//DEBUG_PRINT(" %02x",(unsigned char) *(source+j+i*8));
		}
		printf("\t");
		//DEBUG_PRINT("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
			//DEBUG_PRINT("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
		//DEBUG_PRINT("\n");
	}
	
	if(k)
	{
		printf("%04d :",i*8);
		//DEBUG_PRINT("%04d :",i*8);
		for(j = 0; j < k; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
			//DEBUG_PRINT(" %02x",(unsigned char) *(source+j+i*8));
		}
		for(j = 0; j < (8-k); j++)
		{
			printf("   ");
			//DEBUG_PRINT("   ");
		}
		printf("\t");
		//DEBUG_PRINT("\t");
		for(j = 0; j < k; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
			//DEBUG_PRINT("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");    
	}
	printf("-----------Debug Print------------\n");
	//DEBUG_PRINT("-----------Debug Print------------\n");
}
*/





















