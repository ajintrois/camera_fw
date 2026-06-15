#include <stdio.h> 
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h> 
#include <sys/time.h>

#include "defines.h"
#include "eeprom.h"
#include "common_shm.h"
#include "sarmcommands.h"
#include "defs_aicam_agc_cfg_helper.h"

static int serial_handle = 0;
static struct termios term_settings, term_settings_backup;
// old cap/sim command 24 bytes..
//static unsigned char capcmd[6] = {0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};
//static unsigned char aprcmd[6] = {0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};
//static unsigned char fpscmd[6] = {0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};
//static unsigned char simcmd[6] = {0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};
//static unsigned char ajvcmd[6] = {0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};

// new cap/sim command with authentication 32 bytes..
//static unsigned char ajvcmd[6] = {0xA5 0xC3, flc0, seqn, aut0, aut1, aut2, +sum, 0x35, 0xa2, cmdf, csum, dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7, dat8, dat9, datA, datB, datC, datD, datE, datF, datG, datH, datI, datJ};
/*
seqn -> sequence number lower 8bit
flc0 -> bit fields, 	[7] sequence number[8]
			[6] sequence number[9]
			[5] restart new seq
			[4] reboot
			[3] factory def
			[2] encryption of data
			[1] sync / keepalive
			[0] toggle bit(restart and factory def)
aut0, aut1, aut2 -> authentication data @ sequence index
+sum -> csum + 5 of all data except +sum.		
*/
extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);
extern int safe_atoi(const char *nptr, int *value);
extern char* safe_strncpy( char* dest, const char* src, size_t count);
extern size_t safe_strlen(const char *str, size_t max_len);


void calculate_cs(unsigned char *data)
{
	data[4] = data[0];
	data[4] += data[1];
	data[4] += data[2];
	data[4] += data[3];
}
/*
static void display_buffer (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j;
	char c;

	for(i = 0; i < (buffer_length/8); i++)
	{
		prin tf("%04d :",i*8);
		for(j = 0; j < 8; j++)
			pr intf(" %02x",(unsigned char) *(source+j+i*8));
		pri ntf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			pri ntf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		prin tf("\n");
	}
}
*/
void SendSerialDataNTXD0(int serial4_handle, char * data, int count)
{
	int i,ret=0;

	if(serial4_handle > 0)
	{
		//if((count > 0) && (count < 16))
		//{
			/*for(i=0;i<count;i++)
			{
			serial_again:				
				ret=write(serial_handle, &data[i], count);
				if(ret<0)
				{
					goto serial_again;	
				}
			}*/
			write(serial_handle, data, count);
		//}
	}
}


void *serialPort4RxAccess(void *tdata)
{
	SHARED_RESOURCES *shared_data;
	unsigned char	rxbuffer[128] = {0};
	unsigned char	rxdata[128] = {0};
	unsigned char 	checksum = 0;
	unsigned char 	rxdatacnt = 0;
	unsigned char 	headerok = 0;
	int i, j, datalogcnt = 0, trigger = 0;
	int crnt_capture_data_index = 0;
	unsigned char	datalog[1024];
	T_E2V_GENERAL_CMD *GenSerialCommandPtr;
	T_E2V_SYNCH_CMD	*SyncSerialCommandPtr;
	T_E2V_CAPTURE_CMD *CapSerialCommandPtr;
	T_E2V_WRAP_FOR_AUTH *Auth_Data;
	unsigned short	temptimesec, temptimems, adjacentvehiclerunningcount = 0;
	RADAR_ALL_VEH_BUFF 	*adjacentvehiclebuff;
	unsigned char		adjacentvehiclelastentry = 0;
	struct timespec		pctime;
	struct timeval 		tval;
	struct tm		brokentime;
	unsigned int		xfpscnt = 0, Capturecmdcnt = 0;
	unsigned int		trigger_on = 0;
	SHARED_CONFIG_DATA 	*sharedConfigData;
	int serial_handle1 = 0;
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;
	
	shared_data = (SHARED_RESOURCES*)tdata;
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;
	adjacentvehiclebuff = (RADAR_ALL_VEH_BUFF*)shared_data->radar_all_veh_data;

	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&sharedConfigData->womensafety_camera_parameters[0]);

	for(i = 0; i < 8; i++)
	{
		shared_data->capture_serial_trigger[i] = 0;
		shared_data->capture_serial_xfpSeqNo[i] = 0;
		shared_data->capture_serial_violationNo[i] = 0;
		shared_data->capture_serial_trigger_lane_ack[i] = 0;
		shared_data->capture_serial_trigger_evidence_ack[i] = 0;
	}
	nice(20);
	//if(sharedConfigData->general_details.primary_stream_fps[0] == 0)
		trigger_on = 1;
	serial_handle1 = shared_data->Serialport4fd;
//	prin tf("Serial Rx procesing start handle=%d \n",serial_handle1);
//	for(i = 0; i < 1024; i++)
//	{
//		shared_data->capturecmdauthdata0[i] = (unsigned char)i;
//		shared_data->capturecmdauthdata1[i] = (unsigned char)i;
//		shared_data->capturecmdauthdata2[i] = (unsigned char)i;
//	}
	memset((unsigned char *)shared_data->capturecmdauthdata0, 0, 1024);
	memset((unsigned char *)shared_data->capturecmdauthdata1, 0, 1024);
	memset((unsigned char *)shared_data->capturecmdauthdata2, 0, 1024);
	shared_data->newcapturecmdauthdata = 0;
	if((lightsettingsptr->data.Header.marker1 == AICAM_AGC_HDR_M1) && (lightsettingsptr->data.Header.marker2 == AICAM_AGC_HDR_M2))
		if(lightsettingsptr->data.Header.CapSerialProtocol == 1)
		{
			//prin tf("============================= old_serialTrigger ================= \n");
			goto old_serialTrigger;
		}
	while(1)
	{
		if(headerok)
		{
			if(rxdatacnt >= 32)
			{
				//display_buffer(rxdata, 32);
				// rxed full data verify checksum and process
				checksum = 5;
				for(i = 0; i<7; i++)
					checksum += rxdata[i];
				for(i = 8; i<32; i++)
					checksum += rxdata[i];
				//pri ntf("serial command expected csum :%d rxed= %d\n", checksum, rxdata[7]);
				if(rxdata[7] == checksum)
				{
					Auth_Data = (T_E2V_WRAP_FOR_AUTH *)rxdata;
					//pri ntf("serial command index = %d auth data:%d %d %d exp %d %d %d\n",  Auth_Data->flags.seqindex, Auth_Data->authdata.auth0, Auth_Data->authdata.auth1, Auth_Data->authdata.auth2, shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex], shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex+1], shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex+2]);
					if(shared_data->CaptureStart)
					{
						//clock_gettime(CLOCK_MONOTONIC, &pctime);// time from start of pc.
						clock_gettime(CLOCK_MONOTONIC_RAW, &pctime);// time from start of pc.
						//clock_gettime(CLOCK_BOOTTIME, &pctime);// time from start of pc.
						GenSerialCommandPtr = (T_E2V_GENERAL_CMD *)&rxdata[8];
						if(shared_data->newcapturecmdauthdata)
						{
							for(i = 0; i < 1024; i++)
							{
								shared_data->capturecmdauthdata0[i] = shared_data->capturecmdauthdata1[i];
								shared_data->capturecmdauthdata1[i] = shared_data->capturecmdauthdata2[i];
							}
							shared_data->newcapturecmdauthdata = 0;
						}
						if(Auth_Data->flags.seqindex < 1022)
						{
						if(!((Auth_Data->authdata.auth0 == 0) && (Auth_Data->authdata.auth1 == 0) && (Auth_Data->authdata.auth2 == 0)))
						{
						if(((shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex] == Auth_Data->authdata.auth0) && 
							(shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex + 1] == Auth_Data->authdata.auth1) &&
								 (shared_data->capturecmdauthdata0[Auth_Data->flags.seqindex + 2] == Auth_Data->authdata.auth2)) || 
						   ((shared_data->capturecmdauthdata1[Auth_Data->flags.seqindex] == Auth_Data->authdata.auth0) && 
							(shared_data->capturecmdauthdata1[Auth_Data->flags.seqindex + 1] == Auth_Data->authdata.auth1) &&
								 (shared_data->capturecmdauthdata1[Auth_Data->flags.seqindex + 2] == Auth_Data->authdata.auth2)))
						{
							if((rxdata[0+8] == 0x47) && (rxdata[1+8] == 0x63))
							{
								// serial IP reset ....
								if((rxdata[2+8] == 0x56) && (rxdata[7+8] == 0x78) && (rxdata[5+8] == 0x9A) && (rxdata[4+8] == 0xBC) && (rxdata[6+8] == 0xDE) && (rxdata[8+8] == 0xF0))
									shared_data->IPDefaults = 1;
								if((rxdata[2+8] == 0x57) && (rxdata[7+8] == 0x78) && (rxdata[5+8] == 0x9A) && (rxdata[4+8] == 0xBC) && (rxdata[6+8] == 0xDE) && (rxdata[8+8] == 0xF0))
									shared_data->FactoryDefaults = 1;
							}
							else if((rxdata[0] == 0x35) && (rxdata[1] == 0xA2))
							{

								//prin/tf("serial command seq no %d auth data:%d %d %d\n", Auth_Data->flags.seqindex, Auth_Data->authdata.auth0, Auth_Data->authdata.auth1, Auth_Data->authdata.auth2);
								i = 0;
								j = 0;
								trigger = 0;
								switch(GenSerialCommandPtr->header.cmdNDestination.command)
								{
									
								case(1)://Violation capture command
									CapSerialCommandPtr = (T_E2V_CAPTURE_CMD *)&rxdata[8];
									Capturecmdcnt++;
									shared_data->Capturecmdcnt = Capturecmdcnt;
									trigger = 1;
									j = 1;
									//print f("Violation capture command %ld, violation no:%d\n", sizeof(T_E2V_CAPTURE_CMD), CapSerialCommandPtr->violationNumber);
									break;
								case(2)://ANPR capture command
									//pri ntf("ANPR capture command %ld\n", sizeof(T_E2V_GENERAL_CMD));
									trigger = 0;
									break;
								case(4)://RLVDS / XFPS trigger command
									SyncSerialCommandPtr = (T_E2V_SYNCH_CMD *)&rxdata[8];
	//								if(trigger_on)
	//								{
	//									trigger = 0;
	//									if(shared_data->CameraNumber == 0)
	//									{
	//										printf("rxdata 2 = %x, %x\n", rxdata[2], (rxdata[2] & 0x08));
	//										if((rxdata[2] & 0x10) != 0)
	//										{
	//											xfpscnt++;
	//											shared_data->xfpscmdcnt = xfpscnt;
	//											trigger = 2;
	//											j = 1;
	//										}
	//									}
	//									else if(shared_data->CameraNumber == 1)
	//									{
	//										if(rxdata[2] & 0x20)
	//										{
	//											shared_data->xfpscmdcnt = shared_data->xfpscmdcnt + 1;
	//											trigger = 2;
	//											j = 1;
	//										}
	//									}
	//								}
	/*								i = (2 << shared_data->CameraNumber);
									if(i & SyncSerialCommandPtr->header.cmdNDestination.destination)
									{
										trigger = 2;
										j = 1;
									}*/
									i = 0;
									//prin tf("RLVDS / XFPS trigger command %ld, violation no:%d, Sync no:%d\n", sizeof(T_E2V_SYNCH_CMD), SyncSerialCommandPtr->violationNumber, SyncSerialCommandPtr->xFpsSequenceNumber);
									break;
								case(5)://Adjacent vehicle command
	//								prin tf("Adjacent vehicle data command 1\n");
									i = 1;
									break;
								case(6)://Adjacent vehiclee command
	//								prin tf("Adjacent vehicle data command 2\n");
									i = 1;
									break;
								case(7)://Adjacent vehicle command
	//								prin tf("Adjacent vehicle data command 3\n");
									i = 1;
									break;
								default://=3->Simulation capture command
									//prin tf("Simulation capture command %ld\n", sizeof(T_E2V_GENERAL_CMD));
									trigger = 1;
									j = 2;
									break;
								}
								if((i == 0) && ((j == 1) || (j == 2)))
								{
									// checksum ok now generate trigger..
									if(crnt_capture_data_index > 7)
										crnt_capture_data_index = 0;
									GenSerialCommandPtr->header.checkSum = trigger;
									for(j = 1; j < 24; j++)
									{
										shared_data->capture_serial_data[crnt_capture_data_index][j] = rxdata[j+8];
									}
									shared_data->capture_serial_violationNo[crnt_capture_data_index] = 0;
									shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = 0;
									shared_data->capture_serial_trigger_timesec[crnt_capture_data_index] = (unsigned int)pctime.tv_sec;
									shared_data->capture_serial_trigger_timemilsec[crnt_capture_data_index] = (unsigned int)(pctime.tv_nsec / 1000000);
									shared_data->capture_serial_trigger_lane_ack[crnt_capture_data_index] = 0;
									shared_data->capture_serial_trigger_evidence_ack[crnt_capture_data_index] = 0;
									if(trigger == 1)
									{
										if(j == 1)
										{
											shared_data->capture_serial_violationNo[crnt_capture_data_index] = CapSerialCommandPtr->violationNumber;
											if(CapSerialCommandPtr->flags.xFpsCapture)
												shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = CapSerialCommandPtr->xFpsSequenceNumber;
										}
									}
									else
										shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = SyncSerialCommandPtr->xFpsSequenceNumber;
									shared_data->capture_serial_data[crnt_capture_data_index][0] = rxdata[8];
									shared_data->capture_serial_trigger[crnt_capture_data_index] = trigger;
									crnt_capture_data_index++;
									if(crnt_capture_data_index > 7)
										crnt_capture_data_index = 0;
									sync();
								}
	/*---------------------------------------------------------------------------------------------------------------------*/					
								//Adjacent vehicle command -> save in data buff
								if(adjacentvehiclelastentry >= MAX_RADAR_ALL_VEH_DATA)
									adjacentvehiclelastentry = 0;
								if(adjacentvehiclebuff->header != 0x40f19243)
								{
									// clear contents...
									memset((unsigned char *)shared_data->radar_all_veh_data, 0, 2048);
									adjacentvehiclebuff->header = 0x40f19243;
									adjacentvehiclelastentry = 0;
								}
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].header = 0x9531;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetrunningcnt = adjacentvehiclerunningcount++;
								//gettimeofday(&tval,NULL);// for timing...
								localtime_r(&pctime.tv_sec, &brokentime);
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmSecs = (short)(pctime.tv_nsec/1000000);
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rsec = brokentime.tm_sec;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmin = brokentime.tm_min;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rhour = brokentime.tm_hour;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rdate = brokentime.tm_mday;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmon = brokentime.tm_mon;
								adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].ryear = brokentime.tm_year;// from 1900
								//safe_memcpy(adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetdata, 24, &rxdata[8], 24);// size verified..
								mempcpy(adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetdata, &rxdata[8], 24);
								adjacentvehiclebuff->lastdatapos = adjacentvehiclelastentry;
								adjacentvehiclebuff->lastpacketrunningcnt = adjacentvehiclerunningcount;
								adjacentvehiclebuff->no_ofdata = (adjacentvehiclerunningcount > MAX_RADAR_ALL_VEH_DATA)?(MAX_RADAR_ALL_VEH_DATA-1):adjacentvehiclerunningcount;
								adjacentvehiclelastentry++;
								if(adjacentvehiclelastentry >= MAX_RADAR_ALL_VEH_DATA)
									adjacentvehiclelastentry = 0;
	/*---------------------------------------------------------------------------------------------------------------------*/					
							}
						}
						}
						}
					}
					rxdatacnt -= 32;
					if(rxdatacnt)
						for(j = 0; j < rxdatacnt; j++)
						{
							rxdata[j] = rxdata[j+32];
						}
				}
				else
				{
					// checksum fail.. now find header again...
					for(j = 0; j < rxdatacnt-2; j++)
					{
						rxdata[j] = rxdata[j+2];
					}
					rxdatacnt -= 2;
				}
				headerok = 0;
			}
		}
		else
		{
			if(rxdatacnt > 4)
			{// header rxed..
/*				if(rxdata[0] == 0x35)
				{
					if(rxdata[1] == 0xA2)
					{
						headerok = 1;
						//pri ntf("serial header rxed %x cmd %x\n",rxdata[0], rxdata[2]);
					}
					else
					{
						for(i = 1; i < rxdatacnt; i++)
							rxdata[i-1] = rxdata[i];
						rxdatacnt--;
					}
				}
				else// shift data.. 
				{
					//pri ntf("header failed %x %x\n",rxdata[0], rxdata[1]);
					for(i = 1; i < rxdatacnt; i++)
						rxdata[i-1] = rxdata[i];
					rxdatacnt--;
				}  
=======OLD CODE======*/
				if(rxdata[0] == 0xA5)
				{
					if(rxdata[1] == 0xC3)
					{
						headerok = 1;
						//pr intf("serial header rxed %x cmd %x\n",rxdata[0], rxdata[2]);
					}
					else
					{
						for(i = 1; i < rxdatacnt; i++)
							rxdata[i-1] = rxdata[i];
						rxdatacnt--;
					}
				}
				else// shift data.. 
				{
					//prin tf("header failed %x %x\n",rxdata[0], rxdata[1]);
					for(i = 1; i < rxdatacnt; i++)
						rxdata[i-1] = rxdata[i];
					rxdatacnt--;
				}  
			}
		}
		i = read(serial_handle1, rxbuffer, 1);
		if(i>0)
		{
			if(i < 48)
			{
				if(rxdatacnt < 64)
				{
					for(j = 0; j < i; j++)
						rxdata[rxdatacnt+j] = rxbuffer[j];
					rxdatacnt += i;
/*					for(j = 0; j < i; j++)
						datalog[datalogcnt++] = rxbuffer[j];
					if(datalogcnt >= 256)
					{
						datalogcnt = 0;
						display_buffer(datalog, 24);
					}*/
				}
			}
		}
		else
		{
			usleep(250);
		}
	}
	goto exit_trigger;
	
old_serialTrigger:
	while(1)
	{
		if(headerok)
		{
			if(rxdatacnt >= 24)
			{
				// rxed full data verify checksum and process
				checksum = rxdata[0];
				checksum += rxdata[1];
				checksum += rxdata[2];
				for(i = 4; i<24; i++)
					checksum += rxdata[i];
				//display_buffer(rxdata, 24);
				if(rxdata[3] == checksum)
				{
					if((rxdata[0] == 0x47) && (rxdata[1] == 0x63))
					{
						//prin tf(" =============== IP def rxed ================\n");
						// serial IP reset ....
						if((rxdata[2] == 0x56) && (rxdata[7] == 0x78) && (rxdata[5] == 0x9A) && (rxdata[4] == 0xBC) && (rxdata[6] == 0xDE) && (rxdata[8] == 0xF0))
						{
							shared_data->IPDefaults = 1;
						}
						if((rxdata[2] == 0x57) && (rxdata[7] == 0x78) && (rxdata[5] == 0x9A) && (rxdata[4] == 0xBC) && (rxdata[6] == 0xDE) && (rxdata[8] == 0xF0))
						{
							shared_data->FactoryDefaults = 1;
						}
					}
					else if((rxdata[0] == 0x35) && (rxdata[1] == 0xA2))
					{
						if(shared_data->CaptureStart)
						{
							//display_buffer(rxdata, 24);
							//clock_gettime(CLOCK_MONOTONIC, &pctime);// time from start of pc.
							clock_gettime(CLOCK_MONOTONIC_RAW, &pctime);// time from start of pc.
							//clock_gettime(CLOCK_BOOTTIME, &pctime);// time from start of pc.
							GenSerialCommandPtr = (T_E2V_GENERAL_CMD *)rxdata;
							i = 0;
							j = 0;
							trigger = 0;
							switch(GenSerialCommandPtr->header.cmdNDestination.command)
							{
								
							case(1)://Violation capture command
								CapSerialCommandPtr = (T_E2V_CAPTURE_CMD *)rxdata;
								Capturecmdcnt++;
								shared_data->Capturecmdcnt = Capturecmdcnt;
								trigger = 1;
								j = 1;
								//pri ntf("Violation capture command %ld, violation no:%d\n", sizeof(T_E2V_CAPTURE_CMD), CapSerialCommandPtr->violationNumber);
								break;
							case(2)://ANPR capture command
								//pri ntf("ANPR capture command %ld\n", sizeof(T_E2V_GENERAL_CMD));
								trigger = 0;
								break;
							case(4)://RLVDS / XFPS trigger command
	//							SyncSerialCommandPtr = (T_E2V_SYNCH_CMD *)rxdata;
	//							if(trigger_on)
	//							{
	//								trigger = 0;
	//								if(shared_data->CameraNumber == 0)
	//								{
	//									prin tf("rxdata 2 = %x, %x\n", rxdata[2], (rxdata[2] & 0x08));
	//									if((rxdata[2] & 0x10) != 0)
	//									{
	//										xfpscnt++;
	//										shared_data->xfpscmdcnt = xfpscnt;
	//										trigger = 2;
	//										j = 1;
	//									}
	//								}
	//								else if(shared_data->CameraNumber == 1)
	//								{
	//									if(rxdata[2] & 0x20)
	//									{
	//										shared_data->xfpscmdcnt = shared_data->xfpscmdcnt + 1;
	//										trigger = 2;
	//										j = 1;
	//									}
	//								}
	//							}
	/*							i = (2 << shared_data->CameraNumber);
								if(i & SyncSerialCommandPtr->header.cmdNDestination.destination)
								{
									trigger = 2;
									j = 1;
								}*/
								i = 0;
								//prin tf("RLVDS / XFPS trigger command %ld, violation no:%d, Sync no:%d\n", sizeof(T_E2V_SYNCH_CMD), SyncSerialCommandPtr->violationNumber, SyncSerialCommandPtr->xFpsSequenceNumber);
								break;
							case(5)://Adjacent vehicle command
	//							prin tf("Adjacent vehicle data command 1\n");
								i = 1;
								break;
							case(6)://Adjacent vehiclee command
	//							pri ntf("Adjacent vehicle data command 2\n");
								i = 1;
								break;
							case(7)://Adjacent vehicle command
	//							pri ntf("Adjacent vehicle data command 3\n");
								i = 1;
								break;
							default://=3->Simulation capture command
								//pri ntf("Simulation capture command %ld\n", sizeof(T_E2V_GENERAL_CMD));
								trigger = 1;
								j = 2;
								break;
							}
							if((i == 0) && ((j == 1) || (j == 2)))
							{
								// checksum ok now generate trigger..
								if(crnt_capture_data_index > 7)
									crnt_capture_data_index = 0;
								GenSerialCommandPtr->header.checkSum = trigger;
								for(j = 1; j < 24; j++)
								{
									shared_data->capture_serial_data[crnt_capture_data_index][j] = rxdata[j];
								}
								shared_data->capture_serial_violationNo[crnt_capture_data_index] = 0;
								shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = 0;
								shared_data->capture_serial_trigger_timesec[crnt_capture_data_index] = (unsigned int)pctime.tv_sec;
								shared_data->capture_serial_trigger_timemilsec[crnt_capture_data_index] = (unsigned int)(pctime.tv_nsec / 1000000);
								shared_data->capture_serial_trigger_lane_ack[crnt_capture_data_index] = 0;
								shared_data->capture_serial_trigger_evidence_ack[crnt_capture_data_index] = 0;
								if(trigger == 1)
								{
									if(j == 1)
									{
										shared_data->capture_serial_violationNo[crnt_capture_data_index] = CapSerialCommandPtr->violationNumber;
										if(CapSerialCommandPtr->flags.xFpsCapture)
											shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = CapSerialCommandPtr->xFpsSequenceNumber;
									}
								}
								else
									shared_data->capture_serial_xfpSeqNo[crnt_capture_data_index] = SyncSerialCommandPtr->xFpsSequenceNumber;
								shared_data->capture_serial_data[crnt_capture_data_index][0] = rxdata[0];
								shared_data->capture_serial_trigger[crnt_capture_data_index] = trigger;
								crnt_capture_data_index++;
								if(crnt_capture_data_index > 7)
									crnt_capture_data_index = 0;
								sync();
							}
	/*---------------------------------------------------------------------------------------------------------------------*/					
							//Adjacent vehicle command -> save in data buff
							if(adjacentvehiclelastentry >= MAX_RADAR_ALL_VEH_DATA)
								adjacentvehiclelastentry = 0;
							if(adjacentvehiclebuff->header != 0x40f19243)
							{
								// clear contents...
								memset((unsigned char *)shared_data->radar_all_veh_data, 0, 2048);
								adjacentvehiclebuff->header = 0x40f19243;
								adjacentvehiclelastentry = 0;
							}
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].header = 0x9531;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetrunningcnt = adjacentvehiclerunningcount++;
							//gettimeofday(&tval,NULL);// for timing...
							localtime_r(&pctime.tv_sec, &brokentime);
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmSecs = (short)(pctime.tv_nsec/1000000);
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rsec = brokentime.tm_sec;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmin = brokentime.tm_min;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rhour = brokentime.tm_hour;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rdate = brokentime.tm_mday;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].rmon = brokentime.tm_mon;
							adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].ryear = brokentime.tm_year;// from 1900
							//safe_memcpy(adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetdata, 24, &rxdata[8], 24);// size verified..
							mempcpy(adjacentvehiclebuff->allvehicledata[adjacentvehiclelastentry].packetdata, rxdata, 24);
							adjacentvehiclebuff->lastdatapos = adjacentvehiclelastentry;
							adjacentvehiclebuff->lastpacketrunningcnt = adjacentvehiclerunningcount;
							adjacentvehiclebuff->no_ofdata = (adjacentvehiclerunningcount > MAX_RADAR_ALL_VEH_DATA)?(MAX_RADAR_ALL_VEH_DATA-1):adjacentvehiclerunningcount;
							adjacentvehiclelastentry++;
							if(adjacentvehiclelastentry >= MAX_RADAR_ALL_VEH_DATA)
								adjacentvehiclelastentry = 0;
	/*---------------------------------------------------------------------------------------------------------------------*/					
						}
					}
					rxdatacnt -= 24;
					if(rxdatacnt)
						for(j = 0; j < rxdatacnt; j++)
						{
							rxdata[j] = rxdata[j+24];
						}
				}
				else
				{
					// checksum fail.. now find header again...
					for(j = 0; j < rxdatacnt-2; j++)
					{
						rxdata[j] = rxdata[j+2];
					}
					rxdatacnt -= 2;
				}
				headerok = 0;
			}
		}
		else
		{
			if(rxdatacnt > 4)
			{// header rxed..
				if(rxdata[0] == 0x35)
				{
					if(rxdata[1] == 0xA2)
					{
						headerok = 1;
						//pri ntf("serial header rxed %x cmd %x\n",rxdata[0], rxdata[2]);
					}
					else
					{
						for(i = 1; i < rxdatacnt; i++)
							rxdata[i-1] = rxdata[i];
						rxdatacnt--;
					}
				}
				else if(rxdata[0] == 0x47)
				{
					if(rxdata[1] == 0x63)
					{
						headerok = 1;
						//prin tf("serial header rxed %x cmd %x\n",rxdata[0], rxdata[2]);
					}
					else
					{
						for(i = 1; i < rxdatacnt; i++)
							rxdata[i-1] = rxdata[i];
						rxdatacnt--;
					}
				}
				else// shift data.. 
				{
					//pri ntf("header failed %x %x\n",rxdata[0], rxdata[1]);
					for(i = 1; i < rxdatacnt; i++)
						rxdata[i-1] = rxdata[i];
					rxdatacnt--;
				}
			}
		}
		i = read(serial_handle1, rxbuffer, 1);
		if(i>0)
		{
			if(i < 32)
			{
				if(rxdatacnt < 48)
				{
					for(j = 0; j < i; j++)
						rxdata[rxdatacnt+j] = rxbuffer[j];
					rxdatacnt += i;
/*					for(j = 0; j < i; j++)
						datalog[datalogcnt++] = rxbuffer[j];
					if(datalogcnt >= 256)
					{
						datalogcnt = 0;
						display_buffer(datalog, 24);
					}*/
				}
			}
		}
		else
		{
			usleep(250);
		}
	}
exit_trigger:
	return tdata;
}

int init_uart4()
{
	int i, flag;

	serial_handle = open("/dev/ttyTHS3", O_RDWR | O_NOCTTY | O_NDELAY);
	
	if(serial_handle < 0)
	{
		serial_handle = 0;
	}
	else
	{
		fcntl(serial_handle,F_SETFL,O_NONBLOCK);
		
		tcgetattr(serial_handle,&term_settings);
		tcgetattr(serial_handle,&term_settings_backup);
		memset(&term_settings, 0, sizeof(term_settings));  /* clear the new struct */
		
	term_settings.c_cflag = CS8 | CSTOPB | CLOCAL | CREAD;
	term_settings.c_iflag = IGNPAR;
	term_settings.c_oflag = 0;
	term_settings.c_lflag = 0;
	term_settings.c_cc[VMIN] = 0;      /* block untill n bytes are received */
	term_settings.c_cc[VTIME] = 0;     /* block untill a timer expires (n * 100 mSec.) */

	cfsetispeed(&term_settings, B115200);
	cfsetospeed(&term_settings, B115200);

	tcsetattr (serial_handle,TCSANOW,&term_settings);
		
		
		/*term_settings.c_lflag &=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
		term_settings.c_oflag &= ~OPOST;
		term_settings.c_lflag &=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		term_settings.c_cflag &= ~(PARENB);
		term_settings.c_cflag |= CSIZE | CS8 | CSTOPB;
		term_settings.c_cc[VMIN] =1;
		term_settings.c_cc[VTIME] =0;
		cfsetispeed(&term_settings, B115200);
		cfsetospeed(&term_settings, B115200);
		tcsetattr (serial_handle,TCSANOW,&term_settings);*/
	}
	return serial_handle;	
}

void close_uart4(int serial4_handle)
{
	if(serial4_handle)
	{
		tcsetattr (serial4_handle,TCSANOW,&term_settings_backup);
		close(serial4_handle);
	}
}

int mainserial4(int argc, char* argv[])
{
  static struct termios term_settings, term_settings_backup;
  int i, flag;
  int serial_handle = 0;
  unsigned char gpio_data[6] = {0x5A, 0x7E, 0x55, 0xAA, 0, 0};
  unsigned char keepalive_data[6] = {0x5A, 0x7E, 0x22, 0x33, 0, 0};
unsigned char readdata[32] = {0};
  
	if(argc < 2)
	{
//		//pri tf("%s <serial Port>\n",argv[0]);
		return 0;
	}
//  if(argc > 1)
//  {
//	pri tf("open port %s\n", argv[1]);
	//i = at oi(argv[1]);
//  }
/*  if(i == 1)
    serial_handle = open("/dev/ttyTHS1", O_RDWR);
  else if(i == 2)
    serial_handle = open("/dev/ttyTHS2", O_RDWR);
  else if(i == 3)
    serial_handle = open("/dev/ttyTHS3", O_RDWR);
  else if(i == 4)
    serial_handle = open("/dev/ttyS4", O_RDWR);
  else if(i == 5)
    serial_handle = open("/dev/ttyS5", O_RDWR);
  else
    serial_handle = open("/dev/ttyS0", O_RDWR);
*/
  serial_handle = open(argv[1], O_RDWR);
  if(serial_handle < 1)
  {
//    prin tf("dev %s open error\n", argv[1]);
    return 0;
  }
  fcntl(serial_handle,F_SETFL,O_NONBLOCK);
  tcgetattr(serial_handle,&term_settings);
  tcgetattr(serial_handle,&term_settings_backup);
  term_settings.c_lflag &=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  term_settings.c_oflag &= ~OPOST;
  term_settings.c_lflag &=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  term_settings.c_cflag &= ~(PARENB);
  term_settings.c_cflag |= CSIZE | CS8 | CSTOPB;
  term_settings.c_cc[VMIN] =1;
  term_settings.c_cc[VTIME] =0;
  cfsetispeed(&term_settings, B115200);
  cfsetospeed(&term_settings, B115200);
  tcsetattr (serial_handle,TCSANOW,&term_settings);	
  flag = 0;
  i = 0;
  while(1)
  {
    flag++;
    gpio_data[3] = 1 << (i&3);
    calculate_cs(gpio_data);
    write(serial_handle, gpio_data, 5);
	usleep(10000);
	memset(readdata, 0, 32);
	i = read(serial_handle, readdata, 5);
//	display_buffer(readdata, 8);
    usleep(500000);
	if(flag > 20)
	{
		flag = 0;
		calculate_cs(keepalive_data);
		write(serial_handle, keepalive_data, 5);
		usleep(500000);
	}
  }
  tcsetattr (fileno(stdin),TCSANOW,&term_settings_backup);	
}


 
