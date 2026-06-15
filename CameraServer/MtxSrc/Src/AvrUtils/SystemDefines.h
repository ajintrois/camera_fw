/********************************************************************************************/
/*		Project		:	Nano Cam					    */
/*		Filename	:	SystemDefines.h					    */
/*		Functionality	:	System Header File				    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#include <sys/types.h>

/********************************************************************************************/
/*                          Macro	                                                    */
/********************************************************************************************/

#define GPIO0	0x00
#define GPIO1	0x02
#define GPIO2	0x04
#define GPIO3	0x08
#define GPIO4	0x10
#define GPIO5	0x20
#define GPIO6	0x40
#define GPIO7	0x80

#define INPUT	1
#define OUTPUT	0

//Nano to  Xmega
#define SYSTEM_STATUS_CMD	0x11
#define KEEPALIVE		0x22
#define NANO_CORE_TEMP		0x81
#define SHUT_NANO_PERM_CMD	0xDC
#define RESTART_NANO_CMD	0xCD
#define FW_UPGRADE_NANO_CMD	0xCE

#define SYSTEM_STATUS_WAIT	0x00
#define SYSTEM_STATUS_INIT	0x01
#define SYSTEM_STATUS_READY	0x02
#define SYSTEM_STATUS_CONFIG	0x03
#define SYSTEM_STATUS_RUNNING	0x04
#define SYSTEM_STATUS_APP_EXIT	0x05
#define KEEPALIVE_DAT		0x33
#define SHUT_NANO_PERM_DAT	0xAD
#define RESTART_NANO_DAT	0x45

//Sudheesh Init
#define NANO_INIT		0x66

//RTC Commands
#define READ_RTC_CMD	0x60
#define WRITE_RTC_CMD	0x61
#define READ_RTC_ACK	0x62
#define WRITE_RTC_ACK	0x63
#define READ_RTC_NAK	0x64
#define WRITE_RTC_NAK	0x65
#define NULL_RTC_DAT	0x00

//Xmega To Nano
#define FACTORY_DEFAULTS	0x33
#define BOARD_TEMP		0x44
#define GPIO_FAN_STATUS		0xAA
#define SYS_SHUT_NANO_PERM	0xBE
#define SYS_RESTART_NANO	0xEB
#define SWITCH_PRESS_SHUTDOWN	0xEF
#define PFAIL_NANO_CMD		0x69

#define FACTORY_DEFAULTS_DAT	0x8F
#define SYS_SHUT_NANO_PERM_DAT	0x72
#define SYS_RESTART_NANO_DAT	0x91
#define SWITCH_PRESS_SHTDWN_DAT	0x92
#define PFAIL_NANO_DAT		0x93

typedef struct xmega_interface
{
	unsigned char FrmXmegaFactoryDefaults;
	unsigned char FrmXmegaBoardTemp;
	unsigned char FrmXmegaGPIOStatus;
	unsigned char FrmXmegaFanStatus;
	unsigned char FrmXmegaSystemRestart;
	unsigned char FrmXmegaSystemShutdownPerm;
	unsigned char FrmXmegaSystemPwrCycle;
	unsigned char FrmXmegaOverTempPwrDwn;

	unsigned char ToXmegaSystemStatus;
	unsigned char ToXmegaKeepalive;
	unsigned char ToXmegaNanoCoreTemp;
	unsigned char ToXmegaPwrCycleNano;
	unsigned char ToXmegaShutdownNanoPerm;
	unsigned char ToXmegaRestart;
	

}XMEGA_INTERFACE;

#define UART_FIFO_SIZE		(128*1024)
#define UART_TX_BUF_SIZE	2*1024
#define UART_RX_BUFF_SIZE	2*1024


#define	INIT_TIMER	struct timeval timer_start, timer_end;
#define	EXT_INIT_TIMER	extern struct timeval timer_start, timer_end;
#define START_TIMER	gettimeofday(&timer_start, NULL);
#define STOP_TIMER	gettimeofday(&timer_end, NULL);
#define	PRINT_TIME	double timer_spent = timer_end.tv_sec - timer_start.tv_sec + (timer_end.tv_usec - timer_start.tv_usec) / 1000000.0;\
			printf("Time spent: %.6f\n", timer_spent);	

/*
 * File Operation Utilities
 */

#define DEFINE_FILE				FILE *testfptr;

#define LOGTOFILE(filename,buff,size)		testfptr=fopen(filename,"w");	\
						if(testfptr != NULL)		\
						{				\
							(void)fwrite(buff,1,size,testfptr);	\
							(void)fflush(testfptr);            	\
							(void)fsync(fileno(testfptr));	\
							(void)fclose(testfptr);		\
						}

#define APPENDTOFILE(filename,buff,size)	testfptr=fopen(filename,"a");	\
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	
						
#define BCD_2_BIN00(hex)	(((hex) >> 4) * 10 + ((hex) & 0xf))
#define BIN_2_BCD00(hex)	(((((hex) % 100) / 10) << 4) + (((hex) % 100) % 10))
						
/********************************************************************************************/
/*                         System Defines		                                    */
/********************************************************************************************/
typedef struct CircFifo
{
	int fifo_depth;	// fifo depth (constant)
	int reserved0;	// to pad
	unsigned char *fifo;		// address of the fifo (constant)
	int reserved1;	// to pad	
	int readptr;
	int writeptr;
	int filled_length;
}CIRCULAR_FIFO;

typedef struct avrutilstruct
{
	unsigned char Reset;
	unsigned char FactoryDefaultSwitch;
	unsigned char NanoFwUpgrade;
	unsigned char SwitchPressShutdown;
	unsigned char OverTempShutdown;
	
	/*
	 *UART2 RTC Vineeth
	 */
	volatile unsigned char RTCTimeCode[6];
	unsigned char RTCCmd;
	unsigned char BatteryLowFlag;
	unsigned char OscillatorStopFlag;
	unsigned char InProgress;
		
	unsigned char reserved[50];
}
AVRCTRL;

/********************************************************************************************/
/*                          Extern Function Prototypes                                      */
/********************************************************************************************/

extern void FifoInit(CIRCULAR_FIFO *fifo, int depth, unsigned char *data);
extern void FifoFlush(CIRCULAR_FIFO *fifo);
extern int FifoRewind(CIRCULAR_FIFO *fifo, int length);
extern int FifoWrite(CIRCULAR_FIFO *fifo, unsigned char *data, int len);
extern int FifoRead(CIRCULAR_FIFO *fifo, unsigned char * data, int len);

extern void ConfigureUART2(void);
extern void UART2Process(int pipefd,AVRCTRL *tmpctrl);
extern void UART2Write(int pipefd,unsigned char cmd,unsigned char dat);

extern void ConfigureTimers(void);
extern void TimerProcess(int pipefd);
extern void ConfigureI2C(void);
extern int I2CWrite(unsigned char device, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len);
extern int I2CRead(unsigned char device, unsigned int slave_addr, unsigned int offset,unsigned char *buf, unsigned char len);
extern void GPIOExpanderSetPin(unsigned char gpiopin);
extern void GPIOExpanderClearPin(unsigned char gpiopin);
extern int GPIOExpanderReadPin(unsigned char gpiopin);
extern void GPIOExpanderSetPinDir(unsigned char gpiopin,int direction);
extern void DisplayBuffer (unsigned char * source, unsigned short buffer_length);

//extern void ReadTime(int pipefd);
