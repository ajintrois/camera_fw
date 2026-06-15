/*****************************************************************************
 * example.c: libx264 API usage example
 *****************************************************************************
 * Copyright (C) 2014-2019 x264 project
 *
 * Authors: Anton Mitrofanov <BugMaster@narod.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/
/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#ifdef _WIN32
#include <io.h>       /* _setmode() */
#include <fcntl.h>    /* _O_BINARY */
#endif

#include <stdint.h>
#include <stdio.h>
#include <x264.h>

#define FAIL_IF_ERROR( cond, ... )\
do\
{\
    if( cond )\
    {\
        fprintf( stderr, __VA_ARGS__ );\
        goto fail;\
    }\
} while( 0 )

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

#include "version.h"

void printSystemInfo() {
    printf("Application Name: x264Client\n");
    printf("Application Version: %s\n", APP_VERSION);
    printf("Build TimeStamp: %s\n", BUILD_TIMESTAMP);
	printf("X264_Build Version: %d\n", X264_BUILD);
	printf("X264_POINTVER: %s\n", X264_POINTVER);
}

/*#pragma pack(1)
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
SHM_RTSP_SERVER *H264ShmImgPtr;
static int H264SeqCnt;
 int buff_cnt=0;
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
void ConfigureX264SharedMemory(void)
{  

	void *X264SharedMemPtr;
	int X264SharedMemHandle;//,nRet;

	struct stat statbuf;
	
    	X264SharedMemHandle = shm_open("rtsp_x264_shm.dat", O_RDWR, 0660);
		if (X264SharedMemHandle == -1)
		{
			printf("Error: Unable to open RTSP shm %s\n","rtsp_x264_shm.dat");
			return;
		}

     	if(fstat(X264SharedMemHandle, &statbuf) == -1)
		{
			printf("Error: fstat failed for RTSP shm %s\n","rtsp_x264_shm.dat");
			return;
		}

    	X264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, X264SharedMemHandle, 0);  
    	X264ShmImgPtr = (SHM_X264*)X264SharedMemPtr; 
    	
}
void ConfigureH264ServerShdMem(unsigned char resolution)
{
	void *H264SharedMemPtr;
	int H264SharedMemHandle;

 	struct stat statbuf;
 	
	H264SharedMemHandle = shm_open("rtsp_h264_shm.dat", O_RDWR, 0660);
	if (H264SharedMemHandle == -1)
	{
		printf("Error: Unable to open RTSP shm %s\n","rtsp_h264_shm.dat");
		return;
	}

	printf("RTSP Opened shm %s\n","rtsp_h264_shm.dat");
    if(fstat(H264SharedMemHandle, &statbuf) == -1)
	{
		printf("Error: fstat failed for RTSP shm %s\n","rtsp_h264_shm.dat");
		return;
	}

    H264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, H264SharedMemHandle, 0);  
    H264ShmImgPtr = (SHM_RTSP_SERVER*)H264SharedMemPtr; 
 	

	/*H264ShmImgPtr->rtsp_config_details.fps=LOW;	///MEDIUM/LOW;	25/12/8
	H264ShmImgPtr->rtsp_config_details.resolution=resolution;
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

	safe_strcpy((char*)H264ShmImgPtr->rtsp_config_details.rtsp_stream_name,"h264");

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
	safe_strcpy (H264ShmImgPtr->rtsp_config_details.remote_user_name[0],"onvifusr");
	H264ShmImgPtr->rtsp_config_details.remote_user_level[0] = ADMIN;
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][0] = '6';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][1] = '5';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][2] = '4';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][3] = '3';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][4] = '2';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[0][5] = '1';

	safe_strcpy (H264ShmImgPtr->rtsp_config_details.remote_user_name[1],"User_L01");
	H264ShmImgPtr->rtsp_config_details.remote_user_level[1] = USER;
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '3';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '8';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '5';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '7';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '6';
	H264ShmImgPtr->rtsp_config_details.remote_user_password[1][0] = '4';	

	safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.model_name,"MtxAICamera");
	if(resolution==FIVE_MP)
	{
		safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM5MP");
	}
	else
	{
		if(resolution==THREE_MP)
		{
			safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM3MP");
		}
		else
		{
			if(resolution==EIGHT_MP)
			{
				safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.hardware_ID,"AICAM8MP");
			}
		}
	}

	safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.serial_no,"00001");
	safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.firmware_version,"F248");
	safe_strcpy ((char*)H264ShmImgPtr->rtsp_config_details.manufacturer,"MEDIATRONIX");*/
}

void WriteH264FrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length)
{
	//int z;
	
	//H264SeqCnt++;
	if((!H264ShmImgPtr->prev_start)&&(H264ShmImgPtr->start==1))
	{
		H264ShmImgPtr->prev_start=1;
		printf("Start Buffering H264 %d\n",H264SeqCnt);
	}

	if((H264ShmImgPtr->filled[H264ShmImgPtr->buff_cnt]==0)&&(H264ShmImgPtr->prev_start==1)&&(H264ShmImgPtr->start==1))
	{
		H264ShmImgPtr->counter[H264ShmImgPtr->buff_cnt]=X264ShmImgPtr->yuvframecnt[buff_cnt];
		H264ShmImgPtr->size[H264ShmImgPtr->buff_cnt]=length;
		mempcpy((char *)H264ShmImgPtr->payload[H264ShmImgPtr->buff_cnt],imgbuffer,length);
		H264ShmImgPtr->filled[H264ShmImgPtr->buff_cnt]=1;
		printf("Write H264>>> %d %d %ld\n",H264ShmImgPtr->counter[H264ShmImgPtr->buff_cnt],H264ShmImgPtr->buff_cnt,H264ShmImgPtr->size[H264ShmImgPtr->buff_cnt]);

		H264ShmImgPtr->buff_cnt++;
		if(H264ShmImgPtr->buff_cnt>4)
		{
			H264ShmImgPtr->buff_cnt=0;
		}
		//JPEGShmImgPtr->total_cnt++;

	}

	/*for(z=0;z<5;z++)
	{
		if(JPEGShmImgPtr->filled[z]==1)
		{
			JPEGShmImgPtr->total_cnt++;
		}
	}*/

}

int fillImage(uint8_t* buffer, int width, int height, x264_picture_t *pic)
{
    int ret = x264_picture_alloc(pic, X264_CSP_I420, width, height);
    if (ret < 0) return ret;
    pic->img.i_plane = 3; // Y, U and V
    pic->img.i_stride[0] = width;
    // U and V planes are half the size of Y plane
    pic->img.i_stride[1] = width / 2;
    pic->img.i_stride[2] = width / 2;
    int uvsize = ((width + 1) >> 1) * ((height + 1) >> 1);
    pic->img.plane[0] = buffer; // Y Plane pointer
    pic->img.plane[1] = buffer + (width * height); // U Plane pointer
    pic->img.plane[2] = pic->img.plane[1] + uvsize; // V Plane pointer
    return ret;
}

int main( int argc, char **argv )
{
    int width, height;
    x264_param_t param;
    x264_picture_t pic;
    x264_picture_t pic_out;
    x264_t *h;
    int i_frame = 0;
    int i_frame_size;
    x264_nal_t *nal;
    int i_nal;
    
   	if (argc >= 2)
	{
		if(strcmp(argv[1], "-v") == 0)
		{
			printf("-----------------------\n Version Details\n-----------------------\n");
			printSystemInfo();

			exit(0);
		}
	}

	printSystemInfo();

    //FAIL_IF_ERROR( !(argc > 1), "Example usage: example 352x288 <input.yuv >output.h264\n" );
    //FAIL_IF_ERROR( 2 != safe_sscanf( argv[1], "%dx%d", &width, &height ), "resolution not specified or incorrect\n" );

    sleep(2);

    ConfigureX264SharedMemory();
    ConfigureH264ServerShdMem(EIGHT_MP);
    
    
    //width=1920;
    //height=1080;

    width=1280;
    height=720;

    /* Get default params for preset/tuning */
    //if( x264_param_default_preset( &param, "veryfast", "zerolatency" ) < 0 )
    if( x264_param_default_preset( &param, "ultrafast", "zerolatency" ) < 0 )
        goto fail;

    /* Configure non-default params */
    param.i_bitdepth = 8;
    param.i_csp = X264_CSP_I420;
    param.i_width  = width;
    param.i_height = height;
    param.b_vfr_input = 0;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;
    param.i_fps_num = 10;
    param.i_sps_id = 7;
    
	param.i_threads = 1;	
	param.i_fps_den = 1;
	// Intra refres:
	param.i_keyint_max = 1;
	param.b_intra_refresh = 1;
	
	//Rate control:
	 /*param.rc.i_qp_constant = 22; 
	 param.rc.i_qp_min = 22; 
	 param.rc.i_qp_max = 22;*/
	 	
	
  	param.rc.i_rc_method = X264_RC_CRF;
	param.rc.i_vbv_buffer_size = 1000000;
	param.rc.i_vbv_max_bitrate = 2000;
	param.rc.f_rf_constant = 25;
	param.rc.f_rf_constant_max = 35;
    
    /* Apply profile restrictions. */
    //if( x264_param_apply_profile( &param, "baseline" ) < 0 )
    if( x264_param_apply_profile( &param, "high" ) < 0 )
        goto fail;

    if( x264_picture_alloc( &pic, param.i_csp, param.i_width, param.i_height ) < 0 )
       goto fail;

    //x264_picture_init(&pic);
    
#undef fail
#define fail fail2

    h = x264_encoder_open( &param );
    if( !h )
        goto fail;
#undef fail
#define fail fail3

    int luma_size = width * height;
    int chroma_size = luma_size / 4;
    
    
	if(X264ShmImgPtr == NULL)
	{
		printf("Error: x264 Shared memory not available !!!\n");
		exit(-1);
	}

    while(1)
    {
	    if((X264ShmImgPtr->yuv_filled[buff_cnt]==1))
	    {
		
		//if( x264_picture_alloc( &pic, param.i_csp, param.i_width, param.i_height ) < 0 )
		//	goto fail;
       	pic.img.i_plane = 3; // Y, U and V
		pic.img.i_stride[0] = width;
		// U and V planes are half the size of Y plane
		pic.img.i_stride[1] = width / 2;
		pic.img.i_stride[2] = width / 2;
		//int uvsize = ((width + 1) >> 1) * ((height + 1) >> 1);
		//pic.img.plane[0] = (unsigned char *)X264ShmImgPtr->yuv_payload[buff_cnt]; // Y Plane pointer
		//pic.img.plane[1] = (unsigned char *)X264ShmImgPtr->yuv_payload[buff_cnt] + (width * height); // U Plane pointer
		//pic.img.plane[2] = pic.img.plane[1] + uvsize; // V Plane pointer
    
    		//X264ShmImgPtr->size[buff_cnt]=length;
		
		mempcpy(pic.img.plane[0],(char *)X264ShmImgPtr->yuv_payload[buff_cnt],luma_size);
		mempcpy(pic.img.plane[1],(char *)X264ShmImgPtr->yuv_payload[buff_cnt]+luma_size,chroma_size);
		mempcpy(pic.img.plane[2],(char *)X264ShmImgPtr->yuv_payload[buff_cnt]+luma_size+chroma_size,chroma_size);

		
		
		

		/* Encode frames */
		//for( ;; i_frame++ )
		//{
			/* Read input frame */
		/*	if( fread( pic.img.plane[0], 1, luma_size, stdin ) != luma_size )
			{
				break;
			}
			if( fread( pic.img.plane[1], 1, chroma_size, stdin ) != chroma_size )
			{
				break;
			}
			if( fread( pic.img.plane[2], 1, chroma_size, stdin ) != chroma_size )
			{
				break;
			}*/

		pic.i_pts = i_frame++;
		i_frame_size = x264_encoder_encode( h, &nal, &i_nal, &pic, &pic_out );

		if( i_frame_size < 0 )
		{
				goto fail;
		}
		else if( i_frame_size )
		{
			
			WriteH264FrameToRTSPServerShdMem(nal->p_payload, i_frame_size);
			printf("Frame0 %d\n",i_frame_size);
			/*if( !fwrite( nal->p_payload, i_frame_size, 1, stdout ) )
			{
				goto fail;
			}*/
		}
			
		#if 0
		/* Flush delayed frames */
		while( x264_encoder_delayed_frames( h ) )
		{
			i_frame_size = x264_encoder_encode( h, &nal, &i_nal, NULL, &pic_out );
			if( i_frame_size < 0 )
			{
				goto fail;
			}
			else if( i_frame_size )
			{
				printf("Frame1 %d\n",i_frame_size);
				WriteH264FrameToRTSPServerShdMem(nal->p_payload, i_frame_size);
				/*if( !fwrite( nal->p_payload, i_frame_size, 1, stdout ) )
				{
					goto fail;
				}*/
			}
		}
		#endif
		  
		   //x264_picture_clean( &pic );
		   
		   X264ShmImgPtr->yuv_filled[buff_cnt]=0;
		   
		   buff_cnt++;
		   if(buff_cnt>1)
		   {
		      buff_cnt=0;
		   }
	   }
	   else
	   {
	   	usleep(1000);
	   	//printf("haai2\n");
	   }
    }    
    x264_encoder_close( h );
    x264_picture_clean( &pic );
    return 0;

#undef fail
fail3:
    x264_encoder_close( h );
fail2:
    x264_picture_clean( &pic );
fail:
    return -1;
}
