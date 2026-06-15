/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <sys/ipc.h>
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "RTSPDefines.h"   

/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
SHM_RTSP_SERVER *JPEGShmImgPtr;
SHM_RTSP_SERVER *H264ShmImgPtr;
SHM_X264 *X264ShmImgPtr;
static int H264SeqCnt,JPEGSeqCnt;
int capcount = 0;
DEFINE_FILE
extern char* safe_strncpy( char* dest, const char* src, size_t count);

void ConfigureH264ServerShdMem(unsigned char resolution, unsigned char * userdataptr, unsigned char *ipdetails);
void ConfigureX264ServerSharedMemory(void);
void WriteToX264ShdMem(unsigned char *imgbuffer, unsigned long length);
/*
 *For producer
 */
void ConfigureRTSPServerShdMem(unsigned char resolution, unsigned char * userdataptr, unsigned char *ipdetails)
{
	
	void *JpegSharedMemPtr;
	int JpegSharedMemHandle,nRet;
	int i, j;
	T_CAMERA_REMO userdata[MAX_REMOTE_USER+1];
	struct ip *ip_info = (struct ip *)ipdetails;
	struct stat statbuf;
	
    	JpegSharedMemHandle = shm_open("rtsp_jpeg_shm.dat", O_RDWR, 0660);
	printf("RTSP Opened shm %s\n","rtsp_jpeg_shm.dat");
    	fstat(JpegSharedMemHandle, &statbuf);
    	JpegSharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, JpegSharedMemHandle, 0);  
    	JPEGShmImgPtr = (SHM_RTSP_SERVER*)JpegSharedMemPtr; 
    	
    	

	JPEGShmImgPtr->rtsp_config_details.fps=LOW;	///MEDIUM/LOW;	25/12/8
	JPEGShmImgPtr->rtsp_config_details.resolution=resolution;
	JPEGShmImgPtr->rtsp_config_details.rtsp_authentication=TRUE;
	JPEGShmImgPtr->rtsp_config_details.rtsp_mode=RTSP_UNICAST;

	JPEGShmImgPtr->rtsp_config_details.rtsp_portnum[0] = ip_info->jpgrtsp_portnum[0];
	JPEGShmImgPtr->rtsp_config_details.rtsp_portnum[1] = ip_info->jpgrtsp_portnum[1];
	JPEGShmImgPtr->rtsp_config_details.rtsp_portnum[2] = ip_info->jpgrtsp_portnum[2];
	JPEGShmImgPtr->rtsp_config_details.rtsp_portnum[3] = ip_info->jpgrtsp_portnum[3]+5;
	JPEGShmImgPtr->rtsp_config_details.rtsp_portnum[4]='\0';

	JPEGShmImgPtr->rtsp_config_details.onvif_portnum[0]='1';
	JPEGShmImgPtr->rtsp_config_details.onvif_portnum[1]='0';
	JPEGShmImgPtr->rtsp_config_details.onvif_portnum[2]='0';
	JPEGShmImgPtr->rtsp_config_details.onvif_portnum[3]='0';
	JPEGShmImgPtr->rtsp_config_details.onvif_portnum[4]='\0';

	safe_strncpy((char*)JPEGShmImgPtr->rtsp_config_details.rtsp_stream_name, "jpeg", 5);
	mempcpy(userdata, userdataptr, 5*32);
	for(i = 0, j=1; i< MAX_REMOTE_USER; i++, j++)
	{
		safe_strncpy(JPEGShmImgPtr->rtsp_config_details.remote_user_name[i], userdata[j].name, 9);
		mempcpy(JPEGShmImgPtr->rtsp_config_details.remote_user_password[i], userdata[j].password, 6);
	}

	safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.model_name,"MtxAICamera", 12);
	if(resolution==FIVE_MP)
	{
		safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.hardware_ID,"AICAM5MP", 9);
	}
	else
	{
		if(resolution==THREE_MP)
		{
			safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.hardware_ID,"AICAM3MP", 9);
		}
		else
		{
			if(resolution==EIGHT_MP)
			{
				safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.hardware_ID,"AICAM8MP", 9);
			}
		}
	}

	safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.serial_no,"00001", 6);
	safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.firmware_version,"F248",5);
	safe_strncpy ((char*)JPEGShmImgPtr->rtsp_config_details.manufacturer,"MEDIATRONIX", 12);
	
	ConfigureH264ServerShdMem(resolution, userdataptr, ipdetails);
	ConfigureX264ServerSharedMemory();
}	

void ConfigureH264ServerShdMem(unsigned char resolution, unsigned char * userdataptr, unsigned char *ipdetails)
{
	void *H264SharedMemPtr;
	int i, j;
	int H264SharedMemHandle,nRet;
	T_CAMERA_REMO userdata[MAX_REMOTE_USER+1];
	struct ip *ip_info = (struct ip *)ipdetails;
 	struct stat statbuf;
 	
   	H264SharedMemHandle = shm_open("rtsp_h264_shm.dat", O_RDWR, 0660);
	printf("RTSP Opened shm %s\n","rtsp_h264_shm.dat");
     	fstat(H264SharedMemHandle, &statbuf);
    	H264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, H264SharedMemHandle, 0);  
    	H264ShmImgPtr = (SHM_RTSP_SERVER*)H264SharedMemPtr; 
 	

	H264ShmImgPtr->rtsp_config_details.fps=LOW;	///MEDIUM/LOW;	25/12/8
	H264ShmImgPtr->rtsp_config_details.resolution=resolution;
	H264ShmImgPtr->rtsp_config_details.rtsp_authentication=TRUE;
	H264ShmImgPtr->rtsp_config_details.rtsp_mode=RTSP_UNICAST;

	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[0] = ip_info->h264rtsp_portnum[0];
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[1] = ip_info->h264rtsp_portnum[1];
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[2] = ip_info->h264rtsp_portnum[2];
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[3] = ip_info->h264rtsp_portnum[3]+5;

	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[4]='\0';

	H264ShmImgPtr->rtsp_config_details.onvif_portnum[0]='1';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[1]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[2]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[3]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[4]='\0';

	safe_strncpy((char*)H264ShmImgPtr->rtsp_config_details.rtsp_stream_name,"h264", 5);

	mempcpy(userdata, userdataptr, 5*32);
	for(i = 0, j = 1; i< MAX_REMOTE_USER; i++, j++)
	{
		safe_strncpy(H264ShmImgPtr->rtsp_config_details.remote_user_name[i], userdata[j].name, 9);
		mempcpy(H264ShmImgPtr->rtsp_config_details.remote_user_password[i], userdata[j].password, 6);
	}

	safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.model_name,"MtxAICamera",12);
	if(resolution==FIVE_MP)
	{
		safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM5MP", 9);
	}
	else
	{
		if(resolution==THREE_MP)
		{
			safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM3MP", 9);
		}
		else
		{
			if(resolution==EIGHT_MP)
			{
				safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM8MP", 9);
			}
		}
	}

	safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.serial_no,"00001", 6);
	safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.firmware_version,"F248", 5);
	safe_strncpy ((char*)H264ShmImgPtr->rtsp_config_details.manufacturer,"MEDIATRONIX", 12);


}
void WriteH264FrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length)
{
	int z;
	
	H264SeqCnt++;
	if((!H264ShmImgPtr->prev_start)&&(H264ShmImgPtr->start==1))
	{
		H264ShmImgPtr->prev_start=1;
		printf("Start Buffering H264 %d\n",H264SeqCnt);
	}

	if((H264ShmImgPtr->filled[H264ShmImgPtr->buff_cnt]==0)&&(H264ShmImgPtr->prev_start==1)&&(H264ShmImgPtr->start==1))
	{
		H264ShmImgPtr->counter[H264ShmImgPtr->buff_cnt]=H264SeqCnt;
		H264ShmImgPtr->size[H264ShmImgPtr->buff_cnt]=length;
		mempcpy((char *)H264ShmImgPtr->payload[H264ShmImgPtr->buff_cnt],imgbuffer,length);
		H264ShmImgPtr->filled[H264ShmImgPtr->buff_cnt]=1;

		H264ShmImgPtr->buff_cnt++;
		if(H264ShmImgPtr->buff_cnt>4)
		{
			H264ShmImgPtr->buff_cnt=0;
		}
	}

}

void WriteJPEGFrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length)
{
	int z;
	
	JPEGSeqCnt++;
	if((!JPEGShmImgPtr->prev_start)&&(JPEGShmImgPtr->start==1))
	{
		JPEGShmImgPtr->prev_start=1;
		printf("Start Buffering JPEG %d\n",JPEGSeqCnt);
	}

	if((JPEGShmImgPtr->filled[JPEGShmImgPtr->buff_cnt]==0)&&(JPEGShmImgPtr->prev_start==1)&&(JPEGShmImgPtr->start==1))
	{
		JPEGShmImgPtr->counter[JPEGShmImgPtr->buff_cnt]=JPEGSeqCnt;
		JPEGShmImgPtr->size[JPEGShmImgPtr->buff_cnt]=length;
		mempcpy((char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->buff_cnt],imgbuffer,length);
		JPEGShmImgPtr->filled[JPEGShmImgPtr->buff_cnt]=1;

		JPEGShmImgPtr->buff_cnt++;
		if(JPEGShmImgPtr->buff_cnt>4)
		{
			JPEGShmImgPtr->buff_cnt=0;
		}
		//JPEGShmImgPtr->total_cnt++;

	}
	
	WriteToX264ShdMem(imgbuffer,length);

}

void SetJPEGFPSToRTSPServerShdMem(unsigned char fps)
{
	JPEGShmImgPtr->rtsp_config_details.fps=fps;	///HIGH/MEDIUM/LOW;	11/8/6
}

void ConfigureX264ServerSharedMemory(void)
{  
	void *X264SharedMemPtr;
	int X264SharedMemHandle,nRet;

	struct stat statbuf;
	
    	X264SharedMemHandle = shm_open("rtsp_x264_shm.dat", O_RDWR, 0660);
    	fstat(X264SharedMemHandle, &statbuf);
    	X264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, X264SharedMemHandle, 0);  
    	X264ShmImgPtr = (SHM_X264*)X264SharedMemPtr; 
}
void WriteToX264ShdMem(unsigned char *imgbuffer, unsigned long length)
{
	if((X264ShmImgPtr->filled[X264ShmImgPtr->buff_cnt]==0))
	{
		X264ShmImgPtr->size[X264ShmImgPtr->buff_cnt]=length;
		X264ShmImgPtr->jpgframecnt[X264ShmImgPtr->buff_cnt]=capcount;
		mempcpy((char *)X264ShmImgPtr->payload[X264ShmImgPtr->buff_cnt],imgbuffer,length);
		X264ShmImgPtr->filled[X264ShmImgPtr->buff_cnt]=1;
		
		capcount++;
		
		X264ShmImgPtr->buff_cnt++;
		if(X264ShmImgPtr->buff_cnt>1)
		{
			X264ShmImgPtr->buff_cnt=0;
		}

	}
}






