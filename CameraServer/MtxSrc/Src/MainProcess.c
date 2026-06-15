/********************************************************************************************/
/*		Project		:	TK1 Image Analytics				    */
/*		Filename	:	MainProcess.c					    */
/*		Functionality	:	Main loop Processing and Debug Server		    */
/*		Author		:	Manoj Kumar D					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h> 
#include <pthread.h>
#include <linux/unistd.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "defines.h"
#include "common_shm.h"
/********************************************************************************************/
/*                          Defines                                                         */
/********************************************************************************************/
#define MAXDEBUG_TX_DATA_FIFO_LENGTH	(64*1024)
#define DEBUG_PORTNO			5555
#define ONE_SEC_TIMER			250

#define NO_OF_CHILDS		(2+1+NO_OF_SERVER_SOCKETS+1+1+1)
//---------------------------------------------------------Maheen-------------------------------------------------------
#define BCD_2_BIN00(hex)	((hex >> 4) * 10 + (hex & 0xf))
#define BIN_2_BCD00(hex)	((((hex % 100) / 10) << 4) + ((hex % 100) % 10))
//---------------------------------------------------------Maheen-------------------------------------------------------
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
/*                          Global Vars                                                     */
/********************************************************************************************/
struct sigaction 	sa;
struct itimerval 	timer;
unsigned short		keepalive_timer[MAX_WDT_COUNT] = {200}, keepalive_timer_enabled[MAX_WDT_COUNT] = {0}, keepalive_timer_reload[MAX_WDT_COUNT] = {0}, aux_keep_alive_timer[MAX_WDT_COUNT] = {0};
int			rtcMtimer,  socktimer[NO_OF_SERVER_SOCKETS] = {0};
int			wdt_rd_pipe = 0, wdt_wr_pipe = 0, lux, prevLux, prev_ampm_flag, ampm_flag;
unsigned int		process_timer[NO_OF_PROCESS] = {0};

struct cirfifo		DebugTxFifo[NO_OF_DEBUG_SERVER_SOCKETS];
struct cirfifo		*ProcessDebugFifo;
char 			DebugStr[1024];
int 			DebugStrSize;
unsigned char		*rlvdzonemap;
char 			firmware[32] = {" -no/fw- "};
int			processIDVar = 0, displaymsg_cnt = 500;
//---------------------------------------------------------Maheen-------------------------------------------------------
extern int battery_low;
extern int power_loss;
AVRCTRL			AvrCtrlVar;
//int 			EEoffset=0;
//---------------------------------------------------------Maheen-------------------------------------------------------
/********************************************************************************************/
/*                          Extern Vars                                                     */
/********************************************************************************************/

/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
int  DebugServerTx(struct tst *tst);
void DebugServerRx(struct tst *tst);
void DebugThreadedSocket(void * arg, int* pipes);

void display_buffer (unsigned char * source, unsigned short buffer_length);

void kill_timer();
void init_timer();

void CaptureProcess(int pipefd);
void GpuConvertProcess(int pipefd);
void CompressProcess(int pipefd);
void PcStreamProcess(int* pipes);

void close_uart4(int serial_handle);
int  init_uart4();

//-----------------------------------------------------------------------------------------------------------------
extern void CaptureMainProcess(int pipefd);
extern void PcStreamProcessFunction(SHARED_RESOURCES *shared_data, COMPRESS_PC_SHARED_RESOURCES *compressPcShr, int *pipes);
extern int  ApplyLightSettings(void * data, int pipefd);
extern int  InitLuxADCI2c(int * pi2c_dev_handle);
extern void CloseLuxADCI2c(int * pi2c_dev_handle);
extern int  GetLuxValue(int i2c_dev_handle, int* lux);

extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);
extern int safe_atoi(const char *nptr, int *value);
extern char* safe_strncpy( char* dest, const char* src, size_t count);
extern size_t safe_strlen(const char *str, size_t max_len);

extern int  initGpio();
extern int  closeGpio();
extern void loganevent(const char *processname, const char *eventstr);
//---------------------------------------------------------Maheen-------------------------------------------------------
extern void MaheenInit(int pipefd);
extern void NanoReset(int pipefd);
//extern void _EraseFRAM(int pipefd,int eeprom_offset);
extern void MaheenProcess(int pipefd,AVRCTRL *tmpctrl);
//---------------------------------------------------------Maheen-------------------------------------------------------
/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/
int main(int argc, char* argv[])
{
	int 			prev1Hz = 0, cur1Hz = 0, hostKeepAliveAtimer = 0;
	int 			i, current_process, shm_handle, LUXi2cHandle;
	static pthread_t 	sock_thread;
	unsigned char		*DebugTxFifo_buffer[NO_OF_DEBUG_SERVER_SOCKETS];
	void 			*ShMemory;
	SHARED_RESOURCES 	*shared_data;
	pid_t			pid, chpid[5] = {0};
	FILE 			*fptr;
	int			Parent = 0, wdtpipevar[6][2], pipevar[2], pipefd, debugpipe[NO_OF_CHILDS], debugwrpipe[NO_OF_CHILDS], tmppipes[4];
	char 			c;

	fptr = fopen("../version.txt","rb");
	if(fptr != NULL)
	{
		fread(firmware, 32, 1, fptr);
		fclose(fptr);
	}
	for(i = 0; i < 6; i++)
	{
		while(pipe(wdtpipevar[i]) < 0)
		{
			usleep(100000);
		}
	}
	while(pipe(pipevar) < 0)
	{
		usleep(100000);
	}
	debugpipe[0] = pipevar[0];
	debugwrpipe[0] = pipevar[1];
	pid = fork();
	if(pid == 0)
	{
		// child0
		// for capture
		wdt_rd_pipe = wdtpipevar[0][PIPE_RD_END]; //rd end of parent to child0
		wdt_wr_pipe = wdtpipevar[1][PIPE_WR_END]; //write end for child0 to child1

		processIDVar = 1;
		pid = getpid();
		usleep(1400000);
		keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
		keepalive_timer_enabled[0] = 1;
  		//nice(-10);
		//initGpio();
		init_timer();
		CaptureProcess(pipevar[1]);
		kill_timer();
		//closeGpio();
	}
	else
	{
		// parent
		while(pipe(pipevar) < 0)
		{
			usleep(100000);
		}
		debugpipe[1] = pipevar[0];
		debugwrpipe[1] = pipevar[1];
		chpid[0] = pid;
		pid = fork();
		if(pid == 0)
		{
			//child
			// for compress
			wdt_rd_pipe = wdtpipevar[1][PIPE_RD_END]; //rd end of parent to child0
			wdt_wr_pipe = wdtpipevar[2][PIPE_WR_END]; //write end for child0 to child1

			processIDVar = 2;
			pid = getpid();
			usleep(1300000);
			keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
			keepalive_timer_enabled[0] = 1;
			init_timer();
			CompressProcess(pipevar[1]);
			kill_timer();
		}
		else
		{
			// parent
			for(i = 2; i < (2+NO_OF_SERVER_SOCKETS+1); i++)
			{
				while(pipe(pipevar) < 0)
				{
					usleep(100000);
				}
				debugpipe[i] = pipevar[0];
				debugwrpipe[i] = pipevar[1];
			}
			chpid[1] = pid;
			pid = fork();
			if(pid == 0)
			{
				// child1
				processIDVar = 3;
				wdt_rd_pipe = wdtpipevar[2][PIPE_RD_END]; //rd end of parent to child0
				wdt_wr_pipe = wdtpipevar[3][PIPE_WR_END]; //write end for child0 to child1
				pid = getpid();
				usleep(500000);
				keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
				keepalive_timer_enabled[0] = 1;
				init_timer();
				PcStreamProcess(&debugwrpipe[2]);
				kill_timer();
			}
			else
			{
				// parent
				while(pipe(pipevar) < 0)
				{
					usleep(100000);
				}
				debugpipe[(2+NO_OF_SERVER_SOCKETS+1)] = pipevar[0];// read end..
				debugwrpipe[(2+NO_OF_SERVER_SOCKETS+1)] = pipevar[1];// write end
				chpid[2] = pid;
				pid = fork();
				if(pid == 0)
				{
					// child
					processIDVar = 4;
					wdt_rd_pipe = wdtpipevar[3][PIPE_RD_END]; //rd end of parent to child0
					wdt_wr_pipe = wdtpipevar[4][PIPE_WR_END]; //write end for child0 to child1
					pid = getpid();
					usleep(200000);
					keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
					keepalive_timer_enabled[0] = 1;
					init_timer();
					GpuConvertProcess(pipevar[1]);
					kill_timer();
				}
				else
				{
					// parent
					while(pipe(pipevar) < 0)
					{
						usleep(100000);
					}
					debugpipe[(2+NO_OF_SERVER_SOCKETS+1+1)] = pipevar[0];// read end..
					debugwrpipe[(2+NO_OF_SERVER_SOCKETS+1+1)] = pipevar[1];// write end
					chpid[3] = pid;
					pid = fork();
					if(pid == 0)
					{
						// child
						loganevent("main", firmware);
						loganevent("main", "application Start...!");
						processIDVar = 5;
						wdt_rd_pipe = wdtpipevar[4][PIPE_RD_END]; //rd end of parent to child0
						wdt_wr_pipe = wdtpipevar[5][PIPE_WR_END]; //write end for child0 to child1
						pipefd = pipevar[1];
						pid = getpid();
						process_timer[0] = MAX_PROCESS_LOOP_TIME;// timeout for app exit...
						shm_handle = shm_open(SHM_NAME, O_RDWR|O_CREAT, 0660);
						ftruncate(shm_handle, MAKESIZE4MASK(SHM_SIZE));
		//					DEBUG_PRINT("truncated shm with name %s of size %ld bytes to map size=%X and map mask=%X\n", SHM_NAME, SHM_SIZE, MAKESIZE4MASK(SHM_SIZE), MAKESIZE4MASK(SHM_SIZE)-1);
						ShMemory = mmap(NULL, MAKESIZE4MASK(SHM_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handle, 0);//flag | MAP_32BIT
						shared_data = (SHARED_RESOURCES*)ShMemory;

						// now init all shared resources..
						shared_data->SyncStatus		= 0;
						shared_data->validID 		= 0;
						shared_data->FactoryDefaults 	= 0;
						shared_data->IPDefaults 	= 0;
						shared_data->app_exit		= 0;
						shared_data->settime_flag	= 0;
						shared_data->time_error		= 0;
						shared_data->MainProcStatus	= PROCESSSTATUS_IDLE;
						shared_data->CaptureStatus	= PROCESSSTATUS_IDLE;
						shared_data->ConvertProcStatus	= PROCESSSTATUS_IDLE;
						shared_data->CompressStatus	= PROCESSSTATUS_IDLE;
						shared_data->PcStreamStatus	= PROCESSSTATUS_IDLE;
						shared_data->ImgCapRequired 	= 0;
						shared_data->captureTrigger1 	= 0;
						shared_data->validID 		= 0x46392715;
						shared_data->MainProcStatus	= PROCESSSTATUS_INIT;
						sync();
						keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
						keepalive_timer_enabled[0] = 1;
						keepalive_timer[1] = 2;//
						keepalive_timer_reload[1] = 2;//
						keepalive_timer_enabled[1] = 1;
						init_timer();
						InitLuxADCI2c(&LUXi2cHandle);
//---------------------------------------------------------Maheen-------------------------------------------------------						
						shared_data->SystemReset = 0;
						MaheenInit(1);
//---------------------------------------------------------Maheen-------------------------------------------------------						
						c = 'A';
						write(wdt_wr_pipe, &c, 1);
						initGpio();
						// process loop...
						shared_data->MainProcStatus	= PROCESSSTATUS_RUN;
						lux = 0;
						GetLuxValue(LUXi2cHandle, &lux);
						shared_data->lux = lux;
						shared_data->applyCurrentLightSettings = 1;
						memset((unsigned char *)shared_data->radar_all_veh_data, 0, 2048);
						while(1)
						{
							usleep(100000);
							hostKeepAliveAtimer++;
							//if(hostKeepAliveAtimer >= 15)// 5 second..
							if(keepalive_timer[1] == 0)
							{
								keepalive_timer[1] = 2;//
								hostKeepAliveAtimer = 0;
								GetLuxValue(LUXi2cHandle, &lux);
								shared_data->lux = lux;
								i = ApplyLightSettings(shared_data, 1);
								//DEBUG_PRINT("main loop read LUX %d\n", lux);
								if(shared_data->ImgCapRequired == 1)
								{
									if(shared_data->captureTrigger1)
										shared_data->captureTrigger1 	= 0;
									else
										shared_data->app_exit = 1;// timout from capture...
								}
							}
							if(shared_data->app_exit == 1)
							{
								break;
							}
							if(keepalive_timer[0] == 0)
							{
								loganevent("main loop", "application keepalive_timer expired...!");
								shared_data->app_exit = 1;
								break;
							}
							else
							{
								if(keepalive_timer_reload[0] == 0)
								{
									if(read(wdt_rd_pipe, &c, 1) > 0)
									{
										// got data reload wdt
										//printf("got data in main process\n");
										keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
										write(wdt_wr_pipe, &c, 1);
		/*									if(displaymsg_cnt++ >= 50)
										{
											displaymsg_cnt = 0;
											printf("wdt in main process %d\n", processIDVar);
										}*/
									}
								}
							}
					//---------------------------------------------------------Maheen-------------------------------------------------------							
							if(shared_data->SystemReset)
							{
								shared_data->SystemReset --;
								if(shared_data->SystemReset == 0)
								{
									loganevent("main loop", "shared_data->SystemReset req...!");
									AvrCtrlVar.Reset=1;
									shared_data->SystemReset = 0;
								}
							}
							/*if(shared_data->SystemReset == 25)
							{
								AvrCtrlVar.SwitchPressShutdown=1;
								shared_data->SystemReset = 0;
							}*/
							MaheenProcess(1,&AvrCtrlVar);
							if(AvrCtrlVar.FactoryDefaultSwitch==1)
							{							
								
								loganevent("main loop", "AvrCtrlVar.FactoryDefaultSwitch req...!");
								AvrCtrlVar.FactoryDefaultSwitch=0;
								shared_data->FactoryDefaults = 1;
																
								/*if(EEoffset <= 0x2000)
								{
									DEBUG_PRINT("Erasing FRAM in progress %X \n",EEoffset);
									_EraseFRAM(pipefd,EEoffset);
									EEoffset+=32;
								}
								else
								{
									printf ("Erase complete\n");	
									DEBUG_PRINT("Erasing complete\n");
									EEoffset=0;
									AvrCtrlVar.FactoryDefaultSwitch=0;
									shared_data->SystemReset = 1;
								}*/
								
							}
							if(shared_data->FwUpgrade == 1)
							{
								if(!AvrCtrlVar.NanoFwUpgrade)
								{
									loganevent("main loop", "AvrCtrlVar.NanoFwUpgrade req...!");
									AvrCtrlVar.NanoFwUpgrade=1;
								}
							}
							
							
							if(AvrCtrlVar.SwitchPressShutdown == 1)
							{
								//shared_data->host_data_from_pc = RECORD_OFF;												
								loganevent("main loop", "AvrCtrlVar.SwitchPressShutdown req...!");
								AvrCtrlVar.SwitchPressShutdown = 2;							
							}
							else
							{
								if(AvrCtrlVar.SwitchPressShutdown == 2)
								{
									AvrCtrlVar.SwitchPressShutdown = 3;																	/*if(shared_data->RecStatus == 0)//Record Switched off
									{
										DEBUG_PRINT("Record OFF Done\n");
										printf("((((****))))Record OFF Done\n");
										AvrCtrlVar.SwitchPressShutdown = 3;								
									}*/
								}
							}
							/*
							 *UART2 RTC Vineeth
							 */
							if((shared_data->RTCCmd == 2)&&(AvrCtrlVar.InProgress==0))//Write to RTC
							{
								if(safe_memcpy((char*)AvrCtrlVar.RTCTimeCode, 6, (char*)shared_data->setrtctimecode,6) <= 0)// size verified..
								{
									printf("time not copied now.\n");
								}
								else
								{
									AvrCtrlVar.RTCCmd=2;
									shared_data->RTCCmd=0;
									AvrCtrlVar.InProgress=1;
								}
							}
							if(AvrCtrlVar.RTCCmd == 4)//RTC Write Ack
							{
								AvrCtrlVar.RTCCmd=0;
								shared_data->RTCCmd=4;
								AvrCtrlVar.InProgress=0;
							}
							if((AvrCtrlVar.RTCCmd == 3))//RTC Read Ack
							{
								if(safe_memcpy((char*)shared_data->setrtctimecode, 6, (char*)AvrCtrlVar.RTCTimeCode,6) <= 0)// size verified..
								{
									printf("time not copied now.\n");
								}
								else
								{
									AvrCtrlVar.RTCCmd=0;
									shared_data->RTCCmd=3;
									battery_low=AvrCtrlVar.BatteryLowFlag;
									power_loss=AvrCtrlVar.OscillatorStopFlag;
									AvrCtrlVar.InProgress=0;
								}
							}
							/*if(AvrCtrlVar.RTCCmd == 5)//RTC Read Nak
							{
								printf("Read RTC Nak Received.. \n");
								DEBUG_PRINT("Read RTC Nak Received..\n");		
								AvrCtrlVar.RTCCmd=0;
								shared_data->RTCCmd=1;
							}*/
														
						}
					//---------------------------------------------------------Maheen-------------------------------------------------------						
						shared_data->MainProcStatus	= PROCESSSTATUS_STOP;
						munmap(ShMemory, SHM_SIZE);
						kill_timer();
						CloseLuxADCI2c(&LUXi2cHandle);
						closeGpio();
						loganevent("main loop", "exit...!");
						sleep(2);
						shared_data->MainProcStatus	= PROCESSSTATUS_IDLE;
					}
					else
					{// parent process
						processIDVar = 6;
				  		chpid[4] = pid;
						wdt_rd_pipe = wdtpipevar[5][PIPE_RD_END]; //rd end of parent to child0
						wdt_wr_pipe = wdtpipevar[0][PIPE_WR_END]; //write end for child0 to child1
						pid = getpid();
				  		nice(30);
						keepalive_timer[0] = KEEPALIVE_TIMEOUT_COUNT;//
						keepalive_timer_enabled[0] = 1;
						init_timer();
				  		Parent = 1;
						for(i = 0; i < NO_OF_DEBUG_SERVER_SOCKETS; i++)
						{
							DebugTxFifo_buffer[i] = (unsigned char*)malloc(MAXDEBUG_TX_DATA_FIFO_LENGTH);
							fifo_init(&DebugTxFifo[i], MAXDEBUG_TX_DATA_FIFO_LENGTH, DebugTxFifo_buffer[i]);
						}

						DebugThreadedSocket(&sock_thread, debugpipe);// loop
						for(i = 0; i < NO_OF_DEBUG_SERVER_SOCKETS; i++)
						{
							free(DebugTxFifo_buffer[i]);
						}
						kill_timer();
					}
				}
			}
		}
	}
	if(Parent == 1)
	{
	  kill(chpid[0], SIGKILL);
	  kill(chpid[1], SIGKILL);
	  kill(chpid[2], SIGKILL);
	  kill(chpid[3], SIGKILL);
	  kill(chpid[4], SIGKILL);
	  sleep(1);
	  current_process  = 0x1F;
	  while(1)
	  {
	  	pid = wait(NULL);
	  	if(pid == chpid[0])
	  	{
	  		current_process &= 0x17;
	  	}
	  	else if(pid == chpid[1])
	  	{
	  		current_process &= 0x1B;
	  	}
	  	else if(pid == chpid[2])
	  	{
	  		current_process &= 0x1D;
	  	}
	  	else if(pid == chpid[3])
	  	{
	  		current_process &= 0x1E;
	  	}
	  	else if(pid == chpid[4])
	  	{
	  		current_process &= 0x0F;
	  	}
	  	if(current_process == 0)
		  	break;
	  }
	}
	return 0;
}


// kill >>>>>>>>>>>>>>>>>>>
void kill_timer()
{
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;	/* ... now.... */
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;	/* here...*/
  setitimer (ITIMER_REAL, &timer, NULL);
}


void timer_handler(int signum)
{
	static int  sec_timer, rtemp;

	if(sec_timer == 0)
	{
		//    printf("one second\n");
		sec_timer = ONE_SEC_TIMER - 1;
		for(rtemp = 0; rtemp < MAX_WDT_COUNT; rtemp++)
		{
			if(keepalive_timer_enabled[rtemp])
			{
				if(keepalive_timer[rtemp] > 0)
				{
					keepalive_timer[rtemp] --;
				}
				if(aux_keep_alive_timer[rtemp] > 0)
					aux_keep_alive_timer[rtemp]--;
			}
		}
		if(rtcMtimer)
		{
			rtcMtimer--;
		}
		for(rtemp = 0; rtemp < NO_OF_SERVER_SOCKETS; rtemp++)
		{
			if(socktimer[rtemp])
				socktimer[rtemp]--;
		}
//		printf(" timer 1 sec on process %d timer %d reload %d\n", processIDVar, keepalive_timer[0], keepalive_timer_reload[0]);
	}
	else
	{
		sec_timer--;
		for(rtemp = 0; rtemp < MAX_WDT_COUNT; rtemp++)
		{
			if(keepalive_timer_enabled[rtemp])
			{
				if(keepalive_timer_reload[rtemp] > 0)
				{
					keepalive_timer[rtemp] = keepalive_timer_reload[rtemp];
					keepalive_timer_reload[rtemp] = 0;
				}
			}
		}
	}
}

void init_timer()
{
  memset ((void *)&sa, 0, sizeof (sa));
  sa.sa_handler = &timer_handler;
  sigaction (SIGALRM, &sa, NULL);

  timer.it_value.tv_sec = 0; 		
  timer.it_value.tv_usec = 4000;
  timer.it_interval.tv_sec = 0;		
  timer.it_interval.tv_usec = 4000;

  setitimer (ITIMER_REAL, &timer, NULL);	/* Start a virtual timer. It counts down whenever this process is executing. */
}

/********************************************************************************************/
void GpuConvertProcess(int pipefd)
{
	char c;
	while(1)
	{
		if(keepalive_timer_reload[0] == 0)
		{
			if(read(wdt_rd_pipe, &c, 1) > 0)
			{
				// got data reload wdt
				//printf("-----------got data in GPU process\n");
				keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
				write(wdt_wr_pipe, &c, 1);
				usleep(1000);
			}
		}
		usleep(100000);
	}
}

void CompressProcess(int pipefd)
{
	char c;
	while(1)
	{
		if(keepalive_timer_reload[0] == 0)
		{
			if(read(wdt_rd_pipe, &c, 1) > 0)
			{
				// got data reload wdt
				//printf("-----------got data in COMP process\n");
				keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
				write(wdt_wr_pipe, &c, 1);
				usleep(1000);
			}
		}
		usleep(100000);
	}
}

void CaptureProcess(int pipefd)
{
	char c;
	while(1)
	{
		if(keepalive_timer_reload[0] == 0)
		{
			if(read(wdt_rd_pipe, &c, 1) > 0)
			{
				// got data reload wdt
				//printf("-----------got data in COMP process\n");
				keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
				write(wdt_wr_pipe, &c, 1);
				usleep(1000);
			}
		}
		usleep(100000);
	}
}


void PcStreamProcess(int* pipes)
{
	int 					shm_handle, shm_handlecopc, i, j, pipefd;
	void 					*ShMemory, *ShMemorycopc;
	SHARED_RESOURCES 			*shared_data;
	CONVERT_COMPRESS_SHARED_RESOURCES	*convertCompress;
	COMPRESS_PC_SHARED_RESOURCES		*compressPcShr;
	int					serialport4handle;

	pipefd = pipes[NO_OF_SERVER_SOCKETS];
	serialport4handle = init_uart4();
	//printf("serialport4handle shared_data->Serialport4fd: %d  %d\n",serialport4handle,shared_data->Serialport4fd);
	usleep(200000);
	// mandatory delay for main loop to setup every thing..
	shm_handle = shm_open(SHM_NAME, O_RDWR, 0660);
	ShMemory = mmap(NULL, MAKESIZE4MASK(SHM_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handle, 0);
	shared_data = (SHARED_RESOURCES*)ShMemory;

	while(shared_data->validID != 0x46392715)
		usleep(10000);//10ms
//	printf("PcStreamProcess: Init 0\n");

	shm_handlecopc = shm_open(SHM_NAME_COPC, O_RDWR|O_CREAT, 0660);
	ftruncate(shm_handlecopc, MAKESIZE4MASK(SHM_SIZE_COPC));
//	DEBUG_PRINT("truncated shm with name %s of size %ld bytes to map size=%X and map mask=%X\n", SHM_NAME_COPC, MAKESIZE4MASK(SHM_SIZE_COPC), MAKESIZE4MASK(SHM_SIZE_COPC), MAKESIZE4MASK(SHM_SIZE_COPC)-1);
	ShMemorycopc = mmap(NULL, MAKESIZE4MASK(SHM_SIZE_COPC), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handlecopc, 0);
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES*)ShMemorycopc;
//	printf("PcStreamProcess: Init 1\n");
	
	// now init all shared resources..
	shared_data->PcStreamStatus = PROCESSSTATUS_INIT;
	for(i = 0; i < MAX_RAW_IMAGE_PC_BUFFER; i++)
	{
		for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
		{
			compressPcShr->PcImageStatus[0][i][j] = 0;
			if(i == 0)
			{
				compressPcShr->PcImageRdNo[0][j] = 0;
				compressPcShr->PcImageStreamReq[0][j] = 0;
			}
		}
//		shared_data->compressPc->PcImageOutmemory[0][i] = (unsigned char *)malloc(MAXHIRESJPEGIMGSIZE);
	}
//	printf("PcStreamProcess: Init 2\n");
	compressPcShr->PcImageWrNo[0] = 0;
	for(i = 0; i < MAX_RAW_IMAGE_PC_BUFFER; i++)
	{
		for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
		{
			compressPcShr->PcImageStatus[1][i][j] = 0;
			if(i == 0)
			{
				compressPcShr->PcImageRdNo[1][j] = 0;
				compressPcShr->PcImageStreamReq[1][j] = 0;
			}
		}
//		shared_data->compressPc->PcImageOutmemory[1][i] = (unsigned char *)malloc(MAXLORESJPEGIMGSIZE);
	}
	shared_data->Serialport4fd = serialport4handle;
	compressPcShr->PcImageWrNo[1] = 0;
	compressPcShr->PcViewList[0] = 0;
	compressPcShr->PcViewList[1] = 0;
	compressPcShr->PcViewList[2] = 0;
	compressPcShr->PcViewList[3] = 0;
	compressPcShr->validID2 = 0x79312648;
//	printf("PcStreamProcess: Init 3\n");

	// start processing...
	PcStreamProcessFunction(shared_data, compressPcShr, pipes);
	close_uart4(serialport4handle);
	munmap(ShMemory, MAKESIZE4MASK(SHM_SIZE));
	munmap(ShMemorycopc, MAKESIZE4MASK(SHM_SIZE_COPC));
}

/********************************************************************************************/

//-----------------------------------------------------------------------------------------------------------------
void DebugThreadedSocket(void * arg, int * pipes)
{
	int 			i, j, sock_handle, socket_handle;
	static pthread_t 	sock_thread[NO_OF_DEBUG_SERVER_SOCKETS-1];
	struct sockaddr_in 	socket_addr, client_socket_addr;
	socklen_t		addr_len;
	int			attempt = 0;
	char			ipaddrs[32] = {"0.0.0.0"};
	struct tst		sock_thread_data[NO_OF_DEBUG_SERVER_SOCKETS];
	char 			data[NO_OF_CHILDS][512], c;
	unsigned char 		datacnt[NO_OF_CHILDS] = {0};

	// initialize all socket related parameters
	for(i = 0; i < NO_OF_CHILDS; i++)
	{
		fcntl(pipes[i],F_SETFL,O_NONBLOCK);
	}
	j = 0;
	while(1)
	{
		usleep(5000);//5ms
		if(keepalive_timer[0] == 0)
		{
			break;
		}
		else
		{
			if(keepalive_timer_reload[0] == 0)
			{
				if(read(wdt_rd_pipe, &c, 1) > 0)
				{
					keepalive_timer_reload[0] = KEEPALIVE_TIMEOUT_COUNT;//
					write(wdt_wr_pipe, &c, 1);
					usleep(1000);
				}
			}
		}
	}
	//------------------------------------------------------
	return;
}



