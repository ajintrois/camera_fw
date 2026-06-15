/********************************************************************************************/
/*		Project		:	IP Camera					    */
/*		Filename	:	eeprom.c					    */
/*		Functionality	:	EEPROM routine					    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h> 
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <openssl/evp.h>
#include <ctype.h>

#include "defines.h"
#include "eeprom.h"
#include "common_shm.h"
#include "defs_aicam_agc_cfg_helper.h"

#define FILENAME "/sys/bus/nvmem/devices/fuse/nvmem"


const char FsConfigfile[64] = "/usr/Camera/bin/Fsconfig.bin";
FILE	*fsconfigfptr = NULL;
static int	fsconfigstatus = 0;// 0=file created, 1=file exists, 2= config copied ok.
static int	fsconfigencstatus = 0;// 0=file raw, 1=file encrypted
static unsigned char configdata_rec[(32*1024) + 256];
static unsigned char configdata_enc[(32*1024) + 256];
/********************************************************************************************/
/*                          Defines                                                         */
/********************************************************************************************/
#define _24LC64		1
#define _24LC64_WP		2
#define I2C_ADDR_24LC64 	0x56
#define I2C_ADDR_24LC64_WP	2
#define MAX_BYTES       	128  	/* max number of bytes to write in one piece */
#define I2C_RDWR		0x0707	/* Combined R/W transfer (one stop only)*/
#define I2C_RD			0x01

#define GCM_KEY_SIZE 32   // 256 bits
#define GCM_IV_SIZE  12   // Recommended for GCM
#define GCM_TAG_SIZE 16   // 128-bit authentication tag
#define GCM_MAX_ENC_BLOCK_SIZE (128*1024)
#define GCM_MAX_ENC_BLOCK_SIZE_ENC (128*1024 + 4096)
#
/********************************************************************************************/
/*                          Extern		                                            */
/********************************************************************************************/
//extern char 		DebugStr[1024];
//extern int 		DebugStrSize;
extern int 	debug_flag;
extern char 	debug_buffer[];
extern char 	*DebugBufferPointer;
extern int	lux, prevLux, prev_ampm_flag, ampm_flag, crnt_table;
/********************************************************************************************/
/*                          Extern Prototypes                                               */
/********************************************************************************************/
extern void fifo_data_write(char * buff,unsigned long size);
extern int DeadDelayMS(int ms);
extern int safe_fgets(char *s, size_t maxbufsz, int count, FILE *stream);
extern int safe_memcpy(void *dest, size_t destsz, void *src, size_t count);
extern int safe_atoi(const char *nptr, int *value);
extern char* safe_strncpy( char* dest, const char* src, size_t count);
extern size_t safe_strlen(const char *str, size_t max_len);
extern void loganevent(const char *processname, const char *eventstr);

/********************************************************************************************/
/*                          Variables		                                            */
/********************************************************************************************/
unsigned char	Aeskey[32], DecIV[16];
static int	eepromfd	= 0;
int EepromPageWriteDelay=0;
char keystr[16] = {0};
static unsigned char mtxcfgkey[32] = {0};
static unsigned char mtxcfgiv[16] = {0};
int configWrite_trigger = 0;// 0=no trigger, 1= timer run
int configWrite_timer = 0; // when trigger=1 countdown starts and writes when expires.

struct gnrl general_details,new_general_details;
struct cam camera_parameters[4],new_camera_parameters[4];
struct ip ip_details,new_ip_details;
struct remo remote_user[MAX_REMOTE_USER],new_remote_user[MAX_REMOTE_USER],blank_remote_user[MAX_REMOTE_USER];
struct nocs no_checksum_data,new_no_checksum_data;
struct chsm checksum,new_checksum;
struct wrpr write_protected_data;
struct sch schedule[MAX_SCH_TIMING][MAX_SCH_CHANGES],new_schedule[MAX_SCH_TIMING][MAX_SCH_CHANGES];
struct alrm alarm_data,new_alarm_data;
struct rtsp_stream rtsp_details,new_rtsp_details;
struct wb wb_details,new_wb_details;
struct eeprom_verify ev;
struct dydns dydns_data,new_dydns_data;

struct womensafety_cam womensafety_camera_parameters[65],new_womensafety_camera_parameters[65];
//struct womensafety_chsm womensafety_checksum,new_womensafety_checksum;

/********************************************************************************************/
/*                          Function Prototypes                                             */
/********************************************************************************************/
void save_womensafety_camera_parameters(void);
void default_womensafety_camera_parameters(void);
void verify_womensafety_camera_parameters (void);
void init_womensafety_camera_parameters(void);
void read_womensafety_camera_parameters(void);

void save_womensafety_programmed_data(void);// from PCstream...
void verify_womensafety_fram (void);// from PCstream...
void saveconfigdataenc(void);
//void logSharedConfigData();


void save_to_eeprom (void);
void save_general_details (void);
void save_camera_parameters(void);
void save_remote_user_data(void);
void save_ip_details (void);
void save_dydns_data (void);
void save_alarm_data (void);
void save_rtsp_details (void);
void save_wb_details (void);
void save_checksum(void);
void save_no_cs_data(void);
void save_write_protected_data(void);
void save_programmed_data(void);
void default_camera_parameters(void);
void default_remote_user_data(void);
void default_general_details(void);
void default_ip_details(void);
void default_dydns (void);
void default_alarm (void);
void default_rtsp (void);
void default_wb (void);
void verify_fram (void);
void verify_write_protected_data (void);
void verify_general_details (void);
void  verify_camera_parameters (void);
void verify_remote_user_data (void);
void verify_ip_details (void);
void verify_dydns_data (void);
void verify_alarm_data (void);
void verify_rtsp_details (void);
void verify_wb_details (void);
void verify_nocs_data (void);
void verify_eeprom(void);
void init_general_details(void);
void init_camera_parameters(void);
void init_remote_user_data(void);
void init_ip_details(void);
void init_dydns(void);
void init_alarm(void);
void init_rtsp(void);
void init_wb(void);
void read_general_details(void);
void read_camera_parameters(void);
void read_remote_user_data(void);
void read_ip_details(void);
void read_dydns_data(void);
void read_alarm_data(void);
void read_rtsp_details(void);
void read_wb_details(void);
void set_bcsh(unsigned char cam, unsigned char brightness, unsigned char contrast, unsigned char saturation);
static unsigned short _ReadFromFRAM(unsigned char device,unsigned int slave_addr,
			     unsigned char *destination, unsigned short offset, unsigned short length);
static unsigned short _WriteToFRAM(unsigned char device, unsigned int slave_addr,
			    unsigned char *source, unsigned char *old_data, 
			    unsigned short offset, unsigned short length, unsigned char all);

extern void driveICRDIR(int val);
extern void driveICREN(int val);
extern void driveGpio200(int val);// flash en

/********************************************************************************************/
/*                          Function Defines                                                */
/********************************************************************************************/
static void display_buffer (unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j;
	char c;

	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		for(j = 0; j < 8; j++)
			printf(" %02x",(unsigned char) *(source+j+i*8));
		printf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
	}
}

int ApplyLightSettings(void * data, int pipefd)
{
	SHARED_RESOURCES	*shared_data; 			
	int 			i = 0, j = 0;
	static int		lightTableInit = 0, luxinactivity_timer = 0, inactivity_timer = 0, flashEnabled = 250, IcrControl = 250, NightTime = 0;
	char 			DebugStr[1024];
	int 			DebugStrSize;
	shared_data = (SHARED_RESOURCES*)data;
	SHARED_CONFIG_DATA 	*sharedConfigData;
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;

	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&sharedConfigData->womensafety_camera_parameters[0]);
	j = 0;
	if(shared_data->applyCurrentLightSettings == 1)
	{
		shared_data->applyCurrentLightSettings = 0;
/*		printf("Light settings recieved...\n");
		printf(" lightsettingsptr->data.Header.marker1 = 0x%x \n", lightsettingsptr->data.Header.marker1);
		printf(" lightsettingsptr->data.Header.marker2 = 0x%x \n", lightsettingsptr->data.Header.marker2);
		printf(" lightsettingsptr->data.Header.EnabledFlag = 0x%x \n", lightsettingsptr->data.Header.EnabledFlag);
		printf(" lightsettingsptr->data.Header.DualCapFlag = 0x%x \n", lightsettingsptr->data.Header.DualCapFlag);
		printf(" lightsettingsptr->data.Header.AgcTargetHigh = 0x%x \n", lightsettingsptr->data.Header.AgcTargetHigh);
		printf(" lightsettingsptr->data.Header.AgcTargetLow = 0x%x \n", lightsettingsptr->data.Header.AgcTargetLow);
		printf(" lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue = 0x%x \n", lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue);
		printf(" lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue = 0x%x \n", lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue);
		printf(" lightsettingsptr->data.Night_Settings.NightStart = 0x%x \n", lightsettingsptr->data.Night_Settings.NightStart);
		printf(" lightsettingsptr->data.Night_Settings.NightEnd = 0x%x \n", lightsettingsptr->data.Night_Settings.NightEnd);
		printf(" lightsettingsptr->data.Night_Settings.DualCaptureFlag = 0x%x \n", lightsettingsptr->data.Night_Settings.DualCaptureFlag);
		printf(" lightsettingsptr->data.Night_Settings.Gamma.First = 0x%x \n", lightsettingsptr->data.Night_Settings.Gamma.First);
		printf(" lightsettingsptr->data.Night_Settings.Gamma.Second = 0x%x \n", lightsettingsptr->data.Night_Settings.Gamma.Second);
		printf(" lightsettingsptr->data.Night_Settings.Gain.First = 0x%x \n", lightsettingsptr->data.Night_Settings.Gain.First);
		printf(" lightsettingsptr->data.Night_Settings.Gain.Second = 0x%x \n", lightsettingsptr->data.Night_Settings.Gain.Second);
		printf(" lightsettingsptr->data.Night_Settings.Shutter.First = 0x%x \n", lightsettingsptr->data.Night_Settings.Shutter.First);
		printf(" lightsettingsptr->data.Night_Settings.Shutter.Second = 0x%x \n", lightsettingsptr->data.Night_Settings.Shutter.Second);
		printf(" lightsettingsptr->data.AGC_Settings[0].AvgSel = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].AvgSel);
		printf(" lightsettingsptr->data.AGC_Settings[0].ResponseTime = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].ResponseTime);
		printf(" lightsettingsptr->data.AGC_Settings[0].GammaIndex = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].GammaIndex);
		printf(" lightsettingsptr->data.AGC_Settings[0].AgcPLowThreshold = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].AgcPLowThreshold);
		printf(" lightsettingsptr->data.AGC_Settings[0].AgcTargetLowThreshold  = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].AgcTargetLowThreshold );
		printf(" lightsettingsptr->data.AGC_Settings[0].AgcTargetHighThreshold = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].AgcTargetHighThreshold);
		printf(" lightsettingsptr->data.AGC_Settings[0].AgcPHighThreshold  = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].AgcPHighThreshold );
		printf(" lightsettingsptr->data.AGC_Settings[0].shuttermax = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].shuttermax);
		printf(" lightsettingsptr->data.AGC_Settings[0].shuttermin = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].shuttermin);
		printf(" lightsettingsptr->data.AGC_Settings[0].gainmax = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].gainmax);
		printf(" lightsettingsptr->data.AGC_Settings[0].gainmin = 0x%x \n", lightsettingsptr->data.AGC_Settings[0].gainmin);

		printf(" lightsettingsptr->data.AGC_Settings[1].AvgSel = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].AvgSel);
		printf(" lightsettingsptr->data.AGC_Settings[1].ResponseTime = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].ResponseTime);
		printf(" lightsettingsptr->data.AGC_Settings[1].GammaIndex = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].GammaIndex);
		printf(" lightsettingsptr->data.AGC_Settings[1].AgcPLowThreshold = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].AgcPLowThreshold);
		printf(" lightsettingsptr->data.AGC_Settings[1].AgcTargetLowThreshold  = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].AgcTargetLowThreshold );
		printf(" lightsettingsptr->data.AGC_Settings[1].AgcTargetHighThreshold = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].AgcTargetHighThreshold);
		printf(" lightsettingsptr->data.AGC_Settings[1].AgcPHighThreshold  = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].AgcPHighThreshold );
		printf(" lightsettingsptr->data.AGC_Settings[1].shuttermax = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].shuttermax);
		printf(" lightsettingsptr->data.AGC_Settings[1].shuttermin = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].shuttermin);
		printf(" lightsettingsptr->data.AGC_Settings[1].gainmax = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].gainmax);
		printf(" lightsettingsptr->data.AGC_Settings[1].gainmin = 0x%x \n", lightsettingsptr->data.AGC_Settings[1].gainmin);
		*/
		luxinactivity_timer = 0;// force check...
		j = 1;
	}
	if((shared_data->rtctimecode[2] >= lightsettingsptr->data.Night_Settings.NightStart) || (shared_data->rtctimecode[2] < lightsettingsptr->data.Night_Settings.NightEnd))
	{
		if(NightTime < 2)
		{
			NightTime = 2;
			shared_data->crntLightTable = 2;
			j = 1;
		}
	}
	else
	{
		if(NightTime == 2)
		{
			NightTime = 1;
		}
	}
  	if(NightTime != 2)
	{
		if(shared_data->lux > 255)
			lux = 255;
		else if(shared_data->lux < 1)
			lux = 0;
		else
			lux = shared_data->lux;
		if((prevLux != lux) || (luxinactivity_timer == 0) || (inactivity_timer > 50))
		{
			luxinactivity_timer = 20;
			prevLux = lux;
			if(NightTime == 1)
			{
				if(lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue >= lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue)
				{
					if(lux > lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue)
					{
						//DEBUG_PRINT("night cleared lux%d ------ %d %d\n", lux, lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue, lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue);
						shared_data->crntLightTable = 2;
						NightTime = 0;
						j = 1;
					}
				}
				else
				{
					if(lux > lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue)
					{
						//DEBUG_PRINT("night cleared1 lux%d------ %d %d\n", lux, lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue, lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue);
						shared_data->crntLightTable = 2;
						NightTime = 0;
						j = 1;
					}
				}
			}
			else if(NightTime == 0)
			{
				if(lux <= lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue)
				{
					//DEBUG_PRINT("to night lux%d------ %d %d\n", lux, lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue, lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue);
					shared_data->crntLightTable = 1;
					NightTime = 1;
					j = 1;
				}
			}
			if(inactivity_timer > 50)
			{
				inactivity_timer = 0;
				j = 1;
			}
		}
		else //if(inactivity_timer > 50)// if lux sensor is not connected...
		{
			// check every 5 secs if any change req..
			if(luxinactivity_timer)
				luxinactivity_timer--;
		}
	}
	if(lightTableInit == 0)
	{
		lightTableInit = 1;
		j = 1;
	}
	inactivity_timer++;
	
	if(j == 1)
	{
		inactivity_timer = 0;
		if(NightTime > 0)
		{
			shared_data->IcrControl 	= 1;
			shared_data->DualCaptureEnabled = lightsettingsptr->data.Night_Settings.DualCaptureFlag;
			shared_data->GammaIndex1 	= lightsettingsptr->data.Night_Settings.Gamma.First;// = 0;
			shared_data->GammaIndex2 	= lightsettingsptr->data.Night_Settings.Gamma.Second;// = 0;
			shared_data->GainIndex1 	= lightsettingsptr->data.Night_Settings.Gain.First;// = 5;
			shared_data->GainIndex2 	= lightsettingsptr->data.Night_Settings.Gain.Second;// = 2;
			shared_data->ShutterIndex1 	= lightsettingsptr->data.Night_Settings.Shutter.First;// = 16;
			shared_data->ShutterIndex2 	= lightsettingsptr->data.Night_Settings.Shutter.Second;// = 5;
			shared_data->flashEnabled 	= lightsettingsptr->data.Night_Settings.Flash_enabled;
		}
		else
		{
			shared_data->flashEnabled 	= 0;
			shared_data->IcrControl 	= 0;
			if(lightsettingsptr->data.Header.DualCapFlag != 0)// on
			{
				shared_data->AgcTargetHigh 	= lightsettingsptr->data.Header.AgcTargetHigh;
				shared_data->AvgSel1 		= lightsettingsptr->data.AGC_Settings[0].AvgSel;
				shared_data->ResponseTime1 	= lightsettingsptr->data.AGC_Settings[0].ResponseTime;
				shared_data->GammaIndex1 	= lightsettingsptr->data.AGC_Settings[0].GammaIndex;
				shared_data->AgcPLowThreshold1  = lightsettingsptr->data.AGC_Settings[0].AgcPLowThreshold;
				shared_data->AgcTargetLowThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcTargetLowThreshold;
				shared_data->AgcTargetHighThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcTargetHighThreshold;
				shared_data->AgcPHighThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcPHighThreshold;
				shared_data->shuttermax1 	= lightsettingsptr->data.AGC_Settings[0].shuttermax;
				shared_data->shuttermin1 	= lightsettingsptr->data.AGC_Settings[0].shuttermin;
				shared_data->gainmax1 		= lightsettingsptr->data.AGC_Settings[0].gainmax;
				shared_data->gainmin1 		= lightsettingsptr->data.AGC_Settings[0].gainmin;

				shared_data->AgcTargetLow 	= lightsettingsptr->data.Header.AgcTargetLow;
				shared_data->AvgSel2 		= lightsettingsptr->data.AGC_Settings[1].AvgSel;
				shared_data->ResponseTime2 	= lightsettingsptr->data.AGC_Settings[1].ResponseTime;
				shared_data->GammaIndex2 	= lightsettingsptr->data.AGC_Settings[1].GammaIndex;
				shared_data->AgcPLowThreshold2  = lightsettingsptr->data.AGC_Settings[1].AgcPLowThreshold;
				shared_data->AgcTargetLowThreshold2 = lightsettingsptr->data.AGC_Settings[1].AgcTargetLowThreshold;
				shared_data->AgcTargetHighThreshold2 = lightsettingsptr->data.AGC_Settings[1].AgcTargetHighThreshold;
				shared_data->AgcPHighThreshold2 = lightsettingsptr->data.AGC_Settings[1].AgcPHighThreshold;
				shared_data->shuttermax2 	= lightsettingsptr->data.AGC_Settings[1].shuttermax;
				shared_data->shuttermin2 	= lightsettingsptr->data.AGC_Settings[1].shuttermin;
				shared_data->gainmax2 		= lightsettingsptr->data.AGC_Settings[1].gainmax;
				shared_data->gainmin2 		= lightsettingsptr->data.AGC_Settings[1].gainmin;
				shared_data->DualCaptureEnabled = 1;
			}
			else
			{
				shared_data->AgcTargetHigh 	= lightsettingsptr->data.Header.AgcTargetHigh;
				shared_data->AvgSel1 		= lightsettingsptr->data.AGC_Settings[0].AvgSel;
				shared_data->ResponseTime1 	= lightsettingsptr->data.AGC_Settings[0].ResponseTime;
				shared_data->GammaIndex1 	= lightsettingsptr->data.AGC_Settings[0].GammaIndex;
				shared_data->AgcPLowThreshold1  = lightsettingsptr->data.AGC_Settings[0].AgcPLowThreshold;
				shared_data->AgcTargetLowThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcTargetLowThreshold;
				shared_data->AgcTargetHighThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcTargetHighThreshold;
				shared_data->AgcPHighThreshold1 = lightsettingsptr->data.AGC_Settings[0].AgcPHighThreshold;
				shared_data->shuttermax1 	= lightsettingsptr->data.AGC_Settings[0].shuttermax;
				shared_data->shuttermin1 	= lightsettingsptr->data.AGC_Settings[0].shuttermin;
				shared_data->gainmax1 		= lightsettingsptr->data.AGC_Settings[0].gainmax;
				shared_data->gainmin1 		= lightsettingsptr->data.AGC_Settings[0].gainmin;
				shared_data->DualCaptureEnabled = 0;
			}
		}
		
		if(flashEnabled != shared_data->flashEnabled)
		{
			flashEnabled = shared_data->flashEnabled;
			driveGpio200(flashEnabled);// flash en
		}
		
		if(IcrControl != shared_data->IcrControl)
		{
			IcrControl = shared_data->IcrControl;
			driveICRDIR(IcrControl);
			driveICREN(0);
			driveICREN(1);
		}
	}

/*	if(NightTime)
	{
		DEBUG_PRINT("Light Settings: Night %d lux%d prevlux%d - apply%d table%d Dual%d Gn0-%d Gn1-%d Gm0-%d Gm1-%d S0-%d S1-%d Fl-%d\n",  
		NightTime, shared_data->lux, prevLux, j, shared_data->crntLightTable, shared_data->DualCaptureEnabled, shared_data->GainIndex1, shared_data->GainIndex2, shared_data->GammaIndex1, shared_data->GammaIndex2, shared_data->ShutterIndex1, shared_data->ShutterIndex2, flashEnabled);
	}
	else
	{
		DEBUG_PRINT("Light Settings: AGC lux%d Dual%d Bt0-%d Bt1-%d Rs0-%d Rs1-%d \n", shared_data->lux, shared_data->DualCaptureEnabled, shared_data->AgcTargetHigh, shared_data->AgcTargetLow, shared_data->ResponseTime1, shared_data->ResponseTime2);
	}
	DEBUG_PRINT("no ch hr=%d lux%d- flag-%d timer%d, daytime start-%d,end-%d lux start-%d,end-%d\n", shared_data->rtctimecode[2], lux, NightTime, inactivity_timer, 
	lightsettingsptr->data.Night_Settings.NightStart,  lightsettingsptr->data.Night_Settings.NightEnd, 
	lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue, lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue);
*/	return (NightTime);
}

/********************************************************************************************/
// Function to decrypt AES-GCM
static int decrypt(unsigned char *indata, int indata_len, unsigned char *key, unsigned char *outdata) 
{
	EVP_CIPHER_CTX *ctx;
	const char head[6] = "ZRTC";
	unsigned char iv[GCM_IV_SIZE];
	unsigned char tag[GCM_TAG_SIZE];
	unsigned char header[32];
	int crlen;
	int crsize;
	int len, inlen;
	const char *aad = "Trois_CFGEnc.Tag"; 
	int aad_len = 16;

	crlen = -1;
	mempcpy(header, indata, 32);
	if((header[0] == head[0]) && (header[1] == head[1]) && (header[2] == head[2]) && (header[3] == head[3]))
	{
		mempcpy(tag, &header[4], GCM_TAG_SIZE);
		mempcpy(iv, &header[4+GCM_TAG_SIZE], GCM_IV_SIZE);
		// Create and initialize the context
		if (!(ctx = EVP_CIPHER_CTX_new()))
		{
			printf("EVP_CIPHER_CTX_new failed");
			goto exit_dec;
		}

		// Initialize decryption operation with AES-128-GCM
		if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
		{
			printf("EVP_DecryptInit_ex failed");
			goto cleanup_dec;
		}

		// Set IV length if default 12 bytes is not used
		if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, NULL))
		{
			printf("EVP_CIPHER_CTX_ctrl SET_IVLEN failed");
			goto cleanup_dec;
		}

		// Initialize key and IV
		if (1 != EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) 
		{
			printf("EVP_DecryptInit_ex key/iv failed");
			goto cleanup_dec;
		}

		// Provide AAD data
		if (1 != EVP_DecryptUpdate(ctx, NULL, &len, (unsigned char *)aad, aad_len)) 
		{
			printf("EVP_DecryptUpdate AAD failed");
			goto cleanup_dec;
		}
		crlen = 0;
		inlen = 32;
		indata_len -= 32;
		// Decrypt the ciphertext
		while(indata_len > 0)
		{
			len = 0;
			if(indata_len > GCM_MAX_ENC_BLOCK_SIZE_ENC)
			{
				crsize = GCM_MAX_ENC_BLOCK_SIZE_ENC;
			}
			else
			{
				crsize = indata_len;
			}
			if (1 != EVP_DecryptUpdate(ctx, &outdata[crlen], &len, &indata[inlen], crsize))
			{
				printf("EVP_DecryptUpdate ciphertext failed");
				crsize = 0;
				break;
			}
			indata_len -= crsize;
			inlen += crsize;
			crlen = crlen + len;
		}
		if(crsize == 0)
		{
			goto cleanup_dec;
		}
		// Set the expected authentication tag
		if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, tag)) 
		{
			printf("EVP_CIPHER_CTX_ctrl SET_TAG failed");
			crsize = 0;
			goto cleanup_dec;
		}
		len = 0;
		// Finalize decryption
		crsize = EVP_DecryptFinal_ex(ctx, outdata + crlen, &len);
		if (crsize > 0) 
			crlen += len;

cleanup_dec:		
		EVP_CIPHER_CTX_free(ctx);
		if (crsize > 0) 
		{
			return crlen; // Success
		} 
		else 
		{
			printf("EVP_DecryptFinal_ex failed");
			return -1; // Verification failed
		}
	}
exit_dec:
	return crlen; // result
}

// Function to encrypt AES-GCM
static int encrypt(unsigned char *indata, int indata_len, unsigned char *key, unsigned char *outdata)
{
	EVP_CIPHER_CTX *ctx;
	const char head[6] = "ZRTC";
	unsigned char iv[GCM_IV_SIZE];
	unsigned char tag[GCM_TAG_SIZE];
	unsigned char header[32];
	int len, inlen, crsize, crlen;
	FILE *fptr;
	const char *aad = "Trois_CFGEnc.Tag"; 
	int aad_len = 16;
	
	crlen = -1;
	fptr = popen("pkcs11-tool --module /usr/lib/libckteec.so --generate-random 24 > t24.bin", "r");// will execute the commands in string..
	pclose(fptr);// close will wait for the process to terminate and return..

	fptr =fopen("t24.bin","rb+");
	if(fptr != NULL)
	{
		len = fread(header, 1,7,fptr);
		len = fread(iv, 12,1,fptr);
		fseek(fptr, 0, SEEK_SET);
		memset(header, 0, 32);
		fwrite(header, 8,4,fptr);
		fclose(fptr);
	}
	else
	{
		goto exitenc;
	}
	len = 0;
	// Create and initialize context
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
	{
		printf("EVP_CIPHER_CTX_new failed");
		goto exitenc;
	}

	// Initialize encryption operation with AES-256-GCM
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
	{
		printf("EVP_EncryptInit_ex failed");
		goto cleanup_enc;
	}

	// Set IV length if default (12 bytes) is not used
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, NULL) != 1)
	{
		printf("EVP_CIPHER_CTX_ctrl SET_IVLEN failed");
		goto cleanup_enc;
	}

	// Provide key and IV
	if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1)
	{
		printf("EVP_EncryptInit_ex key/iv failed");
		goto cleanup_enc;
	}


	// (Optional) Provide Additional Authenticated Data (AAD)
	if (EVP_EncryptUpdate(ctx, NULL, &len, (unsigned char *)aad, aad_len) != 1)
	{
		printf("EVP_EncryptUpdate AAD failed");
		goto cleanup_enc;
	}
		
	crlen = 32;
	inlen = 0;
	while(indata_len > 0)
	{
		len = 0;
		if(indata_len > GCM_MAX_ENC_BLOCK_SIZE)
		{
			crsize = GCM_MAX_ENC_BLOCK_SIZE;
		}
		else
		{
			crsize = indata_len;
		}
		if (EVP_EncryptUpdate(ctx, &outdata[crlen], &len, &indata[inlen], crsize) != 1)
		{
			printf("EVP_EncryptUpdate plaintext failed");
			crlen = 0;
			break;
		}
		inlen += crsize;
		indata_len -= crsize;
		// Encrypt plaintext
		crlen = crlen + len;
	}
	if(crlen == 0)
	{
		goto cleanup_enc;
	}
	len = 0;
	// Finalize encryption (GCM does not output more ciphertext here)
	if (EVP_EncryptFinal_ex(ctx, outdata + inlen, &len) != 1)
	{
		printf("EVP_EncryptFinal_ex failed");
		goto cleanup_enc;
	}
	crlen = crlen + len;

	// Get the authentication tag
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag) != 1)
	{
		printf("EVP_CIPHER_CTX_ctrl GET_TAG failed");
		goto cleanup_enc;
	}

	mempcpy(header, head, 4);
	mempcpy(&header[4], tag, GCM_TAG_SIZE);
	mempcpy(&header[4+GCM_TAG_SIZE], iv, GCM_IV_SIZE);
	mempcpy(outdata, header,32);
	printf("encrypted length: %d bytes\n", crlen);

cleanup_enc:
	// Clean up
	EVP_CIPHER_CTX_free(ctx);
	
exitenc:
	return crlen;
	
}

/********************************************************************************************/
void saveconfigdataenc(void)
{
	size_t len;

	if(fsconfigencstatus)
	{
		len=encrypt(configdata_rec, 32*1024, mtxcfgkey, configdata_enc);// return size of encrypted file..
		if(len <= 0)	
		{
			loganevent("config not saved..","encrypt err");
			return;// config not saved..
		}
	}
	else
	{
		if(safe_memcpy(configdata_enc,  32*1024, configdata_rec,  32*1024) <= 0)
			printf("memory not copied..\n");
		//mem cpy(configdata_enc, configdata_rec, 32*1024);
		len = 32*1024;
	}
	fsconfigfptr = fopen(FsConfigfile, "rb+");
	if(fsconfigfptr != NULL)
	{
		fseek(fsconfigfptr, 0, SEEK_SET);
		usleep(500);
		fwrite(configdata_enc, 1, len, fsconfigfptr);
		fsync(fileno(fsconfigfptr));
		fclose(fsconfigfptr);
	}
	//printf("----------------------------------------------------CONFIG FILE SVED.------------------------------------\n");
	return;
}
/********************************************************************************************/
/* helper function to validate PIN (numeric only, max 32 chars) */
static int validate_pin(const char *pin) 
{
	int len = safe_strlen(pin, 9);
	if (!pin || len == 0 || len > 32)
	return 0;
	for (const char *p = pin; *p; p++) 
	{
		if (!isdigit(*p))
		return 0;
	}
	return 1;
}

int readkeyfromsecstorage2(char *pin, unsigned char* key, unsigned char *iv)
{
	FILE *fptr;
	const char cmd1[64] 	= {"pkcs11-tool --module /usr/lib/libckteec.so --login --slot"};
	const char cmd2[16] 	= {"--pin"};
	const char cmd3[8] 	= {"--id"};
	const char cmd4[128] 	= {"--mechanism RSA-PKCS --decrypt --input-file /run/trois/bin/configenc.key.enc  --output-file /run/trois/bin/configenc.key"};
	char command[256] = {0};
	
	unsigned char buffer[1024] = {0};
	int i;
	
	memset(key, 0, 32);
	memset(iv, 0, 16);
	/* CRITICAL: Validate PIN before use */
	if (!validate_pin(pin)) 
	{
		printf("ERROR: Invalid PIN format\n");
		return -1;
	}
	snprintf(command, 225, "%s %01d %s %s %s %04d %s", cmd1, 1, cmd2, pin, cmd3, 132, cmd4);
	//snprintf(command, 225, "%s %01d %s %s %s %04d %s", cmd1, 1, cmd2, "00000000", cmd3, 132, cmd4);
	fptr = popen(command, "r");// will execute the commands in string..
	pclose(fptr);// close will wait for the process to terminate and return..
	fptr = fopen("/run/trois/bin/configenc.key", "rb+");
	if(fptr != NULL)
	{
		i = fread(buffer,1, 512, fptr);
		if((buffer[5] == 0x56) && (buffer[6] == 0x78))
		{
			if(safe_memcpy(key, 32, &buffer[30], 32) <= 0)
				printf("memory not copied..\n");
			//mem cpy(key, &buffer[30], 32);
			if(safe_memcpy(iv, 16, &buffer[76], 16) <= 0)
				printf("memory not copied..\n");
			//mem cpy(iv, &buffer[76], 16);
			if(safe_memcpy(Aeskey, 32, &buffer[96], 32) <= 0)
				printf("memory not copied..\n");
			//mem cpy(Aeskey, &buffer[96], 32);
			if(safe_memcpy(DecIV, 16, &buffer[128], 16) <= 0)
				printf("memory not copied..\n");
			//mem cpy(DecIV, &buffer[128], 16);
			memset(buffer, 0, 512);
			fseek(fptr, 0, SEEK_SET);
			i = fwrite(buffer, 1, 144, fptr);
			fclose(fptr);
			return 0;
		}
		memset(buffer, 0, 512);
		fseek(fptr, 0, SEEK_SET);
		i = fwrite(buffer, 1, 144, fptr);
		fclose(fptr);
	}
	return 1;
}
/********************************************************************************************/

int initI2Ceeprom()
{
	char i2cdev_name[32] = "/dev/i2c-1"; //NANO i2c-1 connected to PCF2129 and EEprom64Kbit
	FILE *fptr;
	unsigned int i, j;
	unsigned char buffer[4096] = {0};
/***************************************************************************/	
	fptr = fopen(FILENAME, "rb");
	if(fptr != NULL)
	{
		fread( buffer, 1, 4096, fptr);
		fclose(fptr);
	}
	j = 0;
	i = *(unsigned int*)&buffer[0x864];// second slot..
	//printf("i = %08x\n", i);
	if(i)
	{
		j = (i >> 1) % 100000000;
	}
	snprintf(keystr,15, "%08u", j);
// get pin from the system..
/***************************************************************************/	
	fsconfigencstatus = 0;
	if(readkeyfromsecstorage2(keystr, mtxcfgkey, mtxcfgiv) == 0)
		fsconfigencstatus = 1;
	fsconfigstatus = 0;
	fsconfigfptr = fopen(FsConfigfile, "rb");
	if(fsconfigfptr == NULL)
	{
		fsconfigstatus = 0;
	}
	else
	{
		fread(configdata_enc, 1, ((32*1024)+32), fsconfigfptr);
		if(fsconfigencstatus)
		{
			printf("decrypting config = %d\n", fsconfigencstatus);
			decrypt(configdata_enc, ((32*1024)+32), mtxcfgkey, configdata_rec);
		}
		else
		{
			printf("Normal config = %d\n", fsconfigencstatus);
			if(safe_memcpy(configdata_rec,  32*1024, configdata_enc,  32*1024) <= 0)
				printf("memory not copied..\n");
			//mem cpy(configdata_rec, configdata_enc, 32*1024);
		}
		fclose(fsconfigfptr);
		sync();
		fsconfigstatus = 1;
	}
//	printf("EEPROM INIT.. fsconfig status = %d\n", fsconfigstatus);
	if(fsconfigstatus == 0)
	{
		eepromfd = open(i2cdev_name, O_RDWR | O_SYNC);
		if(eepromfd < 0)
		{
//			printf("i2c_dev-%s not opened\n",i2cdev_name);
			return -1;
		}
		return eepromfd;
	}
	else
		return 0;
}

void closeI2Ceeprom()
{
	if(eepromfd != 0)
		close(eepromfd);
}

static int eeprom_write(int fd, unsigned int slave_addr, unsigned int offset, unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i, j;
  unsigned char _buf[MAX_BYTES];

  i = 0;
  _buf[i++] = offset>>8; //_buf[0] is the offset addr MSB!
  _buf[i++] = offset;    //_buf[1] is the offset addr LSB!
  len += 2;

  for(j=0;i<len;i++) // i presetted, copy buf[0..n] -> _buf[1..n+1] 
    _buf[i]=buf[j++];

  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;

  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = 0;
  i2cmsg.len   = len;
  i2cmsg.buf   = _buf;
  
again:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
     if(i == -1&& (errno==EINTR||errno==121))
     {
/*#if DEBUG
       printf("i2c error EINTR\n");
#endif*/
	//DEBUG_PRINT("i2c write error EINTR\n");
       goto again;
     }
     else
     {
	//printf("retval=%d:errno=%d:i2c Data Write Error \n",i,errno);
	//DEBUG_PRINT("retval=%d:errno=%d:i2c Data Write Error \n",i,errno);
/*#if LANDEBUG    
    DEBUGCPY("retval=%d:errno=%d:i2c Data Write Error\n")
    DEBUGPRINT(debug_buffer,i,errno)
#endif */
	return -1;
     }
  }  
  return 0;
}


static int eeprom_addr_write(int fd,unsigned int slave_addr, unsigned int offset)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;
  int i = 0;
  unsigned char _buf[8];

  _buf[0] = (unsigned char)offset>>8; //_buf[0] is the offset addr MSB!
  _buf[1] = (unsigned char)offset;    //_buf[1] is the offset addr LSB!

  msg_rdwr.msgs = &i2cmsg;
  msg_rdwr.nmsgs = 1;

  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = 0;
  i2cmsg.len   = 2;
  i2cmsg.buf   = _buf;

againADRWR:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
    if(i == -1&& (errno==EINTR||errno==121))
    {
      goto againADRWR;
    }
    else
    {
      return -1;
    }
  }
  return 0;
}

static int eeprom_read(int fd, unsigned int slave_addr, unsigned int offset,unsigned char *buf, unsigned char len)
{
  struct i2c_rdwr_ioctl_data msg_rdwr;
  struct i2c_msg i2cmsg;  
  int i;

  // first write the addr to the slave device 
  if(eeprom_addr_write(fd,slave_addr,offset)<0)
    return -1;

  msg_rdwr.msgs = &i2cmsg;  
  msg_rdwr.nmsgs = 1;
  
  i2cmsg.addr  = slave_addr;
  i2cmsg.flags = I2C_RD;
  i2cmsg.len   = len;
  i2cmsg.buf   = buf;

again:
  if((i=ioctl(fd,I2C_RDWR,&msg_rdwr))<0)
  {
    if(i == -1&& (errno==EINTR||errno==121))
    {
/*#if DEBUG
      printf("i2c error EINTR\n");
#endif*/
	//DEBUG_PRINT("i2c eeprom_read error EINTR\n"); 
      goto again;
    }
    else
    {
//      printf("retval=%d:errno=%d:i2c Read Error\n",i,errno);
      //DEBUG_PRINT("retval=%d:errno=%d:i2c Read Error \n",i,errno);
/*#if LANDEBUG    
    DEBUGCPY("retval=%d:errno=%d:i2c Read Error\n")
    DEBUGPRINT(debug_buffer,i,errno)
#endif         */
      return -1;
    }
  }  
  return 0;
}

void read_from_eeprom(void)
{
//  printf ("reading %s... \n",ev.display_string);
  _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64, ev.start_address, ev.main_offset, ev.length);
}

void read_general_details(void)
{
  init_general_details();
  read_from_eeprom();
}

void read_camera_parameters(void)
{
  init_camera_parameters();
  read_from_eeprom();
}
void read_womensafety_camera_parameters(void)
{
  init_womensafety_camera_parameters();
  read_from_eeprom();
}
void read_remote_user_data(void)
{
  init_remote_user_data();
  read_from_eeprom();
}

void read_ip_details(void)
{
  init_ip_details();
  read_from_eeprom();
}

void read_dydns_data(void)
{
  init_dydns();
  read_from_eeprom();
}

void read_alarm_data(void)
{
  init_alarm();
  read_from_eeprom();
}

void read_rtsp_details(void)
{
  init_rtsp();
  read_from_eeprom();
}

void read_wb_details(void)
{
  init_wb();
  read_from_eeprom();  
}

void save_general_details (void)
{
  init_general_details();
//	printf("gen details quality = %d\n", new_general_details.primary_stream_quality[0]);
  save_to_eeprom();
}
void save_womensafety_camera_parameters(void)
{
  init_womensafety_camera_parameters();
  save_to_eeprom();
}
void save_camera_parameters(void)
{
  init_camera_parameters();
  save_to_eeprom();
}

void save_remote_user_data(void)
{
  init_remote_user_data();
  save_to_eeprom();
}

void save_ip_details (void )
{
  init_ip_details();
  save_to_eeprom();
}

void save_dydns_data (void)
{
  init_dydns();
  save_to_eeprom();
}

void save_alarm_data (void)
{
  init_alarm();
  save_to_eeprom();
}

void save_rtsp_details (void)
{
  init_rtsp();
  save_to_eeprom();
}

void save_wb_details (void)
{
  init_wb();
  save_to_eeprom();
}

void save_checksum(void)
{
//  printf ("saving checksum... \n");
  _WriteToFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&new_checksum,(unsigned char *)&checksum, CHECKSUM_OFFSET, sizeof(struct chsm),0);
  safe_memcpy(&checksum, sizeof(struct chsm), &new_checksum, sizeof(struct chsm));
  //mem cpy(&checksum, &new_checksum, sizeof(struct chsm));
}
void save_no_cs_data(void)
{
//  printf ("saving no checksum data... \n");
//  _WriteToFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&new_no_checksum_data,(unsigned char *)&no_checksum_data, NO_CHECKSUM_OFFSET, sizeof(struct nocs),0);
}

void save_to_eeprom (void)
{
//  printf ("saving %s... \n",ev.display_string);

  *ev.main_checksum   = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64,ev.new_start_address, ev.start_address, ev.main_offset,   ev.length, 0);
  *ev.mirror_checksum = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64,ev.new_start_address, ev.start_address, ev.mirror_offset, ev.length, 0);
  save_checksum();
  save_no_cs_data();
}

void save_programmed_data(void)
{
 
  _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&checksum, CHECKSUM_OFFSET, sizeof(struct chsm));

  read_general_details();
  read_camera_parameters();
  read_remote_user_data();
  read_ip_details();
  read_dydns_data();
  read_alarm_data();
  read_rtsp_details();
  read_wb_details();
  
//logSharedConfigData();

  save_general_details();
  save_camera_parameters();
  save_remote_user_data();
  save_ip_details();
  save_dydns_data();
  save_alarm_data();
  save_rtsp_details();
  save_wb_details();
  save_no_cs_data();
}
void save_womensafety_programmed_data(void)
{
//  _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&womensafety_checksum, WOMEN_SAFETY_CHECKSUM_OFFSET, sizeof(struct womensafety_chsm));
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;

	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&new_womensafety_camera_parameters[0]);
	read_womensafety_camera_parameters();
	if((lightsettingsptr->data.Header.marker1 == AICAM_AGC_HDR_M1) && (lightsettingsptr->data.Header.marker2 == AICAM_AGC_HDR_M2))
	{
		save_womensafety_camera_parameters();
	}
	else
	{
		new_checksum.mr_womensafety_cam_checksum = new_checksum.womensafety_cam_checksum + 10;
		new_checksum.womensafety_cam_checksum = new_checksum.mr_womensafety_cam_checksum - 5;
		read_womensafety_camera_parameters();
		save_womensafety_camera_parameters();
	}
}
void verify_general_details (void)
{
  init_general_details();
  verify_eeprom();
}

void verify_camera_parameters (void)
{
  init_camera_parameters();
  verify_eeprom();
}
void verify_womensafety_camera_parameters (void)
{
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;
	
	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&new_womensafety_camera_parameters[0]);
	init_womensafety_camera_parameters();
	verify_eeprom();
	if((lightsettingsptr->data.Header.marker1 != AICAM_AGC_HDR_M1) || (lightsettingsptr->data.Header.marker2 != AICAM_AGC_HDR_M2))
	{
		new_checksum.mr_womensafety_cam_checksum = new_checksum.womensafety_cam_checksum + 10;
		new_checksum.womensafety_cam_checksum = new_checksum.mr_womensafety_cam_checksum - 5;
		init_womensafety_camera_parameters();
		verify_eeprom();
	}
}

void verify_remote_user_data (void)
{
  init_remote_user_data();
  verify_eeprom();
}

void verify_ip_details (void)
{
  init_ip_details();
  verify_eeprom();
}

void verify_dydns_data (void)
{
  init_dydns();
  verify_eeprom();
}

void verify_alarm_data (void)
{
  init_alarm();
  verify_eeprom();
}

void verify_rtsp_details (void)
{
  init_rtsp();
  verify_eeprom();
}

void verify_wb_details (void)
{
  init_wb();
  verify_eeprom();
}

void verify_nocs_data (void)
{
/*  _ReadFromFRAM (_24LC64,I2C_ADDR_24LC64,(unsigned char *)&no_checksum_data, NO_CHECKSUM_OFFSET, sizeof(struct nocs));
  mem cpy((unsigned char *)&new_no_checksum_data,(unsigned char *)&no_checksum_data,sizeof(struct nocs));	
  
  printf("no checksum data: saving data\n");
  _WriteToFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&new_no_checksum_data,(unsigned char *)&no_checksum_data,NO_CHECKSUM_OFFSET, sizeof(struct nocs),0);
  printf("no checksum data: ok\n");*/
}
void verify_eeprom(void)
{

  unsigned short 	calculated_main_cs=0;
  unsigned short	calculated_mirror_cs=0;

  unsigned char	main_csok;
  unsigned char 	mirror_csok;
	
  unsigned char	main_buffer  [4096];
  unsigned char	mirror_buffer[4096];

  main_csok 	= 0;
  mirror_csok 	= 0;

  calculated_main_cs 	= _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,main_buffer,ev.main_offset,ev.length);

//  printf("calc_main cs %x \n",calculated_main_cs);
  
//  printf("ee_main cs %x \n",*ev.main_checksum);
	
  if(calculated_main_cs == *ev.main_checksum)
  { 
    safe_memcpy(ev.new_start_address, ev.length, main_buffer, ev.length);
    //mem cpy(ev.new_start_address, main_buffer, ev.length);
//    printf("%s: ok\n",ev.display_string);
/*    *ev.mirror_checksum = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64, ev.new_start_address, main_buffer, ev.mirror_offset, ev.length, 1);
    save_checksum();*/
    return;    
  }
  else
  {
    calculated_mirror_cs = _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,mirror_buffer,ev.mirror_offset,ev.length);
//    printf("calc_mirror cs %x \n",calculated_mirror_cs);
//    printf("ee_mirror cs %x \n",*ev.mirror_checksum); 

    if(calculated_mirror_cs == *ev.mirror_checksum)
    {
      safe_memcpy(ev.new_start_address, ev.length, mirror_buffer, ev.length);
      //memc py(ev.new_start_address, mirror_buffer, ev.length);
//      printf("%s: copying from mirror to main\n",ev.display_string);
      *ev.main_checksum = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64, ev.new_start_address, mirror_buffer, ev.main_offset, ev.length, 1);
      save_checksum();
      return;      
    }
    else
    {
      ev.default_eeprom();
  //    printf("%s: loading defaults\n",ev.display_string);
      *ev.main_checksum   = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64, ev.new_start_address, main_buffer,   ev.main_offset,   ev.length, 1);
      *ev.mirror_checksum = _WriteToFRAM(_24LC64,I2C_ADDR_24LC64, ev.new_start_address, mirror_buffer, ev.mirror_offset, ev.length, 1);
      save_checksum();    
      save_no_cs_data();    
      return;     
    }
  } 
}

void getSharedConfigData(void * data)
{
	SHARED_CONFIG_DATA *sharedConfigData;

	sharedConfigData = (SHARED_CONFIG_DATA *)data;
	safe_memcpy(&sharedConfigData->general_details, 		sizeof(struct gnrl),			&general_details, 		sizeof(struct gnrl));
	safe_memcpy( sharedConfigData->camera_parameters,		sizeof(struct cam)*4,			camera_parameters, 		sizeof(struct cam)*4);
	safe_memcpy(&sharedConfigData->ip_details, 			sizeof(struct ip),			&ip_details, 			sizeof(struct ip));
	safe_memcpy( sharedConfigData->remote_user, 			sizeof(struct remo)*MAX_REMOTE_USER,	remote_user, 			sizeof(struct remo)*MAX_REMOTE_USER);
	safe_memcpy(&sharedConfigData->no_checksum_data, 		sizeof(struct nocs),			&no_checksum_data, 		sizeof(struct nocs));
	safe_memcpy(&sharedConfigData->write_protected_data, 		sizeof(struct wrpr),			&write_protected_data, 		sizeof(struct wrpr));
	safe_memcpy( sharedConfigData->schedule, 			sizeof(struct sch)*MAX_SCH_TIMING*MAX_SCH_CHANGES, schedule, 		sizeof(struct sch)*MAX_SCH_TIMING*MAX_SCH_CHANGES);
	safe_memcpy(&sharedConfigData->alarm_data, 			sizeof(struct alrm),			&alarm_data, 			sizeof(struct alrm));
	safe_memcpy(&sharedConfigData->rtsp_details, 			sizeof(struct rtsp_stream),		&rtsp_details, 			sizeof(struct rtsp_stream));
	safe_memcpy(&sharedConfigData->wb_details, 			sizeof(struct wb),			&wb_details, 			sizeof(struct wb));
	safe_memcpy(&sharedConfigData->dydns_data, 			sizeof(struct dydns),			&dydns_data, 			sizeof(struct dydns));
	safe_memcpy( sharedConfigData->womensafety_camera_parameters, 	sizeof(struct womensafety_cam)*65,	womensafety_camera_parameters,	sizeof(struct womensafety_cam)*65);
};

void verify_womensafety_fram (void)
{
//  unsigned char result=0;
  
//  _ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&womensafety_checksum, WOMEN_SAFETY_CHECKSUM_OFFSET, sizeof(struct womensafety_chsm));
//  mem cpy(&new_womensafety_checksum, &womensafety_checksum, sizeof(struct womensafety_chsm));
  
  
  verify_womensafety_camera_parameters();  
	
  safe_memcpy(&womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65, &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);
  //memc py(&womensafety_camera_parameters[0], &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);
//  printf("womensafety fram init over\n");
}

void copyfsconfig()
{
	unsigned char 	fsconfig_mem[64*1024];
	unsigned short read_cs,calculated_cs,i=0;
	unsigned char buffer[sizeof(struct wrpr)];

	memset(fsconfig_mem, 0, sizeof(fsconfig_mem));
//	printf("===================================================================\n");
//	printf("eeprom cpy to file......");
	calculated_cs	= _ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&buffer[0],WRPR_OFFSET, sizeof(struct wrpr));
	_ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&read_cs,WRPR_CHECKSUM_OFFSET, sizeof(unsigned short));
	
//	mem cpy(&fsconfig_mem[WRPR_OFFSET],		buffer, 				sizeof(struct chsm));
//	mem cpy(&fsconfig_mem[WRPR_CHECKSUM_OFFSET], 	&read_cs, 				sizeof(unsigned short));
	
	safe_memcpy(&fsconfig_mem[CHECKSUM_OFFSET], 		sizeof(struct chsm),			&new_checksum, 				sizeof(struct chsm));
	safe_memcpy(&fsconfig_mem[GENERAL_DETAILS_OFFSET], 	sizeof(struct gnrl),			&general_details, 			sizeof(struct gnrl));
	safe_memcpy(&fsconfig_mem[CAMERA_PARAMETERS_OFFSET], 	(sizeof(struct cam))*1,			&camera_parameters[0], 			(sizeof(struct cam))*1);
	safe_memcpy(&fsconfig_mem[REMOTE_USER_OFFSET], 		(sizeof(struct remo))*MAX_REMOTE_USER,	&remote_user[0], 			(sizeof(struct remo))*MAX_REMOTE_USER);
	safe_memcpy(&fsconfig_mem[IP_DETAILS_OFFSET], 		sizeof(struct ip), 			&ip_details, 				sizeof(struct ip));
	safe_memcpy(&fsconfig_mem[DYDNS_OFFSET], 		sizeof(struct dydns),			&dydns_data, 				sizeof(struct dydns));
	safe_memcpy(&fsconfig_mem[ALARM_OFFSET], 		sizeof(struct alrm),			&alarm_data, 				sizeof(struct alrm));
	safe_memcpy(&fsconfig_mem[RTSP_OFFSET], 		(sizeof(struct rtsp_stream)), 		&rtsp_details, 				(sizeof(struct rtsp_stream)));
	safe_memcpy(&fsconfig_mem[WB_OFFSET], 			(sizeof(struct wb)),			&wb_details, 				(sizeof(struct wb)));
	safe_memcpy(&fsconfig_mem[WOMEN_SAFETY_CAM_OFFSET], 	(sizeof(struct womensafety_cam))*65,	&womensafety_camera_parameters[0], 	(sizeof(struct womensafety_cam))*65);
 
	safe_memcpy(&fsconfig_mem[NO_CHECKSUM_OFFSET], 		sizeof(struct nocs),			&no_checksum_data,			sizeof(struct nocs));
	safe_memcpy(&fsconfig_mem[MR_GENERAL_DETAILS_OFFSET], 	sizeof(struct gnrl),			&general_details, 			sizeof(struct gnrl));
	safe_memcpy(&fsconfig_mem[MR_CAMERA_PARAMETERS_OFFSET],	(sizeof(struct cam))*1,			&camera_parameters[0],			(sizeof(struct cam))*1);
	safe_memcpy(&fsconfig_mem[MR_REMOTE_USER_OFFSET], 	(sizeof(struct remo))*MAX_REMOTE_USER,	&remote_user[0], 			(sizeof(struct remo))*MAX_REMOTE_USER);
	safe_memcpy(&fsconfig_mem[MR_IP_DETAILS_OFFSET], 	sizeof(struct ip),			&ip_details, 				sizeof(struct ip));
	safe_memcpy(&fsconfig_mem[MR_DYDNS_OFFSET], 		sizeof(struct dydns),			&dydns_data, 				sizeof(struct dydns));
	safe_memcpy(&fsconfig_mem[MR_ALARM_OFFSET], 		sizeof(struct alrm),			&alarm_data, 				sizeof(struct alrm));
	safe_memcpy(&fsconfig_mem[MR_RTSP_OFFSET], 		(sizeof(struct rtsp_stream)),		&rtsp_details, 				(sizeof(struct rtsp_stream)));
	safe_memcpy(&fsconfig_mem[MR_WB_OFFSET], 		(sizeof(struct wb)),			&wb_details, 				(sizeof(struct wb)));
	safe_memcpy(&fsconfig_mem[MR_WOMEN_SAFETY_CAM_OFFSET], 	(sizeof(struct womensafety_cam))*65,	&womensafety_camera_parameters[0], 	(sizeof(struct womensafety_cam))*65);
	
	fsconfigfptr = fopen(FsConfigfile, "wb");
	if(fsconfigfptr != NULL)
	{
		fwrite(fsconfig_mem, 4096, 16, fsconfigfptr);
		fsync(fileno(fsconfigfptr));
		fclose(fsconfigfptr);
	}
//	printf("     finished!  \n");
//	printf("===================================================================\n");
///	fsconfigfptr = fopen(FsConfigfile, "rb+");
//	if(fsconfigfptr != NULL)
//	{
//		fsconfigstatus = 1;
//	}
}
void backupfsconfig(int i)
{
	unsigned char 	fsconfig_mem[64*1024];
	unsigned short read_cs,calculated_cs;
	unsigned char buffer[sizeof(struct wrpr)];
	FILE *fptr;
	memset(fsconfig_mem, 0, sizeof(fsconfig_mem));
//	printf("===================================================================\n");
//	printf("backupfsconfig to file......");
//	calculated_cs	= _ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&buffer[0],WRPR_OFFSET, sizeof(struct wrpr));
//	_ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&read_cs,WRPR_CHECKSUM_OFFSET, sizeof(unsigned short));
	
//	mem cpy(&fsconfig_mem[WRPR_OFFSET],		buffer, 				sizeof(struct chsm));
//	mem cpy(&fsconfig_mem[WRPR_CHECKSUM_OFFSET], 	&read_cs, 				sizeof(unsigned short));
	
	safe_memcpy(&fsconfig_mem[CHECKSUM_OFFSET], 		sizeof(struct chsm),			&new_checksum, 				sizeof(struct chsm));
	safe_memcpy(&fsconfig_mem[GENERAL_DETAILS_OFFSET], 	sizeof(struct gnrl),			&general_details, 			sizeof(struct gnrl));
	safe_memcpy(&fsconfig_mem[CAMERA_PARAMETERS_OFFSET], 	(sizeof(struct cam))*1,			&camera_parameters[0], 			(sizeof(struct cam))*1);
	safe_memcpy(&fsconfig_mem[REMOTE_USER_OFFSET], 		(sizeof(struct remo))*MAX_REMOTE_USER,	&remote_user[0], 			(sizeof(struct remo))*MAX_REMOTE_USER);
	safe_memcpy(&fsconfig_mem[IP_DETAILS_OFFSET], 		sizeof(struct ip), 			&ip_details, 				sizeof(struct ip));
	safe_memcpy(&fsconfig_mem[DYDNS_OFFSET], 		sizeof(struct dydns),			&dydns_data, 				sizeof(struct dydns));
	safe_memcpy(&fsconfig_mem[ALARM_OFFSET], 		sizeof(struct alrm),			&alarm_data, 				sizeof(struct alrm));
	safe_memcpy(&fsconfig_mem[RTSP_OFFSET], 		(sizeof(struct rtsp_stream)), 		&rtsp_details, 				(sizeof(struct rtsp_stream)));
	safe_memcpy(&fsconfig_mem[WB_OFFSET], 			(sizeof(struct wb)),			&wb_details, 				(sizeof(struct wb)));
	safe_memcpy(&fsconfig_mem[WOMEN_SAFETY_CAM_OFFSET], 	(sizeof(struct womensafety_cam))*65,	&womensafety_camera_parameters[0], 	(sizeof(struct womensafety_cam))*65);
 
	safe_memcpy(&fsconfig_mem[NO_CHECKSUM_OFFSET], 		sizeof(struct nocs),			&no_checksum_data,			sizeof(struct nocs));
	safe_memcpy(&fsconfig_mem[MR_GENERAL_DETAILS_OFFSET], 	sizeof(struct gnrl),			&general_details, 			sizeof(struct gnrl));
	safe_memcpy(&fsconfig_mem[MR_CAMERA_PARAMETERS_OFFSET],	(sizeof(struct cam))*1,			&camera_parameters[0],			(sizeof(struct cam))*1);
	safe_memcpy(&fsconfig_mem[MR_REMOTE_USER_OFFSET], 	(sizeof(struct remo))*MAX_REMOTE_USER,	&remote_user[0], 			(sizeof(struct remo))*MAX_REMOTE_USER);
	safe_memcpy(&fsconfig_mem[MR_IP_DETAILS_OFFSET], 	sizeof(struct ip),			&ip_details, 				sizeof(struct ip));
	safe_memcpy(&fsconfig_mem[MR_DYDNS_OFFSET], 		sizeof(struct dydns),			&dydns_data, 				sizeof(struct dydns));
	safe_memcpy(&fsconfig_mem[MR_ALARM_OFFSET], 		sizeof(struct alrm),			&alarm_data, 				sizeof(struct alrm));
	safe_memcpy(&fsconfig_mem[MR_RTSP_OFFSET], 		(sizeof(struct rtsp_stream)),		&rtsp_details, 				(sizeof(struct rtsp_stream)));
	safe_memcpy(&fsconfig_mem[MR_WB_OFFSET], 		(sizeof(struct wb)),			&wb_details, 				(sizeof(struct wb)));
	safe_memcpy(&fsconfig_mem[MR_WOMEN_SAFETY_CAM_OFFSET], 	(sizeof(struct womensafety_cam))*65,	&womensafety_camera_parameters[0], 	(sizeof(struct womensafety_cam))*65);
	
	if(i == 0)
		fptr = fopen("/usr/Camera/bin/backupfsconfig0.bin", "wb");
	else
		fptr = fopen("/usr/Camera/bin/backupfsconfig1.bin", "wb");
	if(fptr != NULL)
	{
		fwrite(fsconfig_mem, 4096, 16, fptr);
		fsync(fileno(fptr));
		fclose(fptr);
	}
//	printf("     finished!  \n");
//	printf("===================================================================\n");
}

void verify_fram (void)
{
	unsigned char result=0;
	int i;
	char olddefaultname[9] = "GUEST-05";
  
	_ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&checksum, CHECKSUM_OFFSET, sizeof(struct chsm));
	safe_memcpy(&new_checksum, sizeof(struct chsm), &checksum, sizeof(struct chsm));
	//mem cpy(&new_checksum, &checksum, sizeof(struct chsm));
	//display_buffer((unsigned char*)&new_checksum, 32);

//	verify_nocs_data();
//	mem cpy(&no_checksum_data, &new_no_checksum_data, sizeof(struct nocs));

//	result=verify_write_protected_data();

	verify_general_details();  
	verify_camera_parameters();  
	verify_remote_user_data();  
	verify_ip_details();  
	verify_dydns_data();  
	verify_alarm_data();
	verify_rtsp_details();
	verify_wb_details();
	verify_womensafety_camera_parameters();
	//display_buffer((unsigned char *)&new_womensafety_camera_parameters[0], 32);
	for(i = 0; i < 8; i++)
	{
		if(new_remote_user[4].name[i] != olddefaultname[i])
		{
			break;
		}
	}
	if(i >= 7)
	{
		// username old.. change to new....
		new_checksum.remote_user_checksum = 0x55;
		new_checksum.mr_remote_user_checksum = 0xAA;
		verify_remote_user_data(); 
	}

	sync();
	
	safe_memcpy(&general_details, sizeof(struct gnrl), &new_general_details, sizeof(struct gnrl));
	//mem cpy(&general_details, &new_general_details, sizeof(struct gnrl));
	safe_memcpy(&camera_parameters[0], (sizeof(struct cam))*1, &new_camera_parameters[0], (sizeof(struct cam))*1);
	//mem cpy(&camera_parameters[0], &new_camera_parameters[0], (sizeof(struct cam))*1);
	safe_memcpy(&remote_user[0], (sizeof(struct remo))*MAX_REMOTE_USER, &new_remote_user[0], (sizeof(struct remo))*MAX_REMOTE_USER);
	//mem cpy(&remote_user[0], &new_remote_user[0], (sizeof(struct remo))*MAX_REMOTE_USER);
	safe_memcpy(&ip_details, sizeof(struct ip), &new_ip_details, sizeof(struct ip));
	//mem cpy(&ip_details, &new_ip_details, sizeof(struct ip));
	safe_memcpy(&dydns_data, sizeof(struct dydns), &new_dydns_data, sizeof(struct dydns));
	//mem cpy(&dydns_data, &new_dydns_data, sizeof(struct dydns));
	safe_memcpy(&alarm_data, sizeof(struct alrm), &new_alarm_data, sizeof(struct alrm));
	//mem cpy(&alarm_data, &new_alarm_data, sizeof(struct alrm));
	safe_memcpy(&rtsp_details,(sizeof(struct rtsp_stream)), &new_rtsp_details,(sizeof(struct rtsp_stream)));
	//mem cpy(&rtsp_details,&new_rtsp_details,(sizeof(struct rtsp_stream)));
	safe_memcpy(&wb_details,(sizeof(struct wb)),&new_wb_details,(sizeof(struct wb)));
	//mem cpy(&wb_details,&new_wb_details,(sizeof(struct wb)));
	safe_memcpy(&womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65, &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);
	//mem cpy(&womensafety_camera_parameters[0], &new_womensafety_camera_parameters[0], (sizeof(struct womensafety_cam))*65);

	write_protected_data.device_type = 34;
	general_details.device_type = 34;
	if(fsconfigstatus == 0)
	{
		copyfsconfig();
	}
//	else
//	{
//		backupfsconfig(1);
//	}
	//display_buffer((unsigned char *)&new_womensafety_camera_parameters[0], 32);
//	printf ("fram init over\n");
}

void verify_write_protected_data(void)
{
	unsigned short read_cs,calculated_cs,i=0;
	unsigned char buffer[sizeof(struct wrpr)];

	calculated_cs	= _ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&buffer[0],WRPR_OFFSET, sizeof(struct wrpr));
	_ReadFromFRAM(_24LC64_WP,I2C_ADDR_24LC64_WP,(unsigned char *)&read_cs,WRPR_CHECKSUM_OFFSET, sizeof(unsigned short));

	if (read_cs == calculated_cs)
	{
		safe_memcpy(&write_protected_data, sizeof(struct wrpr), &buffer[0], sizeof(struct wrpr));
		//mem cpy(&write_protected_data,&buffer[0],sizeof(struct wrpr));
//		printf("write protected data Checksum: ok\n");

	}
//	else
//	{
//		printf("write protected data Checksum error\n");
		// checksum failed
//	}
	return;
}

void init_general_details(void)
{
  ev.length 	   	= sizeof(struct gnrl);
  ev.main_offset 	= GENERAL_DETAILS_OFFSET;
  ev.mirror_offset   	= MR_GENERAL_DETAILS_OFFSET;
  ev.main_checksum   	= &new_checksum.general_details_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_general_details_checksum;
  ev.start_address	= (unsigned char *)&general_details;
  ev.new_start_address	= (unsigned char *)&new_general_details;
  ev.display_string  	= "general_details";
  ev.default_eeprom  	= default_general_details;
}

void init_camera_parameters(void)
{
  ev.length 	   	= (sizeof(struct cam))*1;
  ev.main_offset    	= CAMERA_PARAMETERS_OFFSET;
  ev.mirror_offset   	= MR_CAMERA_PARAMETERS_OFFSET;
  ev.main_checksum   	= &new_checksum.camera_parameters_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_camera_parameters_checksum;
  ev.start_address	= (unsigned char *)&camera_parameters[0];
  ev.new_start_address	= (unsigned char *)&new_camera_parameters[0];
  ev.display_string  	= "camera parameters";
  ev.default_eeprom  	= default_camera_parameters;
}
void init_womensafety_camera_parameters(void)
{
  ev.length 	   	= (sizeof(struct womensafety_cam))*65;
  ev.main_offset    	= WOMEN_SAFETY_CAM_OFFSET;
  ev.mirror_offset   	= MR_WOMEN_SAFETY_CAM_OFFSET;
  ev.main_checksum   	= &new_checksum.womensafety_cam_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_womensafety_cam_checksum;
  ev.start_address	= (unsigned char *)&womensafety_camera_parameters[0];
  ev.new_start_address	= (unsigned char *)&new_womensafety_camera_parameters[0];
  ev.display_string  	= "womensafety camera parameters";
  ev.default_eeprom  	= default_womensafety_camera_parameters;
}
void init_remote_user_data(void)
{
  ev.length 	   	= (sizeof(struct remo))*MAX_REMOTE_USER;
  ev.main_offset    	= REMOTE_USER_OFFSET;
  ev.mirror_offset   	= MR_REMOTE_USER_OFFSET;
  ev.main_checksum   	= &new_checksum.remote_user_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_remote_user_checksum;
  ev.start_address	= (unsigned char *)&remote_user[0];
  ev.new_start_address	= (unsigned char *)&new_remote_user[0];
  ev.display_string  	= "remote user data";
  ev.default_eeprom  	= default_remote_user_data;
}

void init_ip_details(void)
{
  ev.length 	   	= sizeof(struct ip);
  ev.main_offset    	= IP_DETAILS_OFFSET;
  ev.mirror_offset   	= MR_IP_DETAILS_OFFSET;
  ev.main_checksum   	= &new_checksum.ip_details_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_ip_details_checksum;
  ev.start_address	= (unsigned char *)&ip_details;
  ev.new_start_address	= (unsigned char *)&new_ip_details;
  ev.display_string  	= "ip details";
  ev.default_eeprom  	= default_ip_details;
}

void init_dydns(void)
{
  ev.length 	   	= sizeof(struct dydns);
  ev.main_offset    	= DYDNS_OFFSET;
  ev.mirror_offset   	= MR_DYDNS_OFFSET;
  ev.main_checksum   	= &new_checksum.dydns_data_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_dydns_data_checksum;
  ev.start_address	= (unsigned char *)&dydns_data;
  ev.new_start_address	= (unsigned char *)&new_dydns_data;
  ev.display_string  	= "dydns";
  ev.default_eeprom  	= default_dydns;
}

void init_alarm (void)
{
  ev.length 	   	= sizeof(struct alrm);
  ev.main_offset    	= ALARM_OFFSET;
  ev.mirror_offset   	= MR_ALARM_OFFSET;
  ev.main_checksum   	= &new_checksum.alarm_data_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_alarm_data_checksum;
  ev.start_address	= (unsigned char *)&alarm_data;
  ev.new_start_address	= (unsigned char *)&new_alarm_data;
  ev.display_string  	= "alarm";
  ev.default_eeprom  	= default_alarm;
}

void init_rtsp (void)
{
  ev.length 	   	= sizeof(struct rtsp_stream);
  ev.main_offset    	= RTSP_OFFSET;
  ev.mirror_offset   	= MR_RTSP_OFFSET;
  ev.main_checksum   	= &new_checksum.rtsp_stream_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_rtsp_stream_checksum;
  ev.start_address	= (unsigned char *)&rtsp_details;
  ev.new_start_address	= (unsigned char *)&new_rtsp_details;
  ev.display_string  	= "rtsp details";
  ev.default_eeprom  	= default_rtsp;
}

void init_wb (void)
{
  ev.length 	   		= sizeof(struct wb);
  ev.main_offset    	= WB_OFFSET;
  ev.mirror_offset   	= MR_WB_OFFSET;
  ev.main_checksum   	= &new_checksum.wb_checksum;
  ev.mirror_checksum 	= &new_checksum.mr_wb_checksum;
  ev.start_address	= (unsigned char *)&wb_details;
  ev.new_start_address	= (unsigned char *)&new_wb_details;
  ev.display_string  	= "wb details";
  ev.default_eeprom  	= default_wb;
}

void default_camera_parameters(void)
{
  unsigned char i = 0,j;

  memset(&new_camera_parameters[0],0,(sizeof(struct cam)) * 1);  
  
	new_camera_parameters[i].name[0]='C';
	new_camera_parameters[i].name[1]='A';
	new_camera_parameters[i].name[2]='M';
	new_camera_parameters[i].name[3]='E';
	new_camera_parameters[i].name[4]='R';
	new_camera_parameters[i].name[5]='A';
	new_camera_parameters[i].name[6]='-';
	new_camera_parameters[i].name[7]='1';
	new_camera_parameters[i].name[8]=0;
	new_camera_parameters[i].brightness= 0;
	new_camera_parameters[i].contrast	 = 30;
	new_camera_parameters[i].saturation= 22;	// 25;   sree
///----------------------------------------------------	audio    
	new_camera_parameters[i].speakervolume= 53;
	new_camera_parameters[i].micvolume= 2; 
///----------------------------------------------------	audio
    
///----------------------------------------------------	Motion Detection    
	for (j = 0; j < 60; j++)
	    new_camera_parameters[i].activity_data[j] = MOTION_DETECT_DISABLED;
	new_camera_parameters[i].activity_data[60] = LOW;
	new_camera_parameters[i].activity_data[61] = 0;
///----------------------------------------------------	Motion Detection 
///---------------------------------------------------- anpr 
	new_camera_parameters[i].Lane_or_Evidence = LANE_CAMERA1; 
	new_camera_parameters[i].FramesPerTrigger = 1;
	new_camera_parameters[i].DayNight_ColourMode = NORMAL_DN_MODE;
	new_camera_parameters[i].FocusAdjustDay = 8;
	new_camera_parameters[i].FocusAdjustNight = -8;        
///---------------------------------------------------- anpr
    
}


void default_womensafety_camera_parameters(void)
{
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;

	memset(&new_womensafety_camera_parameters[0],0,(sizeof(struct womensafety_cam)) * 65);  
	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&new_womensafety_camera_parameters[0]);

	lightsettingsptr->data.Header.marker1 = AICAM_AGC_HDR_M1;
	lightsettingsptr->data.Header.marker2 = AICAM_AGC_HDR_M2;
	lightsettingsptr->data.Header.EnabledFlag = 1;
	lightsettingsptr->data.Header.DualCapFlag = 0;
	lightsettingsptr->data.Header.AgcTargetLow = 80;
	lightsettingsptr->data.Header.AgcTargetLow = 80;

	lightsettingsptr->data.Header.CapSerialProtocol = 0;
	lightsettingsptr->data.Header.NightModeTrigflag = 0;
	
	lightsettingsptr->data.Night_Settings.NightModeLUXThresholdValue = 45;
	lightsettingsptr->data.Night_Settings.DayModeLUXThresholdValue = 55;
	lightsettingsptr->data.Night_Settings.NightStart = 19;
	lightsettingsptr->data.Night_Settings.NightEnd = 7;
	lightsettingsptr->data.Night_Settings.DualCaptureFlag = ON_FLAG;
	lightsettingsptr->data.Night_Settings.Gamma.First = 0;
	lightsettingsptr->data.Night_Settings.Gamma.Second = 0;
	lightsettingsptr->data.Night_Settings.Gain.First = 5;
	lightsettingsptr->data.Night_Settings.Gain.Second = 2;
	lightsettingsptr->data.Night_Settings.Shutter.First = 16;
	lightsettingsptr->data.Night_Settings.Shutter.Second = 5;

	lightsettingsptr->data.AGC_Settings[0].AvgSel = 1;
	lightsettingsptr->data.AGC_Settings[0].ResponseTime = 3;
	lightsettingsptr->data.AGC_Settings[0].GammaIndex = 0;
	lightsettingsptr->data.AGC_Settings[0].AgcPLowThreshold = 80;
	lightsettingsptr->data.AGC_Settings[0].AgcTargetLowThreshold = 96;
	lightsettingsptr->data.AGC_Settings[0].AgcTargetHighThreshold = 144;
	lightsettingsptr->data.AGC_Settings[0].AgcPHighThreshold = 160;
	lightsettingsptr->data.AGC_Settings[0].shuttermax = 1000;
	lightsettingsptr->data.AGC_Settings[0].shuttermin = 20;
	lightsettingsptr->data.AGC_Settings[0].gainmax = 240;
	lightsettingsptr->data.AGC_Settings[0].gainmin = 0;
	

	lightsettingsptr->data.AGC_Settings[1].AvgSel = 1;
	lightsettingsptr->data.AGC_Settings[1].ResponseTime = 3;
	lightsettingsptr->data.AGC_Settings[1].GammaIndex = 0;
	lightsettingsptr->data.AGC_Settings[1].AgcPLowThreshold = 40;
	lightsettingsptr->data.AGC_Settings[1].AgcTargetLowThreshold = 56;
	lightsettingsptr->data.AGC_Settings[1].AgcTargetHighThreshold = 104;
	lightsettingsptr->data.AGC_Settings[1].AgcPHighThreshold = 120;
	lightsettingsptr->data.AGC_Settings[1].shuttermax = 700;
	lightsettingsptr->data.AGC_Settings[1].shuttermin = 20;
	lightsettingsptr->data.AGC_Settings[1].gainmax = 60;
	lightsettingsptr->data.AGC_Settings[1].gainmin = 0;
	
}

void default_remote_user_data(void)
{
	unsigned char i, j;

	memset(&new_remote_user[0],0,(sizeof(struct remo)) * MAX_REMOTE_USER);

	for( i = 0; i < MAX_REMOTE_USER ;i++ )
	{
		new_remote_user[i].password[0] = '5';
		new_remote_user[i].password[1] = '5';
		new_remote_user[i].password[2] = '6';
		new_remote_user[i].password[3] = '6';
		new_remote_user[i].password[4] = '7';
		new_remote_user[i].password[5] = '7';
		new_remote_user[i].level = GUEST;
		new_remote_user[i].name[0] = 'G';
		new_remote_user[i].name[1] = 'u';
		new_remote_user[i].name[2] = 'e';
		new_remote_user[i].name[3] = 's';
		new_remote_user[i].name[4] = 't';
		new_remote_user[i].name[5] = '_';
		new_remote_user[i].name[6] = '0';
		new_remote_user[i].name[7] = 0x31+i;
		new_remote_user[i].name[8] = 0;
	}
	new_remote_user[0].name[0] = 'A';
	new_remote_user[0].name[1] = 'd';
	new_remote_user[0].name[2] = 'm';
	new_remote_user[0].name[3] = 'i';
	new_remote_user[0].name[4] = 'n';
	new_remote_user[0].name[5] = '_';
	new_remote_user[0].name[6] = '0';
	new_remote_user[0].name[7] = '1';
	new_remote_user[0].name[8] = 0;
	new_remote_user[0].level	= ADMIN;	
	new_remote_user[0].password[0] = '5';
	new_remote_user[0].password[1] = '7';
	new_remote_user[0].password[2] = '3';
	new_remote_user[0].password[3] = '8';
	new_remote_user[0].password[4] = '4';
	new_remote_user[0].password[5] = '6';

	new_remote_user[1].name[0] = 'U';
	new_remote_user[1].name[1] = 's';
	new_remote_user[1].name[2] = 'e';
	new_remote_user[1].name[3] = 'r';
	new_remote_user[1].name[4] = '_';
	new_remote_user[1].name[5] = 'L';
	new_remote_user[1].name[6] = '0';
	new_remote_user[1].name[7] = '1';
	new_remote_user[1].name[8] = 0;
	new_remote_user[1].level	= USER;	
	new_remote_user[1].password[0] = '3';
	new_remote_user[1].password[1] = '8';
	new_remote_user[1].password[2] = '5';
	new_remote_user[1].password[3] = '7';
	new_remote_user[1].password[4] = '6';
	new_remote_user[1].password[5] = '4';
}

void default_general_details(void)
{
  unsigned char i;
  
  memset(&new_general_details,0,sizeof(struct gnrl)); 
  
  new_general_details.primary_stream_resolution = EIGHT_MP;     
  new_general_details.primary_stream_type=MEDIA_JPEG;
  new_general_details.agc_mode=INDR_MODE;
///----------------------------------------------------	Motion Detection  
  new_general_details.primary_stream_enabled[0]=NORMAL;
///----------------------------------------------------	Motion Detection  
  new_general_details.primary_stream_fps[0]=HIGH;
  new_general_details.primary_stream_quality[0]=LOW;
  new_general_details.second_stream_enabled=ON;
  new_general_details.third_stream_enabled=OFF;
  new_general_details.second_stream_fps[0]=MEDIUM;
  new_general_details.second_stream_quality[0]=MEDIUM;  
  new_general_details.third_stream_quality[0]=MEDIUM;
  new_general_details.relay_out_polarity=ACTIVE_HIGH;  
  new_general_details.second_stream_resolution=RES640X368;
  new_general_details.third_stream_resolution=RES640X368;
  safe_strncpy (new_general_details.name,"8MMTXCAM", 8);
  new_general_details.name[8] = 0;
  new_general_details.vertical_flip=OFF;
  new_general_details.E2VTestMode=OFF;
  new_general_details.wb_mode=WB_MANUAL_MODE;
///////////////////////////////////////PinkChanges
  new_general_details.gamma_value=9;    
///////////////////////////////////////PinkChanges
  new_general_details.ir_intensity=0x78;
  new_general_details.antiflicker_mode=OFF;
  new_general_details.af_enable_hour=18;
  new_general_details.af_enable_minute=30;
  new_general_details.af_disable_hour=6;
  new_general_details.af_disable_minute=30;
  new_general_details.ir_off_threshold=20;
  new_general_details.ir_on_threshold=10;
  new_general_details.agc_shutter_ceil=SHUTTER_CEIL_40MS;
  new_general_details.MaxHiAvgBrightness=54;
  new_general_details.MaxLoAvgBrightness=51;
  new_general_details.MinHiAvgBrightness=37;
  new_general_details.MinLoAvgBrightness=34;
  new_general_details.EnableAdaptiveAGC=OFF;
}

void default_ip_details(void)
{
  memset(&new_ip_details,0,sizeof(struct ip));

  new_ip_details.ip_address[0]	= 192;
  new_ip_details.ip_address[1] 	= 168;
  new_ip_details.ip_address[2] 	= 134;
  new_ip_details.ip_address[3] 	= 223;

  new_ip_details.subnet_mask[0]	= 255;
  new_ip_details.subnet_mask[1]	= 255;
  new_ip_details.subnet_mask[2]	= 255;
  new_ip_details.subnet_mask[3]	= 0;

  new_ip_details.gateway[0]	= 192;
  new_ip_details.gateway[1] 	= 168;
  new_ip_details.gateway[2] 	= 134;
  new_ip_details.gateway[3] 	= 1;

  new_ip_details.port_num[0]	= '0';
  new_ip_details.port_num[1]	= '0';
  new_ip_details.port_num[2]	= '2';
  new_ip_details.port_num[3]	= '0';
	
  new_ip_details.ws_port_num[0]	= '0';
  new_ip_details.ws_port_num[1]	= '0';
  new_ip_details.ws_port_num[2]	= '9';
  new_ip_details.ws_port_num[3]	= '0';
  
  new_ip_details.h264rtsp_portnum[0]	= '9';
  new_ip_details.h264rtsp_portnum[1]	= '4';
  new_ip_details.h264rtsp_portnum[2]	= '5';
  new_ip_details.h264rtsp_portnum[3]	= '0';
  
  new_ip_details.jpgrtsp_portnum[0]	= '9';
  new_ip_details.jpgrtsp_portnum[1]	= '3';
  new_ip_details.jpgrtsp_portnum[2]	= '5';
  new_ip_details.jpgrtsp_portnum[3]	= '0';
}

void default_dydns(void)
{
  memset(&new_dydns_data,0,sizeof(struct dydns));
 
  new_dydns_data.enabled = OFF;
  
  safe_strncpy(new_dydns_data.serviceType,"dyndns", 6);
  new_dydns_data.serviceType[6] = 0;
  new_dydns_data.username[0] = 'u';
  new_dydns_data.username[1] = 's';
  new_dydns_data.username[2] = 'e';
  new_dydns_data.username[3] = 'r';
  new_dydns_data.username[4] = 0;
  new_dydns_data.password[0] = 'p';
  new_dydns_data.password[1] = 'a';
  new_dydns_data.password[2] = 's';
  new_dydns_data.password[3] = 's';
  new_dydns_data.password[4] = 'w';
  new_dydns_data.password[5] = 'd';
  new_dydns_data.password[6] = 0;
  new_dydns_data.hostname[0] = 'h';
  new_dydns_data.hostname[1] = 'o';
  new_dydns_data.hostname[2] = 's';
  new_dydns_data.hostname[3] = 't';
  new_dydns_data.hostname[4] = 0;
  
  new_dydns_data.dns1[0]= 0;
  new_dydns_data.dns1[1]= 0;
  new_dydns_data.dns1[2]= 0;
  new_dydns_data.dns1[3]= 0;

  new_dydns_data.dns2[0]= 0;
  new_dydns_data.dns2[1]= 0;
  new_dydns_data.dns2[2]= 0;
  new_dydns_data.dns2[3]= 0;  
  
}

void default_alarm (void)
{
  unsigned char i;

  memset(&new_alarm_data,0,sizeof(struct alrm));
  new_alarm_data.input_polarity = N_O;
  new_alarm_data.input_type	= MOMENTARY;
  new_alarm_data.duration	= 10;
  new_alarm_data.enabled	= ON;
}

void default_rtsp (void)
{
  unsigned char i,j;

  memset(&new_rtsp_details,0,(sizeof(struct rtsp_stream)));
  
  new_rtsp_details.media_type=MEDIA_H264;  
  new_rtsp_details.authentication=ON;
  new_rtsp_details.rtsp_sock_mode=RTSP_UNICAST;

  for (j = 0; j < 5; j++)
  {
    new_rtsp_details.rtsp_portnum[0]='1';
    new_rtsp_details.rtsp_portnum[1]='8';
    new_rtsp_details.rtsp_portnum[2]='8';
    new_rtsp_details.rtsp_portnum[3]='8';
    new_rtsp_details.rtsp_portnum[4]='8';
      
    new_rtsp_details.rtp_portnum[0]='0';
    new_rtsp_details.rtp_portnum[1]='0';
    new_rtsp_details.rtp_portnum[2]='5';
    new_rtsp_details.rtp_portnum[3]='5';
    new_rtsp_details.rtp_portnum[4]='4';  
  }   
  
}

void default_wb (void)
{
  memset(&new_wb_details,0,(sizeof(struct wb)));
  
  new_wb_details.wb_red_gain		= 1700; //620; sree	
  new_wb_details.wb_green1_gain		= 1000;	//500; sree
  new_wb_details.wb_green2_gain		= 1000;	//500; sree
  new_wb_details.wb_blue_gain		= 1900;	//768; sree
  
  new_wb_details.wb_red_offset=80;
  new_wb_details.wb_green1_offset=80;
  new_wb_details.wb_green2_offset=80;
  new_wb_details.wb_blue_offset=80;
}

void Config_rebuild()
{
	new_checksum.general_details_checksum 	+= 5;
	new_checksum.camera_parameters_checksum += 5;
	new_checksum.remote_user_checksum 	+= 5;
	new_checksum.ip_details_checksum 	+= 5;
	new_checksum.dydns_data_checksum 	+= 5;
	new_checksum.alarm_data_checksum 	+= 5;
	new_checksum.rtsp_stream_checksum 	+= 5;
	new_checksum.wb_checksum 		+= 5;
	new_checksum.womensafety_cam_checksum 	+= 5;


	new_checksum.mr_general_details_checksum 	+= 5;
	new_checksum.mr_camera_parameters_checksum 	+= 5;
	new_checksum.mr_remote_user_checksum 		+= 5;
	new_checksum.mr_ip_details_checksum 		+= 5;
	new_checksum.mr_dydns_data_checksum 		+= 5;
	new_checksum.mr_alarm_data_checksum 		+= 5;
	new_checksum.mr_rtsp_stream_checksum 		+= 5;
	new_checksum.mr_wb_checksum 			+= 5;
	new_checksum.mr_womensafety_cam_checksum 	+= 5;

/*
	new_checksum.general_details_checksum 	= ~new_checksum.general_details_checksum;
	new_checksum.camera_parameters_checksum = ~new_checksum.camera_parameters_checksum;
	new_checksum.remote_user_checksum 	= ~new_checksum.remote_user_checksum;
	new_checksum.ip_details_checksum 	= ~new_checksum.ip_details_checksum;
	new_checksum.dydns_data_checksum 	= ~new_checksum.dydns_data_checksum;
	new_checksum.alarm_data_checksum 	= ~new_checksum.alarm_data_checksum;
	new_checksum.rtsp_stream_checksum 	= ~new_checksum.rtsp_stream_checksum;
	new_checksum.wb_checksum 		= ~new_checksum.wb_checksum;
	new_checksum.womensafety_cam_checksum 	= ~new_checksum.womensafety_cam_checksum;


	new_checksum.mr_general_details_checksum 	= ~new_checksum.mr_general_details_checksum;
	new_checksum.mr_camera_parameters_checksum 	= ~new_checksum.mr_camera_parameters_checksum;
	new_checksum.mr_remote_user_checksum 		= ~new_checksum.mr_remote_user_checksum;
	new_checksum.mr_ip_details_checksum 		= ~new_checksum.mr_ip_details_checksum;
	new_checksum.mr_dydns_data_checksum 		= ~new_checksum.mr_dydns_data_checksum;
	new_checksum.mr_alarm_data_checksum 		= ~new_checksum.mr_alarm_data_checksum;
	new_checksum.mr_rtsp_stream_checksum 		= ~new_checksum.mr_rtsp_stream_checksum;
	new_checksum.mr_wb_checksum 			= ~new_checksum.mr_wb_checksum;
	new_checksum.mr_womensafety_cam_checksum 	= ~new_checksum.mr_womensafety_cam_checksum;
*/
	save_checksum();
	_ReadFromFRAM(_24LC64,I2C_ADDR_24LC64,(unsigned char *)&checksum, CHECKSUM_OFFSET, sizeof(struct chsm));
	safe_memcpy(&new_checksum, sizeof(struct chsm), &checksum, sizeof(struct chsm));
	//mem cpy(&new_checksum, &checksum, sizeof(struct chsm));
	//display_buffer((unsigned char*)&new_checksum, 32);

}
/*
void _EraseFRAM(int pipefd,int eeprom_offset)		
{	
	unsigned char 	buffer_erase[32];


	memset(buffer_erase,0,sizeof(buffer_erase));
	if(eeprom_offset <= 0x2000)
	{
		eeprom_write(eepromfd,I2C_ADDR_24LC64,eeprom_offset,buffer_erase,32);
		DeadDelayMS(8);
		//usleep(8000);
//		printf("Erasing FRAM at %X completed \n",eeprom_offset);
//		DEBUG_PRINT("Erasing FRAM at %X completed \n",eeprom_offset);
	}
}*/

static void config_file_wr(unsigned short offset, unsigned char *buf, unsigned short len)
{
/*	unsigned int i;
	i = offset;
//	printf("file write @ offset %d of len %d \n", offset, len);
	fsconfigfptr = fopen(FsConfigfile, "rb+");
	fseek(fsconfigfptr, i, SEEK_SET);
	usleep(500);
	fwrite(buf, 1, len, fsconfigfptr);
	fsync(fileno(fsconfigfptr));
	fclose(fsconfigfptr);
*/
	if(safe_memcpy(&configdata_rec[offset], (32*1024 - offset), buf, len) <= 0)
		printf("config update not done\n");
	//mem cpy(&configdata_rec[offset], buf, len);
	if(configWrite_trigger == 0)
	{
		configWrite_timer = 10;
		configWrite_trigger = 1;
	}
	//printf("---------Writing CONFIG FILE.----------------\n");
}

static void config_file_rd(unsigned short offset,unsigned char *buf, unsigned short len)
{
/*	unsigned int i;
	i = offset;
//	printf("file read @ offset %d of len %d \n", offset, len);
	fsconfigfptr = fopen(FsConfigfile, "rb+");
	fseek(fsconfigfptr, i, SEEK_SET);
	usleep(500);
	fread(buf, 1, len, fsconfigfptr);
	fclose(fsconfigfptr);*/
	if(safe_memcpy(buf, len, &configdata_rec[offset], len) <= 0)
		printf("config read not done\n");
	//mem cpy(buf, &configdata_rec[offset], len);
}

static unsigned short _ReadFromFRAM(unsigned char device,unsigned int slave_addr,unsigned char *destination, unsigned short offset, unsigned short length)
{
	
	int 		k;
	unsigned short 	checksum,i,z;
//	unsigned char 	buffer;

	z		= length;
	i		= 0x0000;
	checksum 	= 0x0000;

	if(fsconfigstatus == 0)
	{
		if(device == _24LC64)
		{
			if(eepromfd > 0)
			{
				while(length>64)	  
				{
					eeprom_read(eepromfd,slave_addr,offset+(i*64),destination+(i*64),64);
					length-=64;
					i++;
				}

				if(length<=64)
				{
					eeprom_read(eepromfd,slave_addr,offset+(i*64),destination+(i*64),length);
				}
				i=0;

				while (i < z)
				{
					checksum +=*(destination + i);
					i++;		
				}
			}
			else
			{
				memset(destination,0,length);
			}
		}
	}
	else
	{
		config_file_rd(offset, destination, length);
		i=0;
		while (i < z)
		{
			checksum +=*(destination + i);
			i++;		
		}
	}
	if((checksum + 0x10) == 0)
		checksum = 0;
	return (checksum + 0x10);
}



static unsigned short _WriteToFRAM(unsigned char device,unsigned int slave_addr, unsigned char *source, unsigned char *old_data, unsigned short offset, unsigned short length,unsigned char all)
{
	int 		k;
	unsigned short 	checksum,i, z;
//	unsigned char 	buffer;
//	unsigned char 	old_data_buffer;

	z		= length;
	i	 	= 0x0000;
	checksum 	= 0x0000;

	if(fsconfigstatus == 0)
	{
		if(device == _24LC64)
		{
			if(eepromfd > 0)
			{
				while(length>32)	  
				{
					eeprom_write(eepromfd,slave_addr,offset+(i*32),source+(i*32),32);
					length-=32;
					i++;
					DeadDelayMS(8);
					//usleep(8000);
				}

				if(length<=32)
				{
					eeprom_write(eepromfd,slave_addr,offset+(i*32),source+(i*32),length);
					DeadDelayMS(8);
					//usleep(8000);
				}
				i=0;
				while (i < z)
				{
					checksum += *(source + i);
					i++;		
				}
			}
		}
	}
	else
	{
		config_file_wr(offset, source, length);
		i=0;
		while (i < z)
		{
			checksum +=*(source + i);
			i++;		
		}
	}
	if((checksum + 0x10) == 0)
		checksum = 0;
	return (checksum + 0x10);
}

