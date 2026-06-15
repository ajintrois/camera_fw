/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2013 Live Networks, Inc.  All rights reserved.
// A file source that is a plain byte stream (rather than frames)
// Implementation

/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/
/********************************************************************************************/
/*                          Includes                                                        */
/********************************************************************************************/
#include <iostream>
#include <iomanip>

#include "MtxH264VideoLiveFramedSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

#include "../CameraServer/MtxSrc/Src/MediaServer/MtxStreamer/RTSPDefines.h"  
/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
extern SHM_RTSP_SERVER *H264ShmImgPtr;
/********************************************************************************************/
/*                          Function Defines			                            */
/********************************************************************************************/
MtxH264VideoLiveFramedSource*
MtxH264VideoLiveFramedSource::createNew(UsageEnvironment& env, char const* fileName,unsigned preferredFrameSize,unsigned playTimePerFrame) 
{
	//env << "call MtxH264VideoLiveFramedSource::createNew\n";
	env << "H264RTSP: New Source Joined:\n";

	int z;

	MtxH264VideoLiveFramedSource* newSource	= new MtxH264VideoLiveFramedSource(env, fileName, preferredFrameSize, playTimePerFrame);

	H264ShmImgPtr->start=1;
	H264ShmImgPtr->counter_rtsp=0;
	H264ShmImgPtr->prev_start=0;
	H264ShmImgPtr->buff_cnt=0;
	H264ShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		H264ShmImgPtr->filled[z]=0;
	}

	return newSource;
}

MtxH264VideoLiveFramedSource::MtxH264VideoLiveFramedSource(UsageEnvironment& env, char const* fileName, unsigned preferredFrameSize,unsigned playTimePerFrame) : FramedSource(env)
{

}

MtxH264VideoLiveFramedSource::~MtxH264VideoLiveFramedSource() 
{
	//envir() << "call ~MtxH264VideoLiveFramedSource\n";
	envir() << "H264RTSP: One Source Left...\n";

	int z;
	
	H264ShmImgPtr->start=0;
	H264ShmImgPtr->counter_rtsp=0;
	H264ShmImgPtr->prev_start=0;
	H264ShmImgPtr->buff_cnt=0;
	H264ShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		H264ShmImgPtr->filled[z]=0;
	}
}

void MtxH264VideoLiveFramedSource::doGetNextFrame() 
{
	if (residual_size)
	{
		fFrameSize = residual_size;
		if (fFrameSize > fMaxSize)
		{
			fNumTruncatedBytes = fFrameSize - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else
		{
			fNumTruncatedBytes = 0;
		}
		if (fNumTruncatedBytes > RESIDUALBUFMAXSIZE)
		{
			envir() << "H264RTSP: error: pre-nalu - redidual buffer is overflow! actual size=" << fNumTruncatedBytes << "\n";
			printf("RTSP H264 M$ResetMe..W\n");
			printf("RTSP H264 M$ResetMe..W\n");
			printf("RTSP H264 M$ResetMe..W\n");
			return;
		}
		mempcpy(fTo, residual_buf, fFrameSize);
		if (fNumTruncatedBytes)
		{
			mempcpy(residual_buf, residual_buf + fFrameSize, fNumTruncatedBytes);
		}
	}
	else
	{
		//get new nalu
		while (ReadOneNaluFromVideoList((unsigned char *)framebuffer) == 0);
		
		unsigned long frame_length = ImageSize;
		if (frame_length > fMaxSize)
		{
			fNumTruncatedBytes = frame_length - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else
		{
			fNumTruncatedBytes = 0;
			fFrameSize = frame_length;
		}
		mempcpy(fTo, (unsigned char *)framebuffer, fFrameSize);
		if (fNumTruncatedBytes > RESIDUALBUFMAXSIZE)
		{
			envir() << "H264RTSP: error: new-nalu - redidual buffer is overflow! actual size=" << fNumTruncatedBytes << "\n";
			printf("RTSP H264 >>M$ResetMe..W\n");
			printf("RTSP H264 >>M$ResetMe..W\n");
			printf("RTSP H264 >>M$ResetMe..W\n");
			return;
		}
		mempcpy(residual_buf, (unsigned char *)framebuffer + fFrameSize, fNumTruncatedBytes);
		//printf("fMaxSize=%d \n",fMaxSize);
	}
	residual_size = fNumTruncatedBytes;
	
	
	nextTask() = envir().taskScheduler().scheduleDelayedTask(0,(TaskFunc*)FramedSource::afterGetting, this);
}
/*
 * File Operation Utilities
 */

/*#define DEFINE_FILE				FILE *testfptr;

#define LOGTOFILE(filename,buff,size)		testfptr=fopen(filename,"wb"); \
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	

#define APPENDTOFILE(filename,buff,size)	testfptr=fopen(filename,"a");	\
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	
						
DEFINE_FILE						
int capcount=500;*/
int MtxH264VideoLiveFramedSource::ReadOneNaluFromVideoList(unsigned char *buffer)
{

	int z;
	
	H264ShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		if(H264ShmImgPtr->filled[z]==1)
		{
			H264ShmImgPtr->total_cnt++;
		}
	}

	if((H264ShmImgPtr->filled[H264ShmImgPtr->counter_rtsp]==1)&&(H264ShmImgPtr->start==1)&&(H264ShmImgPtr->total_cnt>0))
	{
		mempcpy((char *)framebuffer,(char *)H264ShmImgPtr->payload[H264ShmImgPtr->counter_rtsp],H264ShmImgPtr->size[H264ShmImgPtr->counter_rtsp]);		
		ImageSize=H264ShmImgPtr->size[H264ShmImgPtr->counter_rtsp];		
		
		

		//set timestamp, since it is live source, i guess it can be wall clock time?
		gettimeofday(&fPresentationTime,0);
		// printf("<<<Read %ld cnt=%d bufcnt=%d sze=%ld\n",fPresentationTime, H264ShmImgPtr->counter[H264ShmImgPtr->counter_rtsp],H264ShmImgPtr->counter_rtsp,ImageSize);

		//mempcpy((char*)&fPresentationTime,(char*)&H264ShmImgPtr->presentation_time[H264ShmImgPtr->counter_rtsp],sizeof(fPresentationTime));
		
		/*if(capcount==500)
		{
			LOGTOFILE("2.h264",(char *)H264ShmImgPtr->payload[H264ShmImgPtr->counter_rtsp],ImageSize);
			capcount--;
		}
		else
		{
			if(capcount>0)
			{
				APPENDTOFILE("2.h264",(char *)H264ShmImgPtr->payload[H264ShmImgPtr->counter_rtsp],ImageSize);
				capcount--;
			}
		}*/				
		H264ShmImgPtr->filled[H264ShmImgPtr->counter_rtsp]=0;
		H264ShmImgPtr->counter_rtsp++;
		if(H264ShmImgPtr->counter_rtsp>4)
		{
			H264ShmImgPtr->counter_rtsp=0;
		}
		H264ShmImgPtr->total_cnt--;	
		
			
	}
	else
	{
		usleep(1000);
		return 0;
	}

	unsigned char *c = (unsigned char *)framebuffer;

	if((c[0] == 0x00) && (c[1] == 0x00) && (c[2] == 0x00) && (c[3] == 0x01)) 
	{
		return 1;
	}
	else 
	{
		printf("H264RTSP: Nalu not found\n");
		return 0;
	}

}

