/********************************************************************************************/
/*		Project			:	Nano Cam				    */
/*		Filename		:	GPIO.c          		            */
/*		Functionality		:	GPIO			         	    */
/*		Author			:	Maheen Rasheed				    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>


#include "SystemDefines.h"
#include "I2C.h"
/********************************************************************************************/
/*                          Defines	                                                    */
/********************************************************************************************/
				
/********************************************************************************************/
/*                          Extern Variable	                         	            *
/********************************************************************************************/

extern struct cirfifo		*ProcessDebugFifo;
extern char 			DebugStr[1024];
extern int 			DebugStrSize;

extern int ResetTimer;
extern int ShutdownTimer;

unsigned char RTCWriteBuffer[16];
/********************************************************************************************/
/*                          Global Variable	                                       	    */
/********************************************************************************************/ 
int TimeUpdateInProgress;

int RunOnce=-1;
int CutKeepAlive;
/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
extern void NanoReset(int pipefd);
extern void NanoFwUpgrade(int pipefd);
extern int CheckUART2TxBuffEmpty(void);
extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);
extern int safe_atoi(const char *nptr, int *value);
extern char* safe_strncpy( char* dest, const char* src, size_t count);
extern size_t safe_strlen(const char *str, size_t max_len);

/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
/*int main(int argc, char **argv) 
{
	unsigned char val;
	MaheenInit();
#if 0
	val=0x40;printf("%x\n",val);
	I2CWrite(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);	
	val=0x00;printf("%x\n",val);
	I2CRead(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);
	printf("%x\n",val);
#endif	
	while(1)
	{
		MaheenProcess();
	}

    return 0; 
}*/

void MaheenInit(int pipefd)
{
	ConfigureTimers();
	//ConfigureI2C();
	ConfigureUART2();
}
void MaheenProcess(int pipefd,AVRCTRL *tmpctrl)
{
	FILE *fptr;
	
	if(tmpctrl->SwitchPressShutdown==3)
	{
		printf("Maheen:Shutdown Process Entered\n");


		sync();

		UART2Write(pipefd,SHUT_NANO_PERM_CMD,SHUT_NANO_PERM_DAT);						
		

		tmpctrl->SwitchPressShutdown=4;	
	}
	if((ShutdownTimer==0)&&(tmpctrl->SwitchPressShutdown==4))
	{
		if(CheckUART2TxBuffEmpty())
		{
			printf("Maheen:Shutdown Process Entered5\n");
			tmpctrl->SwitchPressShutdown=5;	
			fptr = popen("shutdown now", "r");// will execute the commands in string..
			sleep(1);	
			pclose(fptr);// close will wait for the process to terminate and return..
			while(1)
			{
				sleep(1);	
			}
		}
	}
	
	if(tmpctrl->Reset==1)
	{
		tmpctrl->Reset=0;
		ResetTimer=5;
	}
	if(tmpctrl->NanoFwUpgrade==1)
	{
		if(RunOnce==-1)
		{
			NanoFwUpgrade(pipefd);
			RunOnce=0;
		}
	}
	if(ResetTimer==0)
	{
  		
  		NanoReset(pipefd);
  		CutKeepAlive=1;
  		ResetTimer=-1;
	}


		/*
		 *Set RTC Time received.
		 *
		 */
		if(tmpctrl->RTCCmd==2)
		{
			printf("Maheen:SetRTC Received.. \n");
			RTCWriteBuffer[0]=BIN_2_BCD00(tmpctrl->RTCTimeCode[0]);					
			RTCWriteBuffer[1]=BIN_2_BCD00(tmpctrl->RTCTimeCode[1]);
			RTCWriteBuffer[2]=BIN_2_BCD00(tmpctrl->RTCTimeCode[2]);					
			RTCWriteBuffer[3]=BIN_2_BCD00(tmpctrl->RTCTimeCode[3]);
			RTCWriteBuffer[4]=BIN_2_BCD00(0);					
			RTCWriteBuffer[5]=BIN_2_BCD00(tmpctrl->RTCTimeCode[4]+1);
			RTCWriteBuffer[6]=BIN_2_BCD00(tmpctrl->RTCTimeCode[5]);					
			RTCWriteBuffer[7]=BIN_2_BCD00(tmpctrl->RTCTimeCode[0]);
			RTCWriteBuffer[8]=BIN_2_BCD00(tmpctrl->RTCTimeCode[1]);					
			RTCWriteBuffer[9]=BIN_2_BCD00(tmpctrl->RTCTimeCode[2]);
			RTCWriteBuffer[10]=BIN_2_BCD00(tmpctrl->RTCTimeCode[3]);					
			RTCWriteBuffer[11]=BIN_2_BCD00(0);
			RTCWriteBuffer[12]=BIN_2_BCD00(tmpctrl->RTCTimeCode[4]+1);					
			RTCWriteBuffer[13]=BIN_2_BCD00(tmpctrl->RTCTimeCode[5]);
			
			/*
			 *Send RTC Write CMD to UART2 Vineeth
			 */
			UART2Write(pipefd,WRITE_RTC_CMD,NULL_RTC_DAT);	

			tmpctrl->RTCCmd=0;
		}
		if(tmpctrl->InProgress)
		{
			TimeUpdateInProgress=1;
		}
		else
		{
			TimeUpdateInProgress=0;
		}
		/*
		 *Read RTC Time Again due to failed previous attempt. Try 3 times.
		 *
		 */
		/*if(tmpctrl->RTCCmd==5)		
		{
			ReadTimeCnt++;
			if(ReadTimeCnt<=2)
			{
				
				// *Send RTC Read CMD to UART2 Vineeth
				 
				UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);	
				tmpctrl->RTCCmd=1;
			}
			else
			{
				tmpctrl->RTCCmd=0;
				ReadTimeCnt=0;
			}			
		}
		else		
		{
			ReadTimeCnt=0;
		}*/

	TimerProcess(pipefd);
	UART2Process(pipefd,tmpctrl);		
}

void GPIOExpanderSetPin(unsigned char gpiopin)
{
	unsigned char val;	
	I2CRead(PCA9557,I2C_ADDR_PCA9557,0x01,&val,1);	
	val|=gpiopin;
	I2CWrite(PCA9557,I2C_ADDR_PCA9557,0x01,&val,1);	
}
void GPIOExpanderClearPin(unsigned char gpiopin)
{
	unsigned char val;	
	I2CRead(PCA9557,I2C_ADDR_PCA9557,0x01,&val,1);	
	val&=~gpiopin;
	I2CWrite(PCA9557,I2C_ADDR_PCA9557,0x01,&val,1);	
}
int GPIOExpanderReadPin(unsigned char gpiopin)
{
	unsigned char val;	
	I2CRead(PCA9557,I2C_ADDR_PCA9557,0x00,&val,1);	
	if(val&gpiopin)
	{
		return 1;
	}	
	return 0;
}
void GPIOExpanderSetPinDir(unsigned char gpiopin,int direction)
{
	unsigned char val;

	if(direction==INPUT)	
	{
		I2CRead(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);	
		val|=gpiopin;
		I2CWrite(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);	
	}
	else
	{
		I2CRead(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);	
		val&=~gpiopin;
		I2CWrite(PCA9557,I2C_ADDR_PCA9557,0x03,&val,1);	
	}
}
void MyDisplayBuffer (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j,k;
	char c;

	k=buffer_length%8;
	printf("******Debug Print**********\n");
	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		for(j = 0; j < 8; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
		}
		printf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
	}
	
	if(k)
	{
		printf("%04d :",i*8);
		for(j = 0; j < k; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
		}
		for(j = 0; j < (8-k); j++)
		{
			printf("   ");
		}
		printf("\t");
		for(j = 0; j < k; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");    
	}
	printf("-----------Debug Print------------\n");
}













