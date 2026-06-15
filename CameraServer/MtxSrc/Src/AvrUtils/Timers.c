/********************************************************************************************/
/*		Project		:	NANO CAM					    */
/*		Filename	:	ProcessTimer.c					    */
/*		Functionality	:	Timing Routines					    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h> 
#include <linux/watchdog.h>

#include "SystemDefines.h"
/********************************************************************************************/
/*                          MACROS		                                            */
/********************************************************************************************/
#define ONESEC_TIME_CNT	100

#define CLOCKID 	CLOCK_REALTIME
#define SIG 		SIGRTMIN

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
			} while (0)

#define DEBUG_PRINT printf			
						

/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
extern int TestTimer;

extern struct cirfifo		*ProcessDebugFifo;
extern char 			DebugStr[1024];
extern int 			DebugStrSize;
extern int CutKeepAlive;
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
static timer_t timerid;

int KeepAliveTimer;
int CoreTempTimer;
int ResetTimer=-1;
int ShutdownTimer;
int ReadRTCTimer=3600;
/********************************************************************************************/
/*                          Function Prototypes	                                            */
/********************************************************************************************/
static void ProcessTimerHandler(int sig, siginfo_t *si, void *uc);
static void ConfigureProcessTimer(void);
/********************************************************************************************/
/*                          Function Defines	                                            */
/********************************************************************************************/
static void ProcessTimerHandler(int sig, siginfo_t *si, void *uc)
{
	// To take care of timers required in some processes..
	static unsigned int  SecCnt=0;
	int i=0,j=0;
	if(SecCnt == 0)
	{
		if(ShutdownTimer)
		{
			ShutdownTimer--;
		}
		if(KeepAliveTimer)
		{
			KeepAliveTimer--;
		}
		if(CoreTempTimer)
		{
			CoreTempTimer--;
		}
		if(TestTimer)
		{
			TestTimer--;
		}
		if(ResetTimer)
		{
			ResetTimer--;
		}
		if(ReadRTCTimer)
		{
			ReadRTCTimer--;
		}

		SecCnt = ONESEC_TIME_CNT;
	}
	else
	{
		SecCnt--;
	}    
}

static void ConfigureProcessTimer(void)
{

	struct sigevent sev;
	struct itimerspec its;
	long long freq_nanosecs;
	//sigset_t mask;
	struct sigaction sa;


	/* Establish handler for timer signal */

	printf("TIMER:Establishing handler for signal %d\n", SIG);
	

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = ProcessTimerHandler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIG, &sa, NULL) == -1)
	{
		errExit("sigaction");
	}

	/* Create the timer */

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIG;
	sev.sigev_value.sival_ptr = &timerid;
	if (timer_create(CLOCKID, &sev, &timerid) == -1)
	{
		errExit("timer_create");
	}

	printf("TIMER:ID is 0x%lx\n", (long) timerid);

	/* Start the timer */

	freq_nanosecs = 10000000;
	its.it_value.tv_sec = freq_nanosecs / 1000000000;
	its.it_value.tv_nsec = freq_nanosecs % 1000000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) == -1)
	{
		errExit("timer_settime");
	}
	
	
}

void ProcessTimerUnInit(void)
{
	timer_delete(timerid);
}

void ConfigureTimers(void)
{
	ConfigureProcessTimer();
	KeepAliveTimer=5;
}

void TimerProcess(int pipefd)
{
	if(!KeepAliveTimer)
	{
		if(CutKeepAlive==0)
		{
			printf("M$Keepalive..W\n");
			KeepAliveTimer=5;
			//DEBUG_PRINT("KEEPALIVE cmd \n");
			UART2Write(pipefd,KEEPALIVE,KEEPALIVE_DAT);
			//UART2Write(SYSTEM_STATUS_CMD,SYSTEM_STATUS_INIT);
		}
	}
	if(!CoreTempTimer)
	{
		UART2Write(pipefd,NANO_CORE_TEMP,0xFF);
		CoreTempTimer=60;
	}
	if(!ReadRTCTimer)
	{
		/*
		 *Send RTC Read CMD to UART2 Vineeth
		 */
		UART2Write(pipefd,READ_RTC_CMD,NULL_RTC_DAT);	
		ReadRTCTimer=3600;
	}
	
}
