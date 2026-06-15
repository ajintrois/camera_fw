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
/*                          Includes	                                                    */
/********************************************************************************************/
#include <iostream>
#include <iomanip>
#include <cairo/cairo.h>
#include "MtxMIMGVideoLiveFramedSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "../CameraServer/MtxSrc/Src/MediaServer/MtxStreamer/RTSPDefines.h"  

/********************************************************************************************/
/*                          Macros	                                                    */
/********************************************************************************************/
#define RESIDUALBUFMAXSIZE		0x200000
/********************************************************************************************/
/*                          Extern Variable	                                            */
/********************************************************************************************/
extern SHM_RTSP_SERVER *JPEGShmImgPtr;
//extern SYSTEMVARS *SystemVariablesShmPTR;
extern unsigned long fps_duration;


/********************************************************************************************/
/*                          Global Variable	                                            */
/********************************************************************************************/
static unsigned char lastfrm[4*1024*1024];
static int lastfrm_size;

//static struct timeval g_afPresentationTime; 
//static struct timeval s_alPresentationTime; 

static u_int16_t  q_precision;
static u_int16_t  q_len;
static u_int8_t  qtable[2*64];
/********************************************************************************************/
/*                          Function Definition                                             */
/********************************************************************************************/
MtxMIMGVideoLiveFramedSource*
MtxMIMGVideoLiveFramedSource::createNew(UsageEnvironment& env, char const* fileName,unsigned preferredFrameSize,unsigned playTimePerFrame) 
{
	// env << "call MtxMIMGVideoLiveFramedSource::createNew\n";
	env << "JPEGRTSP: New Source Joined:\n";
	int z;

	MtxMIMGVideoLiveFramedSource* newSource	= new MtxMIMGVideoLiveFramedSource(env, fileName, preferredFrameSize, playTimePerFrame);

	// printf("FileName: %s\n", fileName);

	JPEGShmImgPtr->start=1;
	JPEGShmImgPtr->counter_rtsp=0;
	JPEGShmImgPtr->prev_start=0;
	JPEGShmImgPtr->buff_cnt=0;
	JPEGShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		JPEGShmImgPtr->filled[z]=0;
	}
	
	return newSource;
}

MtxMIMGVideoLiveFramedSource::MtxMIMGVideoLiveFramedSource(UsageEnvironment& env, char const* fileName, unsigned preferredFrameSize,unsigned playTimePerFrame) : JPEGVideoSource(env)
{

	
}

MtxMIMGVideoLiveFramedSource::~MtxMIMGVideoLiveFramedSource() 
{
	//envir() << ":Source Deactivated:\n";
	envir() << "JPEGRTSP: Source Destroy...\n";
	int z;

	JPEGShmImgPtr->start=0;
	JPEGShmImgPtr->counter_rtsp=0;
	JPEGShmImgPtr->prev_start=0;
	JPEGShmImgPtr->buff_cnt=0;
	JPEGShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		JPEGShmImgPtr->filled[z]=0;
	}
}
static void get_qtableinfo(u_int8_t  *jpeg, int size)
{
	int idx=0;

	u_int16_t  precision;

	while(idx < size)
	{
		if(jpeg[idx]==0xff && jpeg[idx+1]==0xdb)
		{
			idx += 2;
			break;
		}
		idx++;
	}
	idx += 2;
	precision = (jpeg[idx++]>>4)&0xff;
	if(precision ==0)
	{//precision=0, table len = 64bytes
		mempcpy(qtable,&jpeg[idx],64);
		idx += 64;
	}
	else
	{
		printf("error:precision is %d\n",precision);
		//while(1);
	}

	q_len = 64;
	//check if there is the second qtable
	if(jpeg[idx]==0xff && jpeg[idx+1]==0xdb)
	{
		idx +=4;
		precision = (jpeg[idx++]>>4)&0xff;
		if(precision ==0)
		{
			//precision=0, table len = 64bytes
			mempcpy(&qtable[64],&jpeg[idx],64);
			idx += 64;
		}
		else
		{
			printf("error:second qtable precision is %d\n",precision);
			while(1);
		}
		q_len += 64;
	}
	q_precision = precision;
	//printf("precision =%d q_len=%d\n",q_precision,q_len);
}

static int get_jpeg_raw_start(unsigned char *jpeg, int size)
{
	int idx=0;
	while(idx < size)
	{
		if(jpeg[idx]==0xff && jpeg[idx+1]==0xda)
		{
			idx += 2;
			idx += (jpeg[idx]<<8)|jpeg[idx+1];
			return idx;
		}
		idx++;
	}
	return -1;
}

static int skip_jpeg_ffd9(unsigned char *jpeg, int size)
{
	u_int8_t *b = jpeg + size - 2 ;
	u_int32_t cnt;
	cnt = 0;
	while(cnt <= (u_int32_t)(size -2))
	{
		if(b[0]==0xff && b[1]==0xd9)
		{
			//return (cnt+2);
			return (cnt);
		}
		b--;
		cnt ++;
	}
	return -1;
}

void MtxMIMGVideoLiveFramedSource::doGetNextFrame() 
{
	int newfrm = 0;

	while (ReadOneNaluFromVideoList((unsigned char *)framebuffer) == 0);//get new nalu	

	/*if(ReadOneNaluFromVideoList((unsigned char *)framebuffer))
	{
		newfrm = 1;
	}
	else
	{
		//printf("Warning newfrm=0\n");
	}*/
	newfrm = 1;
	if(newfrm)
	{
		get_qtableinfo((unsigned char *)framebuffer,ImageSize);
		int rawoffset = get_jpeg_raw_start((unsigned char *)framebuffer,ImageSize);
		if(rawoffset <0)
		{
			envir() << "error: jpeg raw data not found\n";			
			fNumTruncatedBytes = 0;
			fFrameSize = lastfrm_size;
			mempcpy(fTo, lastfrm, lastfrm_size);
			goto exit;
		}
		//			printf("rawoffset = %d\n",rawoffset);
		int tail_skip_len = skip_jpeg_ffd9((unsigned char *)framebuffer,ImageSize);
		if(tail_skip_len < 0)
		{
			envir() << "error: jpeg 0xffd9 not found\n";			
			fNumTruncatedBytes = 0;
			fFrameSize = lastfrm_size;
			mempcpy(fTo, lastfrm, lastfrm_size);
			goto exit;
		}
		//			printf("fMaxSize = %d\n",fMaxSize);
		unsigned char *raw_start = (unsigned char *)framebuffer + rawoffset;
		unsigned int frame_length = ImageSize - rawoffset - tail_skip_len;
		//unsigned int frame_length = ImageSize -2-2;//- rawoffset - tail_skip_len;
		//printf("rawoffset=%d, frame_length=%d fMaxSize = %d\n",rawoffset,frame_length,fMaxSize);
		if(frame_length > fMaxSize) 
		{
			envir()<<"MIMG Error:frame_length>fMaxSize";
			envir()<<" frame_length="<<frame_length;
			envir()<<" fMaxSize="<<fMaxSize << "\n";			
			fNumTruncatedBytes = 0;
			fFrameSize = lastfrm_size;
			mempcpy(fTo, lastfrm, lastfrm_size);
			goto exit;
		}
		else 
		{
			fNumTruncatedBytes = 0;
			fFrameSize = frame_length;
		}

		mempcpy(lastfrm,raw_start,fFrameSize);
		lastfrm_size = fFrameSize;
		mempcpy(fTo, raw_start, fFrameSize);
	}
	/*else
	{
		fNumTruncatedBytes = 0;
		fFrameSize = lastfrm_size;
		mempcpy(fTo, lastfrm, lastfrm_size);
	}*/

exit:

	//set timestamp, since it is live source, i guess it can be wall clock time?
	gettimeofday(&fPresentationTime,0);

	#if 0
        //to be used if both audio and video is there          
	//add for AV SYNC
		if(g_afPresentationTime.tv_sec || g_afPresentationTime.tv_usec)
		{
			if(g_afPresentationTime.tv_sec != s_alPresentationTime.tv_sec || g_afPresentationTime.tv_usec != s_alPresentationTime.tv_usec)
			{
				fPresentationTime.tv_sec = g_afPresentationTime.tv_sec;
				fPresentationTime.tv_usec = g_afPresentationTime.tv_usec;
				s_alPresentationTime = g_afPresentationTime;
			}
		}

	//	fDurationInMicroseconds = 200000; //for IPCAM, MIMG fps = 5
		fDurationInMicroseconds = 1000000/25;
	#endif
	
	#if 0
	// Set the 'presentation time': Another method
	if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) 
	{
		gettimeofday(&fPresentationTime, NULL);
	}
	else 
	{
		// Increment by the play time of the previous frame:
		unsigned uSeconds = fPresentationTime.tv_usec + 1000000/25; //25fps, the real frame speed from camera is about 5~7frames/s
		fPresentationTime.tv_sec += uSeconds/1000000;
		fPresentationTime.tv_usec = uSeconds%1000000;
	}
	#endif	

	//live source don't need set duration
	//fDurationInMicroseconds
	/*if(JPEGShmImgPtr->rtsp_config_details.fps==HIGH)
	{
		fDurationInMicroseconds = 100000;//90909;//11 or 10
	}
	else
	{
		if(JPEGShmImgPtr->rtsp_config_details.fps==MEDIUM)
		{
			fDurationInMicroseconds = 142857;//125000;//8 or 7
		}
		else
		{
			if(JPEGShmImgPtr->rtsp_config_details.fps==LOW)
			{
				fDurationInMicroseconds = 200000;//166667;//6 or 5
			}
		}
	}*/	
	//fDurationInMicroseconds = 100000;

	nextTask() = envir().taskScheduler().scheduleDelayedTask(0,(TaskFunc*)FramedSource::afterGetting, this);
}


u_int8_t MtxMIMGVideoLiveFramedSource::type() 
{
//	return 0; //for YUV420
//	return 1; //for YUV422
	return 1; //64/65/0 
}

u_int8_t MtxMIMGVideoLiveFramedSource::qFactor() 
{
	//return 128;
	return 255;
}

u_int8_t MtxMIMGVideoLiveFramedSource::width() 
{
//	return 320/8; //640/8
//	return 640/8; //640/8
//	return (1920/8);

	u_int8_t retval = 0;
 
	if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)
		retval= (1280/8);
	if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
		retval= (1280/8);
	if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
	{
		//retval= (1920/8);
		retval= (1280/8);
	}
	
	return retval;

}

u_int8_t MtxMIMGVideoLiveFramedSource::height() 
{
//	return 192/8; //480/8
//	return 352/8; //480/8
//	return (368/8);

	u_int8_t retval = 0;
 
	if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)
		retval= (720/8);
	if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
		retval= (1024/8);
	if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
	{
		//retval= (1080/8);
		retval= (720/8);
	}

	return retval;

}

u_int16_t MtxMIMGVideoLiveFramedSource::restartInterval()
{
	return 84;
}

u_int8_t const* MtxMIMGVideoLiveFramedSource::quantizationTables(u_int8_t& precision,u_int16_t& length) 
{
	//  precision = q_precision;
	length = q_len;//128;
	precision=0;
	//length=0;
	return qtable;
}
unsigned char pH264[256*1024];
FILE *tstfptr; 
char buffer1[32];

#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;

extern void overlayText(Mat &img, const string &text, int x, int y, Vec3b color, int scale=1);
extern int displayTextOnImage(cv::Mat &img, const std::string &text);


Mat ImageBuffer,InImg,OutImage;
unsigned char ResizedJpegImg[4*1024*1024];
char logbuffer[1024];	
#include <sys/time.h>
void GetSystemTime(volatile unsigned char *ptimecode)
{
	struct timeval tval;
	struct tm brokentime;

	gettimeofday(&tval,NULL);
	localtime_r(&tval.tv_sec,&brokentime);
	ptimecode[0] = brokentime.tm_sec;//seconds
	ptimecode[1] = brokentime.tm_min;//minutes
	ptimecode[2] = brokentime.tm_hour;//hour
	ptimecode[3] = brokentime.tm_mday;//date
	ptimecode[4] = brokentime.tm_mon;//month
	ptimecode[5] = brokentime.tm_year;//year
	
	//printf("System Time = %d:%d:%d %d:%d:%d\n",ptimecode[2],ptimecode[1],ptimecode[0],ptimecode[3],ptimecode[4]+1,ptimecode[5]+1900);
}

const char *monthName[]=
{
	"JAN",
	"FEB",
	"MAR",
	"APR",
	"MAY",
	"JUN",
	"JUL",
	"AUG",
	"SEP",
	"OCT",
	"NOV",
	"DEC"
};

#include <iostream>
#include <sstream>
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>


using std::cout;

#define BOXSIZE 50
cv::Point getPt1(Mat& frame) 
{
    return cv::Point(frame.cols,frame.rows-BOXSIZE);
}

cv::Point getPt2(Mat& frame) 
{
    return cv::Point(0,frame.rows);
}

void drawRectangle (Mat& frame) 
{
    static cv::Point pt1(getPt1(frame));
    static cv::Point pt2(getPt2(frame));
    static cv::Scalar white(0,0,0);


    Mat overlay;
    frame.copyTo(overlay);

    cv::rectangle(
        overlay,
        pt1,
        pt2,
        white,
        cv::FILLED
    );

    double opacity = 0.6;
    addWeighted(overlay, opacity, frame, 1 - opacity, 0, frame);

}

void drawText(Mat& frame, char *text) 
{
    int baseline=0;
    const int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    const double fontScale = 1;
    const int thickness = 1;

    static Size textSize = getTextSize(text, fontFace,fontScale, thickness, &baseline);

    static Point textOrg((getPt1(frame).x - getPt2(frame).x)/2-textSize.width/2, (getPt1(frame).y + getPt2(frame).y)/2+textSize.height/2);

    putText(frame, text, textOrg, fontFace, fontScale, Scalar::all(255), thickness, 8);
}
int MtxMIMGVideoLiveFramedSource::ReadOneNaluFromVideoList(unsigned char *buffer)
{

	//int i=0,j=0,colour_r,colour_g,colour_b,nRet;
	unsigned char timecode[6];
	char dispbuffer[256];
	Mat out;
	Mat overlay;
	int z;

	JPEGShmImgPtr->total_cnt=0;
	for(z=0;z<5;z++)
	{
		if(JPEGShmImgPtr->filled[z]==1)
		{
			JPEGShmImgPtr->total_cnt++;
		}
	}

	//printf("haai222 %d %d %d\n",JPEGShmImgPtr->total_cnt,JPEGShmImgPtr->filled[JPEGShmImgPtr->counter_rtsp],JPEGShmImgPtr->start);
	if((JPEGShmImgPtr->filled[JPEGShmImgPtr->counter_rtsp]==1)&&(JPEGShmImgPtr->start==1)&&(JPEGShmImgPtr->total_cnt>0))
	{
		
		//printf("Read1\n");
		#if 1
		//int idx = JPEGShmImgPtr->counter_rtsp;
		//size_t jpegSize = JPEGShmImgPtr->size[idx];

		vector<unsigned char> ImVec(	(char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->counter_rtsp],
						(char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->counter_rtsp]+JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp]);
		//printf("Read3\n");
		
		//InImg = imdecode(ImVec,IMREAD_COLOR);  	
		OutImage = imdecode(ImVec,IMREAD_COLOR);  
		/*if(!OutImage.empty()) 
		{
			if ((OutImage.type() != CV_8UC3)||(!OutImage.isContinuous()))
			{
			    	OutImage=OutImage.clone();			    			    	
			}
			if ((OutImage.channels() != 3))
			{
				cv::cvtColor(OutImage,OutImage,cv::COLOR_BGR2RGB);	
			}
			if ((OutImage.type() == CV_16UC3))
			{
				cv::Mat temp;
				OutImage.convertTo(temp,CV_8UC3,1.0/256.0);		
				OutImage=temp;
			}
		}*/
		//printf("Read21\n");
		//std::cout << "JPEG size=" << JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp] << " decode ok=" << !OutImage.empty() << std::endl;
		//if(InImg.data!=NULL)
		/*if (OutImage.empty()) 
		{
    			std::cerr << "imdecode failed! JPEG size=" 
		              << JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp] << std::endl;
		    return 0;  // skip this frame safely
		}*/
		//if(OutImage.data!=NULL)
		if (!OutImage.empty())
		{
			//resize(img,OutImage[i],Size(640,640),INTER_CUBIC);
			//resize(img,OutImage[i],Size(640,640),INTER_NEAREST);
			//INTER_AREA

			#if 0
			if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)
			{
				resize(InImg,OutImage,Size(1280,720),INTER_NEAREST);
				//printf("3MP RTSP Resoultion>>>>>> %d\n",JPEGShmImgPtr->rtsp_config_details.resolution);
			}
			else
			{
				if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
				{
					resize(InImg,OutImage,Size(1280,1024),INTER_NEAREST);
					//printf("5MP RTSP Resoultion>>>>>> %d\n",JPEGShmImgPtr->rtsp_config_details.resolution);
				}
				else
				{
					if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
					{
						resize(InImg,OutImage,Size(1920,1080),INTER_NEAREST);
						//printf("5MP RTSP Resoultion>>>>>> %d\n",JPEGShmImgPtr->rtsp_config_details.resolution);
					}
				}
			}
			#endif

			//InImg.copyTo(OutImage);//If resize is not used
			//if(OutImage.data!=NULL)
			if (!OutImage.empty())
			{
				GetSystemTime(timecode);
				//snprintf(dispbuffer,sizeof(dispbuffer),"%s %d %d %d:%d:%d",
				//monthName[timecode[4]],timecode[3],timecode[5]+1900,timecode[2],timecode[1],timecode[0]);				

				snprintf(	dispbuffer, sizeof(dispbuffer),
							"%02d-%s-%04d %02d:%02d:%02d",
							timecode[3], monthName[timecode[4]], timecode[5]+1900,
							timecode[2],timecode[1],timecode[0]);	



				//printf("rtsptime>>%s\n",dispbuffer);
				
				//colour_b=(rand()%256);
				//colour_g=(rand()%256);
				//colour_r=(rand()%256);
				
				//cv::Size text_size;
				//text_size=getTextSize(dispbuffer, FONT_HERSHEY_SIMPLEX, .6,2,0);
				//printf("Read11\n");
/*				
				if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)				
				{
					rectangle(OutImage,Point(20, 690 - text_size.height),Point(20+text_size.width, 690), Scalar(0,0,0),-1,false);
					putText(OutImage,dispbuffer, Point(20, 690), FONT_HERSHEY_SIMPLEX, .6, Scalar(0,255,0), 2,LINE_8);
				}
				if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
				{
					rectangle(OutImage,Point(20, 1000 - text_size.height),Point(20+text_size.width, 1000), Scalar(0,0,0),-1,false);
					putText(OutImage, dispbuffer, Point(20, 1000), FONT_HERSHEY_SIMPLEX, .6, Scalar(0,255,0), 2,LINE_8);
				}

*/				
				//printf("Read8\n");
				
			

#if 0
				double alpha = 0.3;

				// copy the source image to an overlay
				//OutImage.copyTo(overlay);
				//overlay = OutImage.clone();
				overlay=OutImage;
				if (overlay.empty()) {
 				   std::cerr << "overlay creation failed!" << std::endl;
 				   return 0;
				}
				printf("Read2\n");
				std::cout << "Image size: " << overlay.cols << "x" << overlay.rows << std::endl;
				std::cout << "overlay: " << overlay.cols << "x" << overlay.rows 
          			<< " ch=" << overlay.channels() 
          			<< " type=" << overlay.type() 
          			<< " depth=" << overlay.depth()           			
          			<< " continuous=" << overlay.isContinuous() 
          			<< std::endl;
          			//if (overlay.depth() != CV_8U) 
          			//{
				//    overlay.convertTo(overlay, CV_8UC3, 255.0);
				//}
				
				/*if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)				
				{
					// draw a filled, yellow rectangle on the overlay copy
					rectangle(overlay,Point(20, 690 - text_size.height),Point(20+text_size.width, 690), Scalar(0,0,0),-1);//,false);
				}
				if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
				{
					// draw a filled, yellow rectangle on the overlay copy
					rectangle(overlay,Point(20, 1000 - text_size.height),Point(20+text_size.width, 1000), Scalar(0,0,0),-1);//,false);
				}*/
				if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
				{
					// draw a filled, yellow rectangle on the overlay copy
					//rectangle(overlay,Point(20, 1050 - text_size.height),Point(20+text_size.width, 1050), Scalar(0,0,0),-1);//,false);
					//rectangle(overlay,Point(20, 690 - text_size.height),Point(20+text_size.width, 690), Scalar(0,0,0),-1);//,false);
					rectangle(overlay,Point(20, 690 - 10),Point(20+100, 690), Scalar(0,0,0),-1);//,false);
				}
				printf("Read3\n");
				// blend the overlay with the source image
				addWeighted(overlay, alpha, OutImage, 1 - alpha, 0, OutImage);				

				if(JPEGShmImgPtr->rtsp_config_details.resolution==THREE_MP)				
				{
					putText(OutImage, dispbuffer, Point(20, 690), FONT_HERSHEY_SIMPLEX, .6, Scalar(255,255,255), 2,LINE_8);
				}
				if(JPEGShmImgPtr->rtsp_config_details.resolution==FIVE_MP)
				{
					putText(OutImage, dispbuffer, Point(20, 1000), FONT_HERSHEY_SIMPLEX, .6, Scalar(255,255,255), 2,LINE_8);
				}
				if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
				{
					//putText(OutImage, dispbuffer, Point(20, 1050), FONT_HERSHEY_SIMPLEX, .6, Scalar(255,255,255), 2,LINE_8);
					cv::putText(OutImage, dispbuffer, cv::Point(20, 690), cv::FONT_HERSHEY_SIMPLEX, .6, cv::Scalar(255,255,255), 2,cv::LINE_8);
				}
#endif
				//drawText(OutImage,dispbuffer);
				string ts = dispbuffer;
				//overlayText(OutImage, ts, 20, 680, Vec3b(255,255,255), 1);

				displayTextOnImage(OutImage, ts);
    				/*
					if(JPEGShmImgPtr->rtsp_config_details.resolution==EIGHT_MP)
    				{
    					overlayText(OutImage, ts, 20, 680, Vec3b(255,255,255), 3);
    				}
						*/
    
				vector<unsigned char> ImVecEnc(OutImage.data,OutImage.data+OutImage.rows*OutImage.cols*OutImage.elemSize());
				std::vector<int> rtsp_qlty_param(2);
				rtsp_qlty_param[0] = cv::IMWRITE_JPEG_QUALITY;
				rtsp_qlty_param[1] = 40;//default(95) 0-100
				imencode(".jpg",OutImage,ImVecEnc,rtsp_qlty_param);
				if((!ImVecEnc.empty())&&(ImVecEnc.size()!=0))
				{
					for(unsigned long m=0;m<ImVecEnc.size();m++)
					{
						ResizedJpegImg[m]=ImVecEnc[m];
						JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp]=ImVecEnc.size();
					}
					mempcpy((char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->counter_rtsp],
						ResizedJpegImg,
						JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp]);						
				}
			}
		}
		#endif

		//printf("haaai\n");					
		 /* mtx_sprintf(buffer1,"%d.jpg",JPEGShmImgPtr->counter[JPEGShmImgPtr->counter_rtsp]);
		  tstfptr=fopen(buffer1,"wb");
		  fwrite((char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->counter_rtsp],1,JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp],tstfptr);
		  fclose(tstfptr);*/


		//printf("<<<Read %d %d %ld\n",JPEGShmImgPtr->counter[JPEGShmImgPtr->counter_rtsp],JPEGShmImgPtr->counter_rtsp,JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp]);
		mempcpy((char *)framebuffer,(char *)JPEGShmImgPtr->payload[JPEGShmImgPtr->counter_rtsp],JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp]);		
		ImageSize=JPEGShmImgPtr->size[JPEGShmImgPtr->counter_rtsp];		
		JPEGShmImgPtr->filled[JPEGShmImgPtr->counter_rtsp]=0;
		
		//mtx_sprintf(logbuffer,"%ld\n",fPresentationTime.tv_usec);
		//APPENDTOFILE("/tmp/rtsp.log",logbuffer,mtx_strlen(logbuffer));
		
		JPEGShmImgPtr->counter_rtsp++;
		if(JPEGShmImgPtr->counter_rtsp>4)
		{
			JPEGShmImgPtr->counter_rtsp=0;
		}

			
		//usleep(125000);
		//usleep(40000);
	}
	else
	{
		usleep(1000);
		//usleep(100);
		return 0;
	}

	unsigned char *c = (unsigned char *)framebuffer;
	if((c[0] == 0xff) && (c[1] == 0xd8)) 
	{
		return 1;
	}
	else 
	{

		return 0;
	}
}

