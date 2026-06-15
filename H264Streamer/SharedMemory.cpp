/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/

/********************************************************************************************/
/*                          Includes	                                                    */
/********************************************************************************************/
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

#include "../CameraServer/MtxSrc/Src/MediaServer/MtxStreamer/RTSPDefines.h"   
/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
//extern unsigned char Live555H264FrameRate;
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
SHM_RTSP_SERVER *H264ShmImgPtr;
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
void ConfigureRTSPServerSharedMemory(void)
{  

	void *H264SharedMemPtr;
	int H264SharedMemHandle,nRet;

	struct stat statbuf;
	
    	H264SharedMemHandle = shm_open("/rtsp_h264_shm.dat", O_CREAT | O_RDWR, 0666);
    	nRet=ftruncate(H264SharedMemHandle, sizeof(SHM_RTSP_SERVER));
	if(nRet<0)
	{
		;
	}

    	fstat(H264SharedMemHandle, &statbuf);
    	H264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, H264SharedMemHandle, 0);  
    	H264ShmImgPtr = (SHM_RTSP_SERVER*)H264SharedMemPtr; 
    	
	//used in H264or5VideoStreamFramer.cpp
	//Live555H264FrameRate=8; //fps
	
	/*H264ShmImgPtr->rtsp_config_details.fps=3;	///MEDIUM/LOW;	25/12/8
	H264ShmImgPtr->rtsp_config_details.resolution=EIGHT_MP;
	H264ShmImgPtr->rtsp_config_details.rtsp_authentication=TRUE;
	H264ShmImgPtr->rtsp_config_details.rtsp_mode=RTSP_UNICAST;

	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[0]='8';
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[1]='5';
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[2]='5';
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[3]='6';
	H264ShmImgPtr->rtsp_config_details.rtsp_portnum[4]='\0';

	H264ShmImgPtr->rtsp_config_details.onvif_portnum[0]='1';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[1]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[2]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[3]='0';
	H264ShmImgPtr->rtsp_config_details.onvif_portnum[4]='\0';

	mtx_strcpy((char*)H264ShmImgPtr->rtsp_config_details.rtsp_stream_name,"h264");

	for( int i = 0; i < MAX_REMOTE_USER ;i++ )
	{
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][0] = '5';
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][1] = '5';
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][2] = '6';
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][3] = '6';
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][4] = '7';
		H264ShmImgPtr->rtsp_config_details.remote_user_password[i][5] = '7';
		H264ShmImgPtr->rtsp_config_details.remote_user_level[i] = GUEST;
		mtx_sprintf(H264ShmImgPtr->rtsp_config_details.remote_user_name[i], "Guest_%02d", i+1);
	}
	mtx_strcpy (H264ShmImgPtr->rtsp_config_details.remote_user_name[0],"onvifusr");
	H264ShmImgPtr->rtsp_config_details.remote_user_level[0] = ADMIN;
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][0] = '6';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][1] = '5';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][2] = '4';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][3] = '3';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][4] = '2';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][5] = '1';

	mtx_strcpy (H264ShmImgPtr->rtsp_config_details.remote_user_name[1],"User_L01");
	H264ShmImgPtr->rtsp_config_details.remote_user_level[1] = USER;
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '3';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '8';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '5';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '7';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '6';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '4';	

	mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.model_name,"MtxAICamera");
	if(H264ShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
	{
		mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM5MP");
	}
	else
	{
		if(H264ShmImgPtr->rtsp_config_details.resolution==THREE_MP)
		{
			mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM3MP");
		}
		else
		{
			if(H264ShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
			{
				mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM8MP");
			}
		}
	}

	mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.serial_no,"00001");
	mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.firmware_version,"F248");
	mtx_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.manufacturer,"MEDIATRONIX");*/
	
	
}
/*
#pragma pack(1)
typedef struct ShmX264
{
	unsigned char filled[2];
	int buff_cnt;
	unsigned long size[2];
	int jpgframecnt[2];
	unsigned char payload[2][4*1024*1024];
	
	unsigned char yuv_filled[2];
	int yuv_buff_cnt;
	unsigned long yuv_size[2];	
	int yuvframecnt[2];	
	unsigned char yuv_payload[2][13*1024*1024];
	
}SHM_X264;
#pragma pack(0)*/

SHM_X264 *X264ShmImgPtr;

void ConfigureX264ServerSharedMemory(void)
{  
	void *X264SharedMemPtr;
	int X264SharedMemHandle,nRet;

	struct stat statbuf;
	
    	X264SharedMemHandle = shm_open("/rtsp_x264_shm.dat", O_CREAT | O_RDWR, 0666);
    	nRet=ftruncate(X264SharedMemHandle, sizeof(SHM_X264));
	if(nRet<0)
	{
		;
	}

    	fstat(X264SharedMemHandle, &statbuf);
    	X264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, X264SharedMemHandle, 0);  
    	X264ShmImgPtr = (SHM_X264*)X264SharedMemPtr; 
    	
    	memset(X264ShmImgPtr,0,sizeof(SHM_X264));
}

