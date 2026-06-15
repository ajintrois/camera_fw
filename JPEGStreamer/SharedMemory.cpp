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
/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
SHM_RTSP_SERVER *JPEGShmImgPtr;
SHM_RTSP_SERVER *H264ShmImgPtr;
//static int H264SeqCnt,JPEGSeqCnt;
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
void ConfigureRTSPServerSharedMemory(void)
{  

	void *JpegSharedMemPtr;
	int JpegSharedMemHandle,nRet;

	struct stat statbuf;
	
    	JpegSharedMemHandle = shm_open("/rtsp_jpeg_shm.dat", O_CREAT | O_RDWR, 0666);
    	nRet=ftruncate(JpegSharedMemHandle, sizeof(SHM_RTSP_SERVER));
	if(nRet<0)
	{
		;
	}

    	fstat(JpegSharedMemHandle, &statbuf);
    	JpegSharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, JpegSharedMemHandle, 0);  
    	JPEGShmImgPtr = (SHM_RTSP_SERVER*)JpegSharedMemPtr; 
    	
    	////printf("Created RTSP JPEG shm %s\n","rtsp_jpeg_shm.dat");
		printf("Success:: Creation of shared resource...\n");;
	
}

void ConfigureH264ServerShdMem(void)
{
	void *H264SharedMemPtr;
	int H264SharedMemHandle;

 	struct stat statbuf;
 	
   	H264SharedMemHandle = shm_open("rtsp_h264_shm.dat", O_RDWR, 0660);
	///printf("RTSP Opened shm %s\n","rtsp_h264_shm.dat");
	printf("Success:: RTSP Shared Resource Opened\n");
    	fstat(H264SharedMemHandle, &statbuf);
    	H264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, H264SharedMemHandle, 0);  
    	H264ShmImgPtr = (SHM_RTSP_SERVER*)H264SharedMemPtr; 
}

#if 0
/*
 *For producer
 */
void ConfigureRTSPServerShdMem(void)
{
	
	void *JpegSharedMemPtr;
	int JpegSharedMemHandle;

	struct stat statbuf;
	
    	JpegSharedMemHandle = shm_open("rtsp_jpeg_shm.dat", O_RDWR, 0660);
	printf("RTSP Opened shm %s\n","rtsp_jpeg_shm.dat");
    	fstat(JpegSharedMemHandle, &statbuf);
    	JpegSharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, JpegSharedMemHandle, 0);  
    	JPEGShmImgPtr = (SHM_RTSP_SERVER*)JpegSharedMemPtr; 
    	
    	//JPEGShmImgPtr->rtsp_config_details.Fps=HIGH/MEDIUM/LOW;	25/12/8
}	
void WriteJPEGFrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length)
{
	JPEGSeqCnt++;
	if((!JPEGShmImgPtr->prev_start)&&(JPEGShmImgPtr->start==1))
	{
		JPEGShmImgPtr->prev_start=1;
		//printf("Start Buffering JPEG %d\n",JPEGSeqCnt);
	}

	if((JPEGShmImgPtr->filled[JPEGShmImgPtr->buff_cnt]==0)&&(JPEGShmImgPtr->prev_start==1)&&(JPEGShmImgPtr->start==1))
	{
		JPEGShmImgPtr->counter[JPEGShmImgPtr->buff_cnt]=JPEGSeqCnt;
		JPEGShmImgPtr->size[JPEGShmImgPtr->buff_cnt]=length;
		mempcpy((char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->buff_cnt],imgbuffer,length);
		JPEGShmImgPtr->filled[JPEGShmImgPtr->buff_cnt]=1;
		//printf("Write JPEG>>> %d %d\n",JPEGSeqCnt,JPEGShmImgPtr->buff_cnt);

		JPEGShmImgPtr->buff_cnt++;
		if(JPEGShmImgPtr->buff_cnt>4)
		{
			JPEGShmImgPtr->buff_cnt=0;
		}
		JPEGShmImgPtr->total_cnt++;
	}
}
#endif
