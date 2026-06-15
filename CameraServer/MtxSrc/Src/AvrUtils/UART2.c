/********************************************************************************************/
/*		Project		:	Nano Cam					    */
/*		Filename	:	Serial.c					    */
/*		Functionality	:	Second Uart Process				    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/

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
#include <dirent.h>
#include "SystemDefines.h"


/********************************************************************************************/
/*                          Defines	                                                    */
/********************************************************************************************/

#define UART1	0
#define UART2	1
#define UART3	2


				
/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
char ptimecode[7];
int TestTimer;
int CmdSent;
int NanoResetCmd=0;
extern struct cirfifo		*ProcessDebugFifo;
extern char 			DebugStr[1024];
extern int 			DebugStrSize;
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/ 
static int Cport[3],error,RxState=-1;
static struct termios new_port_settings,old_port_settings[3];
static char comports[3][16]=
{
	"/dev/tty0",//UARTA
	"/dev/ttyTHS1",//UARTB
	"/dev/ttyTHS2" //UARTC
};
static char UART2Txbuffer[1024];
static char UART2Rxbuffer[1024];
static CIRCULAR_FIFO UART2TxFifo;
static CIRCULAR_FIFO UART2RxFifo;
static XMEGA_INTERFACE XmegaCtrl;
static int ReadTimeCnt=0;
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
void NanoToXmegaTest();

static int RTCDataCheck(int Second,int Minute,int Hour,int Date,int Month,int Year);
extern void loganevent(const char *processname, const char *eventstr);
extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_atoi(const char *nptr, int *value);

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
	ssize_t nRet;

	nRet=read(Cport[comport_number], buf, size);

	return (int)nRet;
}

static int SerialSendByte(int comport_number, unsigned char byte)
{
	ssize_t n;
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
static void SerialTransmit(void)
{
	char so_buff[1024];
	unsigned short len;
	int i;

		SerialSendBuf(UARTB,so_buff,len);

}

static void SerialReceive(void)
{
	char so_buff[1024];
	unsigned short len;
	int i;

		len=SerialPollComport(UARTB);		
		if(len > 0)
		{
			if(len>=10)			
			{
				SerialReadComport(UARTB,so_buff,UART_RX_BUFF_SIZE);	
			}
			else
			{
				SerialReadComport(UARTB,so_buff,len);
			}
		}
	
}
*/

/*static void UART2Transmit(void)
{
	char so_buff[1024];
	unsigned short len;
	int i;

		SerialSendBuf(UARTB,so_buff,len);

}*/



void ConfigureUART2(void)
{

	int  bdrate=115200;    	
	char mode[]={'8','N','1',0}; 
	int i=0,j=0,k=0;
	unsigned short calc_cs=0;

	if(SerialOpenComport(UART2, bdrate, mode))
	{
		printf("SERIAL:Can not open comport\n");		
	}
	FifoInit(&UART2TxFifo,1024,(unsigned char *)UART2Txbuffer);
	FifoInit(&UART2RxFifo,1024,(unsigned char *)UART2Rxbuffer);


	RxState=-1;

}
/*
INIT_TIMER
int tempflag;*/

void ReadNanoTemperature(int pipefd)
{
	FILE *fptr;
	char tempbuff[32];
	int CPUTempval = 0,CPUTemp = 0;
	

	fptr=fopen("/sys/devices/virtual/thermal/thermal_zone1/temp","r");
	if(fptr!=NULL)
	{
		if((safe_fgets(tempbuff,32, 32, fptr))>=0)// returned succes...
		{
			if(safe_atoi(tempbuff, &CPUTempval) > 0)
				CPUTemp=(int)(CPUTempval/1000);
			else
				CPUTemp=0;//
		}
		(void)fclose(fptr);
	}
	XmegaCtrl.ToXmegaNanoCoreTemp=(unsigned char)CPUTemp;

//	printf("AO=%d CPU=%d GPU=%d PLL=%d PMIC=%d Fan=%d \n",AOTemp,CPUTemp,GPUTemp,PLLTemp,PMICTemp,FanTemp);
}
extern unsigned char RTCWriteBuffer[16];
extern void MyDisplayBuffer (unsigned char * source, unsigned short buffer_length);
void UART2Write(int pipefd,unsigned char cmd,unsigned char dat)
{
	unsigned char buffer[5];	
	int rt,i,calc_checksum=0;
	unsigned char tempbuffer[32];	
	
	
	buffer[0]=0x5A;
	buffer[1]=0x7E;
	buffer[2]=cmd;
	if(cmd==NANO_CORE_TEMP)
	{
		ReadNanoTemperature(pipefd);
		buffer[3]=XmegaCtrl.ToXmegaNanoCoreTemp;
		printf("^^^^^^^^^^^^^^^^>>>******Core Temp = %x \n",buffer[3]);
		
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
		MyDisplayBuffer((unsigned char *)tempbuffer,32);	

	}
	else
	{
		buffer[4]= 0x5A + 0x7E + cmd +buffer[3];
		rt=FifoWrite(&UART2TxFifo,(unsigned char *)buffer,5);
		//printf("UART2Write: %x %x %x %x %x\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
	}
	
	CmdSent=0;
	//START_TIMER
	//tempflag=1;
}


static void UART2Transmit(void)
{
	unsigned char val;
	int len;

	if(UART2TxFifo.filled_length)
	{	
		len = FifoRead(&UART2TxFifo,&val,1);
		SerialSendByte(UART2,val);
//		printf("txed %d %x\n",(int)UART2TxFifo.filled_length,val);
	}
/*	else
	{
		if(tempflag)
		{
			STOP_TIMER
			PRINT_TIME
			tempflag=0;
		}
	}*/
}
int CheckUART2TxBuffEmpty(void)
{
	if(UART2TxFifo.filled_length)
	{
		return 0;
	}
	
	return 1;
}
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
static void UART2Read(int pipefd,AVRCTRL *tmpctrl)
{
	unsigned char val=0,cmd=0,cmddat=0,rx_checksum=0,calc_checksum=0;
	unsigned char rtcdat[32];
	int i=0;
	static unsigned char rtc_pass=0;
	static int rtc_pass_cnt=0;	
	static int rtc_read_fail_cnt=0;	
	static int rtc_write_fail_cnt=0;

	DEFINE_FILE
	
	/*
	 *Timeout for rtc pass var
	 */	
	if(rtc_pass_cnt>65000)
	{
		rtc_pass_cnt=0;
		rtc_pass=0;
		printf("READ_RTC_CMD timeout\n");
	}

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
				//printf("Header Received\n");			
				if(!rtc_pass)
					FifoRead(&UART2RxFifo,&cmd,1);	
				if((cmd==READ_RTC_ACK)||(rtc_pass))
				{
					rtc_pass=1;
					rtc_pass_cnt++;
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
						MyDisplayBuffer(rtcdat,24);
						if(rx_checksum==calc_checksum)
						{
							printf("READ_RTC_ACK cmd OSF:%x BF:%x BLF:%x\n",rtcdat[14],rtcdat[15],rtcdat[16]);
							printf("UART2 RTC Time = %d:%d:%d %d:%d:%d\n",rtcdat[2],rtcdat[1],rtcdat[0],rtcdat[3],rtcdat[5],rtcdat[6]);
							
							
							if(RTCDataCheck(BCD_2_BIN00(rtcdat[0]),BCD_2_BIN00(rtcdat[1]),BCD_2_BIN00(rtcdat[2]),
									BCD_2_BIN00(rtcdat[3]),BCD_2_BIN00(rtcdat[5])-1,BCD_2_BIN00(rtcdat[6])+2000)==1)
							{
								printf("UART2:RTC Read Data Check OK\n");
								
								tmpctrl->RTCTimeCode[0]=BCD_2_BIN00(rtcdat[0]);
								tmpctrl->RTCTimeCode[1]=BCD_2_BIN00(rtcdat[1]);
								tmpctrl->RTCTimeCode[2]=BCD_2_BIN00(rtcdat[2]);
								tmpctrl->RTCTimeCode[3]=BCD_2_BIN00(rtcdat[3]);
								tmpctrl->RTCTimeCode[4]=BCD_2_BIN00(rtcdat[5]);
								tmpctrl->RTCTimeCode[5]=BCD_2_BIN00(rtcdat[6]);

								tmpctrl->BatteryLowFlag=rtcdat[16];
								tmpctrl->OscillatorStopFlag=rtcdat[14];
									
								tmpctrl->RTCCmd=3;
								ReadTimeCnt=0;
							}
							else
							{
								printf("UART2:RTC Read Data Check Fail\n");
								//tmpctrl->RTCCmd=5;
								// Send RTC Read CMD to UART2 Vineeth
				 				if(ReadTimeCnt<2)
				 				{
					 				UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);	
									tmpctrl->RTCCmd=1;
									ReadTimeCnt++;
								}
								else
								{
									ReadTimeCnt=0;
									tmpctrl->RTCCmd=0;
								}
							}
						}
						else
						{
							printf("UART2:RTC Read ACK Checksum Error rx:%x calc:%x\n",rx_checksum,calc_checksum);
							//tmpctrl->RTCCmd=5;
							// Send RTC Read CMD to UART2 Vineeth
			 				if(ReadTimeCnt<2)
			 				{
				 				UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);	
								tmpctrl->RTCCmd=1;
								ReadTimeCnt++;
							}
							else
							{
								ReadTimeCnt=0;
								tmpctrl->RTCCmd=0;
							}
						}
					}
				}
				else
				{
					//printf("Header Received\n");			
					//FifoRead(&UART2RxFifo,&cmd,1);	
					FifoRead(&UART2RxFifo,&cmddat,1);		
					FifoRead(&UART2RxFifo,&rx_checksum,1);	
					calc_checksum= 0x5A + 0x7E + cmd + cmddat;
	//				printf("cmd=%x data=%x cs=%x calc_checksum=%x\n",cmd,cmddat,rx_checksum,calc_checksum);	
					if(rx_checksum==calc_checksum)
					{
						//printf("Packet Received cmd=%x data=%x\n",cmd,cmddat);	

						if(cmd==FACTORY_DEFAULTS)
						{
							printf("FACTORY_DEFAULTS cmd\n");
							XmegaCtrl.FrmXmegaFactoryDefaults=FACTORY_DEFAULTS_DAT;
							tmpctrl->FactoryDefaultSwitch=1;
						}
						else if(cmd==BOARD_TEMP)
						{
							printf("BOARD_TEMP cmd\n");	
							XmegaCtrl.FrmXmegaBoardTemp=cmddat;
						}
						else if(cmd==GPIO_FAN_STATUS)
						{
							printf("GPIO_FAN_STATUS cmd\n");	
							XmegaCtrl.FrmXmegaGPIOStatus=(cmddat&0x7F);
							printf("GPIOStatus %x\n",XmegaCtrl.FrmXmegaGPIOStatus);
							XmegaCtrl.FrmXmegaFanStatus=(cmddat&0x80)>>7;
							printf("FanStatus %x\n",XmegaCtrl.FrmXmegaFanStatus);	
						}
						else if(cmd==SYS_SHUT_NANO_PERM)
						{
							printf("SYS_SHUT_NANO_PERM cmd\n");	
							XmegaCtrl.FrmXmegaSystemShutdownPerm=SYS_SHUT_NANO_PERM_DAT;
						}
						else if(cmd==PFAIL_NANO_CMD)
						{
							printf("PFAIL_NANO_CMD cmd\n");	
							//XmegaCtrl.FrmXmegaSystemShutdownPerm=SWITCH_PRESS_SHTDWN_DAT;
	//						LOGTOFILE("pshutdwn","gggg",sizeof("gggg"));
							tmpctrl->SwitchPressShutdown=1;
						}
						else if(cmd==SYS_RESTART_NANO)
						{
							printf("SYS_RESTART_NANO cmd\n");
							XmegaCtrl.FrmXmegaSystemRestart=SYS_RESTART_NANO_DAT;
						}
						else if(cmd==SWITCH_PRESS_SHUTDOWN)
						{
							printf("SWITCH_PRESS_SHUTDOWN cmd\n");	
							//XmegaCtrl.FrmXmegaSystemShutdownPerm=SWITCH_PRESS_SHTDWN_DAT;
	//						LOGTOFILE("pshutdwn","gggg",sizeof("gggg"));
							tmpctrl->SwitchPressShutdown=1;
						}					
						else if(cmd==READ_RTC_NAK)
						{
							rtc_read_fail_cnt++;	
							printf("READ_RTC_NAK cmd %d\n",rtc_read_fail_cnt);	
							tmpctrl->RTCCmd=5;
							/*
							 *Send RTC Read CMD to UART2 Vineeth
							 */
							//UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);								
						}					
						else if(cmd==WRITE_RTC_NAK)
						{
							rtc_write_fail_cnt++;	
							printf("WRITE_RTC_NAK cmd %d\n",rtc_write_fail_cnt);	
							/*
							 *Send RTC Write CMD to UART2 Vineeth
							 */
							//UART2Write(pipefd,WRITE_RTC_CMD,NULL_RTC_DAT);								
						}					
						else if(cmd==WRITE_RTC_ACK)
						{
							printf("WRITE_RTC_ACK cmd\n");	
							tmpctrl->RTCCmd=4;
						}					
					}
					else
					{
						printf("Checksum Error\n");	
					}	
					
				}
			}
		}				
	}
}

void UART2Process(int pipefd,AVRCTRL *tmpctrl)
{
	FILE *fptr;
	//NanoToXmegaTest();
	UART2Transmit();
	UART2Receive();
	UART2Read(pipefd,tmpctrl);
	
	if((tmpctrl->OverTempShutdown==0)&&(XmegaCtrl.ToXmegaNanoCoreTemp>78))
	{
		printf("Core Temperature too high %d\n",XmegaCtrl.ToXmegaNanoCoreTemp);
		
		loganevent("AVR_Uart", "OverTempShutdownt.!");
		sync();	
		UART2Write(pipefd,SHUT_NANO_PERM_CMD,SHUT_NANO_PERM_DAT);	
		tmpctrl->OverTempShutdown=1;
			
	}	
	if((tmpctrl->OverTempShutdown==1))
	{
		if(CheckUART2TxBuffEmpty())
		{
			printf("Maheen:Shutdown Process Entered5\n");
			tmpctrl->SwitchPressShutdown=5;	
			sync();
			fptr = popen("shutdown now", "r");// will execute the commands in string..
			sleep(1);	
			pclose(fptr);// close will wait for the process to terminate and return..
			while(1)
			{
				sleep(1);	
			}
		}
	}		
}

void NanoReset(int pipefd)
{
	printf("RESTART_NANO_CMD cmd \n");
	UART2Write(pipefd,RESTART_NANO_CMD,RESTART_NANO_DAT);
}

void NanoFwUpgrade(int pipefd)
{
	printf("FW_UPGRADE_NANO_CMD cmd \n");
	UART2Write(pipefd,FW_UPGRADE_NANO_CMD,0xFF);
}

void NanoToXmegaTest()
{
}
 
/*
 *Read Time
 */												
/*void ReadTime(int pipefd)
{
	struct timeval tval;
	struct tm *ltime, stime;
	struct tm		brokentime;

	gettimeofday(&tval,NULL);
	localtime_r(&tval.tv_sec,&brokentime);
	ptimecode[0] = (char)brokentime.tm_sec;//seconds
	ptimecode[1] = (char)brokentime.tm_min;//minutes
	ptimecode[2] = (char)brokentime.tm_hour;//hour
	ptimecode[3] = (char)brokentime.tm_mday;//date
	ptimecode[4] = (char)brokentime.tm_mon+1;//month
	ptimecode[5] = (char)brokentime.tm_year-100;//year
	
	printf("Time = %d:%d:%d %d:%d:%d\n",ptimecode[2],ptimecode[1],ptimecode[0],ptimecode[3],ptimecode[4],ptimecode[5]);
}
*/
static int RTCDataCheck(int Second,int Minute,int Hour,int Date,int Month,int Year)
{
	
	
	if((Date<1)||(Second<0)||(Minute<0)||(Hour<0)||(Month<0)||(Year<0))
	{
	    	printf("2.RTC returned invalid negative data \n");  
	    	printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
		return -1;
	}  
	else
	{
		if((Second>59)||(Minute>59)||(Hour>23)||(Month>11))
		{
		    	printf("3.RTC returned invalid negative data \n");  
	    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
			return -1;
		}
		else
		{      
			if(((Month==3)||(Month==5)||(Month==8)||(Month==10))&&(Date>30))
			{
			    	printf("4.RTC returned invalid negative data \n");  
		    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
				return -1;
			}
			else
			{
				if(((Month==0)||(Month==2)||(Month==4)||(Month==6)||(Month==7)||(Month==9)||(Month==11))&&(Date>31))
				{
				    	printf("5.RTC returned invalid negative data \n");  
			    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
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
								return -1;
							}
						}
						else
						{
							if((Month==1)&&(Date>28))
							{
							    	printf("8.RTC returned invalid negative data \n");  
						    		printf("%d:%d:%d %d-%d-%d\n",Hour,Minute,Second,Date,Month,Year);  
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

 

