#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include "common.h"
#include "sv.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include "../defines.h"
#include "../common_shm.h"
#include "../eeprom.h" 

#include "NvJpegEncoder.h"
#include "NvBufSurface.h"
#include "NvUtils.h"
#include "../defs_aicam_agc_cfg_helper.h"

extern int InitSensorI2c1(int * pi2c_dev_handle);
extern int InitSensorI2c2(int * pi2c_dev_handle, int res);
extern void close_i2c(int * pi2c_dev_handle);
extern int RegInitSensorI2c(int i2c_dev_handle, int *vamx, int *hmax);
extern int SetShutterSensorI2c(int i2c_dev_handle, int vmax, int shutterindex);
extern int SetGainSensorI2c(int i2c_dev_handle, int gainindex, int aperture);
extern int SetGainValSensorI2c(int i2c_dev_handle, int gainval);
extern int SetShutterLineSensorI2c(int i2c_dev_handle, int shutterLines);
extern int GetShutterSensorI2c(int i2c_dev_handle);
extern int SetNormalSensorI2c(int i2c_dev_handle);
extern int checkExtTrigger(int i2c_dev_handle, int sh1, int sh2);
extern int SetExtTriggerSensorI2c(int i2c_dev_handle);
extern int StopExtTrig(int i2c_dev_handle);
extern int StartExtTrig(int i2c_dev_handle);
extern int Pulse2WidthExtTrig(int i2c_dev_handle, int val);// val =shutter in microsecs
extern int Pulse1WidthExtTrig(int i2c_dev_handle, int val);// val =shutter in microsecs
extern int MTXcudaISPNVbuffer(unsigned char *cuda_in_buffer, unsigned char *cuda_y_buffer, unsigned char *cuda_u_buffer, unsigned char *cuda_v_buffer, uint16_t width, uint16_t height, float loadgamma, float wbr, float wbgr, float wbgb, float wbb, short roffset, short g1offset, short g2offset, short boffset, int color);
extern size_t safe_strlen(const char *str, size_t max_len);
extern int GetVsync();
extern int closeGpio();
extern int initGpio();
//unsigned short	keepalive_timer[MAX_WDT_COUNT], keepalive_timer_enabled[MAX_WDT_COUNT], keepalive_timer_reload[MAX_WDT_COUNT], aux_keep_alive_timer[MAX_WDT_COUNT];

//------------Maheen--------------------------------------------------------------------------
#define THREE_MP	15
#define FIVE_MP		17
#define EIGHT_MP	20
extern void ConfigureRTSPServerShdMem(unsigned char resolution, unsigned char *userdata, unsigned char *ipdetails);
extern void WriteJPEGFrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length);
extern void SetJPEGFPSToRTSPServerShdMem(unsigned char fps);
//------------Maheen--------------------------------------------------------------------------


#pragma pack(1)
typedef struct embeddeddataimx565
{
	/* 92bytes of data
	12 bit raw data format
	 for every 2 bytes of the embedded data there is a padding byte[0x55].

	10 bit raw data format
	 for every 4 bytes of the embedded data there is a padding byte[0x55].

	skip the padding and read with below structure. 
	*/
	unsigned char res0;
	unsigned char res1;
	unsigned char res2;
	unsigned char res3;
	unsigned char res4;
	unsigned char res5;
	unsigned char hvmode;//bits[5,4]..
	unsigned char res7;
	unsigned char roimode;//bit[0].
	unsigned char res9;
	unsigned char hvreverse;//bit[4]=h, bit[0]=v
	unsigned char shutterspeed[3];//24bits 
	unsigned char res14_26[(26-14) + 1];//13bytes
	unsigned char blklvl[2];//12 bits[11,0] 
	unsigned char res29;
	unsigned char trig;//bits[4,3]=trigtiming, bits[2,0]trigmode
	unsigned char res31_32[2];
	unsigned char gain[2];//9bits..
	unsigned char res35_46[(46-35) + 1];
	unsigned char vmax[3];//24bits  
	unsigned char hmax[2];//16 bits  
	unsigned char res52_91[92-52];
} TEMBEDDEDDATA565;
#pragma pack()


typedef struct agcontext{
int AvgSel;//0=Center, 1=center2/3rd, 2=full
int ResponseTime; // brightness running average in frames.[1<->7] values=(1,2,4,8,16,32,64) 
int shuttermax;// in uSec, 1500
int shuttermin;// in uSec, 22
int gainmax;// 320 == 20db
int gainmin;// 0 == 1
int AgcPmLowThreshold; //	=0x58
int AgcTargetLowThreshold; // 	=0x68
int AgcTargetThreshold; // 	=0x80
int AgcTargetHighThreshold; //  =0x98
int AgcPmHighThreshold; // 	=0xA8
int init;// = 0;
int frameCount;
int avgfull;
int avg23rd;
int avgcent;
int slowcnt;// to slow the processing to avoid buffetting..
int avg;
int dir;
int prevAvgdata[256];
int prevAvgIndexWr;
int prevAvgIndexRd;
int prevAvgIndexPrime;
int PrevAppliedGain;
int PrevAppliedShutter;
int shutterlines;
int shutter;
int GainDBLinear;
int Gain;
} AGCCONTEXT;


void getembeddeddata(unsigned char * inputdata, int rawdatatype, unsigned char *emdatabuf)
{
	//rawdatatype = 0 --> 12bit format... and = 1 ---> 10bit format
	TEMBEDDEDDATA565 *outdata;
	int i = 0, j = 0, k = 0;

	outdata = (TEMBEDDEDDATA565 *)emdatabuf;
	for(; j < 92; i++, k++)
	{
		if(rawdatatype == 0)
		{	if(k == 2)
			{
				k = -1;
				continue;
			}
		}
		else
		{
			if(k == 4)
			{
				k = -1;
				continue;
			}
		}
		emdatabuf[j++] = inputdata[i];
	}
	i = outdata->shutterspeed[0] << 8;
	i += outdata->shutterspeed[1];
	i = i << 8;
	i += outdata->shutterspeed[2];
	j = outdata->gain[0]<<8;
	j += outdata->gain[1];
	j = j & 0x1ff;
	return;
}


static void setGammaReverseCurveConstant(float gamma, unsigned char *gammaRevTable)
{
//	const float gamma = 1.8;//1.8;
	//MAX_VALUE_X = 512
//	float	gammaValue[12] = {1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4, 2.45, 2.5, 2.55, 2.6, 2.65}, loadgamma;
//	float	gammaValue[12] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, loadgamma;
	double d, actualgamma;
	int i;
	unsigned short GammaCurve[512];//;

	actualgamma = gamma;// =1/gamma;
	GammaCurve[0] = 0;
	for(i = 1; i < 512; i++)
	{
		d = pow(i/(double)512,actualgamma);
		GammaCurve[i] = (unsigned short)(d*512);
	}
	gammaRevTable[0] = 0;
	for(i = 1; i < 256; i++)
		gammaRevTable[i] = GammaCurve[i << 1] >> 1;
}

static void get_brightness_image(unsigned char *data, int width, int height, int *avg, int *centeravg, int *avg23rds)
{
	int i, j, k, l, m, n, p = 0, q, r, s, t, u, v, x = 0, y = 0;
	int avgp = 0, centavgp = 0, avg23rdp = 0;
	unsigned char g;
	k = height*2/3;
	l = width *2/3;
	m = (height - k )/2;
	n = (width - l)/2;
	k += m;
	l += n;
	q = height/4;
	r = width/4;
	s = q + (height/2);
	t = r + (width/2);
	for(i = 0; i < height; i+=2)
	{
		u = v = 0;
		if((i  > m) && (i < k))
			u = 1;
		if((i  > q) && (i < s))
			v = 1;
		for(j = 0; j < width; j+=2)
		{
			//g = data[p];
			g = data[i*width + j];
			avgp += g;
			if(u)
				if((j  > n) && (j < l))
				{
					avg23rdp += g;
					x++;
				}
			if(v)
				if((j  > r) && (j < t))
				{
					centavgp += g;
					y++;
				}
			//p++;
		}
	}
	*avg = (avgp/(width*height/4));
	*centeravg = centavgp/y;
	*avg23rds = avg23rdp/x;
	return;
}

unsigned int get_Seqnumber(time_t timecode, unsigned char *crntyear, time_t *yrtime, unsigned int *seqno)
{
	struct tm	brokentime, temptime;
	time_t 		ltime;
	unsigned int	sec;
	
	(*seqno)++;
	localtime_r(&timecode, &brokentime);
	if((*crntyear) != brokentime.tm_year)
	{
		temptime.tm_sec = 0;
		temptime.tm_min = 0;
		temptime.tm_hour = 0;
		temptime.tm_mday = 1;
		temptime.tm_mon = 0;
		temptime.tm_year = brokentime.tm_year;
		*yrtime = mktime(&temptime);
		*seqno = 0;
	}
	*crntyear = brokentime.tm_year;
	localtime_r(yrtime, &brokentime);
	ltime = timecode - (*yrtime);
	sec = ((ltime << 7) & 0xfffff000) + *seqno;
	return(sec);
}


void AgcProcess(AGCCONTEXT *AGCContext) 
{
	int crnt_avg, avg, avgsum = 0, value, i, j, k;

	int IMX264AGainDbLinear[16] = {
	0,// input=output ratio=1
	60,// 2
	95,// 3
	120,//4
	139,//5
	155,//6
	169,//7
	180,//8
	190,//9
	200,//10
	208,//11
	215,//12
	222,//13
	229,//14
	235,//15
	240 //16
	};
	
	if(AGCContext->init)
	{
		AGCContext->init = 0;
		AGCContext->prevAvgIndexPrime = 10;
		AGCContext->frameCount = 0;
		AGCContext->prevAvgIndexWr = 0;
		AGCContext->prevAvgIndexRd = 0;
		AGCContext->slowcnt = 0;
		AGCContext->GainDBLinear = 0;
	}
	AGCContext->frameCount++;
	if(AGCContext->frameCount > 256)
		AGCContext->frameCount = 0;
	if(AGCContext->AvgSel == 0)
	{
		avg = AGCContext->avgcent;
	}
	else if(AGCContext->AvgSel == 1)
	{
		avg = AGCContext->avg23rd;
	}
	else
	{
		avg = AGCContext->avgfull;
	}
	AGCContext->prevAvgdata[AGCContext->prevAvgIndexWr] = avg;
	AGCContext->prevAvgIndexWr++;
	if(AGCContext->prevAvgIndexWr >= AGCContext->ResponseTime)
		AGCContext->prevAvgIndexWr = 0;
	if(AGCContext->prevAvgIndexPrime > 0)
	{
		AGCContext->PrevAppliedShutter = AGCContext->shuttermax;
		AGCContext->PrevAppliedGain = AGCContext->gainmax;
		AGCContext->shutter = AGCContext->shuttermax;
		AGCContext->Gain= AGCContext->gainmax;
		AGCContext->prevAvgdata[AGCContext->prevAvgIndexWr] = avg;
		AGCContext->prevAvgIndexWr++;
		if(AGCContext->frameCount < AGCContext->prevAvgIndexPrime)
		{
			AGCContext->shutter = AGCContext->shuttermax - AGCContext->frameCount;
			AGCContext->Gain = AGCContext->gainmax - (5* AGCContext->frameCount);
		}
		AGCContext->prevAvgIndexPrime--;
	}
	else
	{
		if(AGCContext->PrevAppliedShutter > AGCContext->shuttermax)
			AGCContext->shutter = AGCContext->shuttermax;
		if(AGCContext->PrevAppliedGain > AGCContext->gainmax)
			AGCContext->Gain = AGCContext->gainmax;
		if(AGCContext->PrevAppliedShutter < AGCContext->shuttermin)
			AGCContext->shutter = AGCContext->shuttermin;
		if(AGCContext->PrevAppliedGain < AGCContext->gainmin)
			AGCContext->Gain = AGCContext->gainmin;
		k = AGCContext->ResponseTime;
		crnt_avg = avg;
		for(i = 0; i < k; i++)
			avgsum += AGCContext->prevAvgdata[i];
		avg = (unsigned char)((float)avgsum/i);
		AGCContext->avg = avg;
		i = 0;// change speed -> 0 = no change, 1=slow, 2=normal, 3=speed
		k = 0;// direction -> 0 = up, 1 = down
		AGCContext->dir = 0;
		if(AGCContext->slowcnt)
		{
			AGCContext->slowcnt--;
/*			if(crnt_avg < AGCContext->AgcTargetThreshold)
			{
				i = 1;// slow
				k = 0;// up
				AGCContext->dir = 1;
			}
			if(crnt_avg > AGCContext->AgcTargetThreshold)
			{
				i = 1;// slow
				k = 1;// down
				AGCContext->dir -1;
			}
			else
			{
				// no actions..
				i = 0;
			}*/
				// no actions..
				i = 0;
				AGCContext->dir = 0;
		}
		else if((avg > AGCContext->AgcTargetLowThreshold) && (avg < AGCContext->AgcTargetHighThreshold))
		{
			if(avg < AGCContext->AgcTargetThreshold)
			{
				i = 1;// slow
				k = 0;// up
				AGCContext->dir = 1;
				if(AGCContext->ResponseTime == 64)
					AGCContext->slowcnt = 7;
				else if(AGCContext->ResponseTime == 32)
					AGCContext->slowcnt = 6;
				else if(AGCContext->ResponseTime == 16)
					AGCContext->slowcnt = 5;
				else if(AGCContext->ResponseTime == 8)
					AGCContext->slowcnt = 4;
				else if(AGCContext->ResponseTime == 4)
					AGCContext->slowcnt = 3;
				else if(AGCContext->ResponseTime == 2)
					AGCContext->slowcnt = 2;
				else
					AGCContext->slowcnt = 8;
			}
			else if(avg > AGCContext->AgcTargetThreshold)
			{
				i = 1;// slow
				k = 1;// down
				AGCContext->dir -1;
				if(AGCContext->ResponseTime == 64)
					AGCContext->slowcnt = 7;
				else if(AGCContext->ResponseTime == 32)
					AGCContext->slowcnt = 6;
				else if(AGCContext->ResponseTime == 16)
					AGCContext->slowcnt = 5;
				else if(AGCContext->ResponseTime == 8)
					AGCContext->slowcnt = 4;
				else if(AGCContext->ResponseTime == 4)
					AGCContext->slowcnt = 3;
				else if(AGCContext->ResponseTime == 2)
					AGCContext->slowcnt = 2;
				else
					AGCContext->slowcnt = 8;
			}
			else
			{
				// no actions..
				if(AGCContext->ResponseTime == 64)
					AGCContext->slowcnt = 10;
				else if(AGCContext->ResponseTime == 32)
					AGCContext->slowcnt = 9;
				else if(AGCContext->ResponseTime == 16)
					AGCContext->slowcnt = 8;
				else if(AGCContext->ResponseTime == 8)
					AGCContext->slowcnt = 7;
				else if(AGCContext->ResponseTime == 4)
					AGCContext->slowcnt = 6;
				else if(AGCContext->ResponseTime == 2)
					AGCContext->slowcnt = 5;
				else
					AGCContext->slowcnt = 12;
				i = 0;
			}
/*			if(abs(avg - AGCContext->AgcTargetThreshold) < 3)
				AGCContext->slowcnt = 3;
			else if(abs(avg - AGCContext->AgcTargetThreshold) < 6)
				AGCContext->slowcnt = 6;
			else if(abs(crnt_avg - AGCContext->AgcTargetThreshold) < 8)
				AGCContext->slowcnt = 8;*/
		}
		else if(crnt_avg <= AGCContext->AgcTargetLowThreshold)
		{
			if(avg >= AGCContext->AgcTargetLowThreshold)
			{
				i = 0;
				AGCContext->dir = 0;
				if(AGCContext->ResponseTime == 64)
					AGCContext->slowcnt = 7;
				else if(AGCContext->ResponseTime == 32)
					AGCContext->slowcnt = 6;
				else if(AGCContext->ResponseTime == 16)
					AGCContext->slowcnt = 5;
				else if(AGCContext->ResponseTime == 8)
					AGCContext->slowcnt = 4;
				else if(AGCContext->ResponseTime == 4)
					AGCContext->slowcnt = 3;
				else if(AGCContext->ResponseTime == 2)
					AGCContext->slowcnt = 2;
				else
					AGCContext->slowcnt = 8;
			}
			else
			{
				k = 0;// up
				if(avg < AGCContext->AgcPmLowThreshold) 
				{
					i = 3;// increase brightness fast..
					AGCContext->dir = 3;
				}
				else
				{
					i = 2;// increase brightness
					AGCContext->dir = 2;
					if(AGCContext->ResponseTime == 64)
						AGCContext->slowcnt = 7;
					else if(AGCContext->ResponseTime == 32)
						AGCContext->slowcnt = 6;
					else if(AGCContext->ResponseTime == 16)
						AGCContext->slowcnt = 5;
					else if(AGCContext->ResponseTime == 8)
						AGCContext->slowcnt = 4;
					else if(AGCContext->ResponseTime == 4)
						AGCContext->slowcnt = 3;
					else if(AGCContext->ResponseTime == 2)
						AGCContext->slowcnt = 2;
					else
						AGCContext->slowcnt = 8;
				}
			}
		}
		else if(crnt_avg >= AGCContext->AgcTargetHighThreshold)
		{
			if(avg <= AGCContext->AgcTargetHighThreshold)
			{
				i = 0;
				if(AGCContext->ResponseTime == 64)
					AGCContext->slowcnt = 7;
				else if(AGCContext->ResponseTime == 32)
					AGCContext->slowcnt = 6;
				else if(AGCContext->ResponseTime == 16)
					AGCContext->slowcnt = 5;
				else if(AGCContext->ResponseTime == 8)
					AGCContext->slowcnt = 4;
				else if(AGCContext->ResponseTime == 4)
					AGCContext->slowcnt = 3;
				else if(AGCContext->ResponseTime == 2)
					AGCContext->slowcnt = 2;
				else
					AGCContext->slowcnt = 8;
			}
			else
			{
				k = 1;// down
				if(avg > AGCContext->AgcPmHighThreshold) 
				{
					i = 3;// reduce brightness fast..
					AGCContext->dir = -3;
				}
				else
				{
					i = 2;// reduce brightness
					AGCContext->dir = -2;
					if(AGCContext->ResponseTime == 64)
						AGCContext->slowcnt = 7;
					else if(AGCContext->ResponseTime == 32)
						AGCContext->slowcnt = 6;
					else if(AGCContext->ResponseTime == 16)
						AGCContext->slowcnt = 5;
					else if(AGCContext->ResponseTime == 8)
						AGCContext->slowcnt = 4;
					else if(AGCContext->ResponseTime == 4)
						AGCContext->slowcnt = 3;
					else if(AGCContext->ResponseTime == 2)
						AGCContext->slowcnt = 2;
					else
						AGCContext->slowcnt = 8;
				}
			}
		}
		//else nothing..
		// apply change.
		if(i != 0)
		{
			if(i == 1)// slow...
			{
				if(AGCContext->frameCount >= (AGCContext->ResponseTime + (AGCContext->ResponseTime/2)))
				{// change 1 up /down
					AGCContext->frameCount = 0;
					if(k == 0)// up
					{
						if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
						{
							j = AGCContext->shuttermax - AGCContext->PrevAppliedShutter;
							if(j > 200)
								value = AGCContext->PrevAppliedShutter+50;
//							else if(j > 100)
//								value = AGCContext->PrevAppliedShutter+40;
							else if(j > 50)
								value = AGCContext->PrevAppliedShutter+20;
							else //if(AGCContext->PrevAppliedShutter < 500)
								value = AGCContext->shuttermax;
				   			AGCContext->shutter = value;
				   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
				   			{
					   			AGCContext->Gain = AGCContext->gainmin;
				   			}
			   			}
			   			else
			   			{
			   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermax)
			   				{
					   			AGCContext->shutter = AGCContext->shuttermax;
			   				}
				   			if(AGCContext->PrevAppliedGain < AGCContext->gainmax)
				   			{
//				   				AGCContext->GainDBLinear++;
//				   				if(AGCContext->GainDBLinear > 15)
//				   					AGCContext->GainDBLinear = 15;
//				   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];
//				   				AGCContext->frameCount = 0;
				   				j = AGCContext->gainmax - AGCContext->PrevAppliedGain;
//								if(j > 150)
//									value = AGCContext->PrevAppliedGain+30;
//								if(j > 80)
//									value = AGCContext->PrevAppliedGain+5;
								if(j > 10)
									value = AGCContext->PrevAppliedGain+2;
								else if(j > 2)
									value = AGCContext->PrevAppliedGain+1;
								else 
									value = AGCContext->gainmax;
					   			AGCContext->Gain = value;
				   			}
				   			//else nothing..
				   		}
			   		}
			   		else// down
			   		{
			   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
			   			{
//			   				if(AGCContext->GainDBLinear)
//			   					AGCContext->GainDBLinear--;
//			   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];
//			   				AGCContext->frameCount = 0;
			   				j = AGCContext->PrevAppliedGain - AGCContext->gainmin;
//							if(j > 150)
//								value = AGCContext->PrevAppliedGain-30;
//							if(j > 80)
//								value = AGCContext->PrevAppliedGain-5;
							if(j > 10)
								value = AGCContext->PrevAppliedGain-2;
							else if(j > 2)
								value = AGCContext->PrevAppliedGain-1;
							else 
								value = AGCContext->gainmin;
				   			AGCContext->Gain = value;
			   				if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
			   				{
					   			AGCContext->shutter = AGCContext->shuttermax;
			   				}
			   			}
			   			else
			   			{
			   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermin)
			   				{
			   					j = AGCContext->PrevAppliedShutter - AGCContext->shuttermin;
								if(j > 200)
									value = AGCContext->PrevAppliedShutter-50;
//								else if(j > 100)
//									value = AGCContext->PrevAppliedShutter-40;
								else if(j > 50)
									value = AGCContext->PrevAppliedShutter-20;
								else 
									value = AGCContext->shuttermin;
					   			AGCContext->shutter = value;
					   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
					   			{
						   			AGCContext->Gain = AGCContext->gainmin;
					   			}
					   		}
					   		//else nothing
			   			}
			   		}
				}
			}
			else if(i == 2)// fast
			{
				if(AGCContext->frameCount >= AGCContext->ResponseTime)
				{// change 1 up /down
					//AGCContext->frameCount = 0;
					if(k == 0)// up
					{
						if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
						{
							j = AGCContext->shuttermax - AGCContext->PrevAppliedShutter;
							if(j > 300)
								value = AGCContext->PrevAppliedShutter+70;
//							else if(j > 200)
//								value = AGCContext->PrevAppliedShutter+70;
							else if(j > 50)
								value = AGCContext->PrevAppliedShutter+20;
							else 
								value = AGCContext->shuttermax;
				   			AGCContext->shutter = value;
				   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
				   			{
					   			AGCContext->Gain = AGCContext->gainmin;
				   			}
			   			}
			   			else
			   			{
			   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermax)
			   				{
					   			AGCContext->shutter = AGCContext->shuttermax;
			   				}
				   			if(AGCContext->PrevAppliedGain < AGCContext->gainmax)
				   			{
/*				   				AGCContext->GainDBLinear++;
				   				if(AGCContext->GainDBLinear > 15)
				   					AGCContext->GainDBLinear = 15;
				   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];
				   				AGCContext->frameCount = 0;*/
				   				j = AGCContext->gainmax - AGCContext->PrevAppliedGain;
//								if(j > 200)
//									value = AGCContext->PrevAppliedGain+60;
//								if(j > 100)
//									value = AGCContext->PrevAppliedGain+20;
								if(j > 20)
									value = AGCContext->PrevAppliedGain+5;
								else if(j > 2)
									value = AGCContext->PrevAppliedGain+1;
								else 
									value = AGCContext->gainmax;
					   			AGCContext->Gain = value;
				   			}
				   			//else nothing..
				   		}
			   		}
			   		else// down
			   		{
			   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
			   			{
/*			   				if(AGCContext->GainDBLinear)
			   					AGCContext->GainDBLinear--;
			   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];
			   				AGCContext->frameCount = 0;*/
			   				j = AGCContext->PrevAppliedGain - AGCContext->gainmin;
//							if(j > 200)
//								value = AGCContext->PrevAppliedGain-60;
//							if(j > 100)
//								value = AGCContext->PrevAppliedGain-20;
							if(j > 20)
								value = AGCContext->PrevAppliedGain-5;
							else if(j > 2)
								value = AGCContext->PrevAppliedGain-1;
							else 
								value = AGCContext->gainmin;
				   			AGCContext->Gain = value;
			   				if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
			   				{
					   			AGCContext->shutter = AGCContext->shuttermax;
			   				}
			   			}
			   			else
			   			{
			   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermin)
			   				{
			   					j = AGCContext->PrevAppliedShutter - AGCContext->shuttermin;
								if(j > 300)
									value = AGCContext->PrevAppliedShutter-70;
//								else if(j > 200)
//									value = AGCContext->PrevAppliedShutter-70;
								else if(j > 50)
									value = AGCContext->PrevAppliedShutter-20;
								else 
									value = AGCContext->shuttermin;
					   			AGCContext->shutter = value;
					   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
					   			{
						   			AGCContext->Gain = AGCContext->gainmin;
					   			}
					   		}
					   		//else nothing
			   			}
			   		}
				}
			}
			else// change drastically..
			{
				AGCContext->frameCount = 0;
				if(k == 0)// up
				{
					if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
					{
						j = AGCContext->shuttermax - AGCContext->PrevAppliedShutter;
						if(j > 200)
							value = AGCContext->PrevAppliedShutter+90;
						else if(j > 50)
							value = AGCContext->PrevAppliedShutter+40;
						else 
							value = AGCContext->shuttermax;
			   			AGCContext->shutter = value;
			   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
			   			{
				   			AGCContext->Gain = AGCContext->gainmin;
			   			}
		   			}
		   			else
		   			{
		   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermax)
		   				{
							AGCContext->shutter = AGCContext->shuttermax;
		   				}
			   			if(AGCContext->PrevAppliedGain < AGCContext->gainmax)
			   			{
/*			   				AGCContext->GainDBLinear++;
			   				if(AGCContext->GainDBLinear > 15)
			   					AGCContext->GainDBLinear = 15;
			   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];*/
			   				j = AGCContext->gainmax - AGCContext->PrevAppliedGain;
//							if(j > 200)
//								value = AGCContext->PrevAppliedGain+100;
							if(j > 20)
								value = AGCContext->PrevAppliedGain+5;
							else if(j > 5)
								value = AGCContext->PrevAppliedGain+2;
							else 
								value = AGCContext->gainmax;
				   			AGCContext->Gain = value;
			   			}
			   			//else nothing..
			   		}
		   		}
		   		else// down
		   		{
		   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
		   			{
/*		   				if(AGCContext->GainDBLinear)
		   					AGCContext->GainDBLinear--;
		   				value = IMX264AGainDbLinear[AGCContext->GainDBLinear];*/
		   				j = AGCContext->PrevAppliedGain - AGCContext->gainmin;
//						if(j > 200)
//							value = AGCContext->PrevAppliedGain-100;
//						if(j > 100)
//							value = AGCContext->PrevAppliedGain-50;
						if(j > 20)
							value = AGCContext->PrevAppliedGain-5;
						else if(j > 5)
							value = AGCContext->PrevAppliedGain-2;
						else 
							value = AGCContext->gainmin;
			   			AGCContext->Gain = value;
		   				if(AGCContext->PrevAppliedShutter < AGCContext->shuttermax)
		   				{
				   			AGCContext->shutter = AGCContext->shuttermax;
		   				}
		   			}
		   			else
		   			{
		   				if(AGCContext->PrevAppliedShutter > AGCContext->shuttermin)
		   				{
		   					j = AGCContext->PrevAppliedShutter - AGCContext->shuttermin;
							if(j > 200)
								value = AGCContext->PrevAppliedShutter-90;
							else if(j > 50)
								value = AGCContext->PrevAppliedShutter-40;
							else 
								value = AGCContext->shuttermin;
				   			AGCContext->shutter = value;
				   			if(AGCContext->PrevAppliedGain > AGCContext->gainmin)
				   			{
					   			AGCContext->Gain = AGCContext->gainmin;
				   			}
				   		}
				   		//else nothing
		   			}
		   		}
	   		}
		}
		AGCContext->PrevAppliedShutter = AGCContext->shutter;
		AGCContext->PrevAppliedGain = AGCContext->Gain;
		AGCContext->prevAvgIndexRd++;
		if(AGCContext->prevAvgIndexRd >= AGCContext->ResponseTime)
			AGCContext->prevAvgIndexRd = 0;
	}
	return;
}

typedef struct VsyncThreadArg {
	int	Valid;// 0=stop 1=start
	int	SensorHandle;
	int	mode;// 0=single shutter 1=dual shutter 
	int	ExtTrigEn;
	int	nightsh1;
	int	nightsh2;
	int	Shutter, ShutterSet, ShutterSetVal;// single
	int	Shutter1, Shutter1Set, Shutter1SetVal;// 
	int	Shutter2, Shutter2Set,Shutter2SetVal;
	int	Gain, GainSet, GainSetVal;
	int 	Gain1, Gain1Set, Gain1SetVal;
	int 	Gain2, Gain2Set, Gain2SetVal;
} VSYNC_THREAD_ARG;

void *Vsyncthread(void * arg)
{
	VSYNC_THREAD_ARG	*syncarg;
	int			VsysncFlag, 
				prevVsysncFlag,
				PreDualfl = 0,
				PrevMode,
				VsyncCount = 0,
				switchfl = 0,
				exttrigrefresh = 0;
	
	syncarg = (VSYNC_THREAD_ARG *)arg;
	initGpio();
	VsysncFlag = GetVsync();
	prevVsysncFlag = VsysncFlag;
	PrevMode = syncarg->mode;
//	privntf("==================================== Vsyncthread Start ==============%d =\n", syncarg->Valid);
	while(1)
	{
		if(syncarg->Valid)
		{
			VsysncFlag = GetVsync();
			if(VsysncFlag != prevVsysncFlag)
			{
				//prin tf("VsysncFlag = %d count = %d\n", VsysncFlag, VsyncCount);
				if(VsysncFlag == 0)// risingedge...
				{
					VsyncCount++;
					if(PrevMode)
					{
						if(PreDualfl == 0)
						{
							// set hi shutter
							if(syncarg->Shutter1Set)
							{
								syncarg->Shutter1 = syncarg->Shutter1SetVal;
								syncarg->Shutter1Set = 0;
							}
							if(syncarg->SensorHandle)
							{
								SetShutterLineSensorI2c(syncarg->SensorHandle, syncarg->Shutter1);
							}
							if(syncarg->Gain1Set)
							{
								syncarg->Gain1 = syncarg->Gain1SetVal;
								syncarg->Gain1Set = 0;
							}	
							if(syncarg->SensorHandle)
							{
								SetGainValSensorI2c(syncarg->SensorHandle, syncarg->Gain1);
							}
							PreDualfl = 1;
						}
						else
						{
							// set lo shutter
							if(syncarg->Shutter2Set)
							{
								syncarg->Shutter2 = syncarg->Shutter2SetVal;
								syncarg->Shutter2Set = 0;
								
							}
							if(syncarg->SensorHandle)
							{
								SetShutterLineSensorI2c(syncarg->SensorHandle, syncarg->Shutter2);
							}
							if(syncarg->Gain2Set)
							{
								syncarg->Gain2 = syncarg->Gain2SetVal;
								syncarg->Gain2Set = 0;
							}	
							if(syncarg->SensorHandle)
							{
								SetGainValSensorI2c(syncarg->SensorHandle, syncarg->Gain2);
							}
							PreDualfl = 0;
							if(PrevMode != syncarg->mode)
							{
								PrevMode = syncarg->mode;
							}
						}
					}
					else
					{
						if(syncarg->ShutterSet)
						{
							syncarg->Shutter = syncarg->ShutterSetVal;
							syncarg->ShutterSet = 0;
						}
						if(syncarg->GainSet)
						{
							syncarg->Gain = syncarg->GainSetVal;
							syncarg->GainSet = 0;
						}	
						if(PrevMode != syncarg->mode)
						{
							PrevMode = syncarg->mode;
							PreDualfl = 0;
						}
					}
//					if((VsyncCount & 0x1F) == 0)
//						pri ntf("Vsync count = %d mode = %d, PreDualfl= %d, handle= %d shutter 1=%d, 2=%d \n", VsyncCount, PrevMode, PreDualfl, syncarg->SensorHandle,syncarg->Shutter1, syncarg->Shutter2);
				}
				else// falling edge..
				{
					if(syncarg->ExtTrigEn == 1)//turn on ext trigger...
					{
						syncarg->ExtTrigEn = 2;
						if(syncarg->nightsh1 > 2500)
							Pulse1WidthExtTrig(syncarg->SensorHandle, 2500);
						else
							Pulse1WidthExtTrig(syncarg->SensorHandle, syncarg->nightsh1);
					}
					else if(syncarg->ExtTrigEn == 2)
					{
						syncarg->ExtTrigEn = 3;
						if(syncarg->nightsh2 > 2500)
							Pulse2WidthExtTrig(syncarg->SensorHandle, 2500);
						else
							Pulse2WidthExtTrig(syncarg->SensorHandle, syncarg->nightsh2);
					}
					else if(syncarg->ExtTrigEn == 3)
					{
						syncarg->ExtTrigEn = 4;
						StartExtTrig(syncarg->SensorHandle);
					}
					else if(syncarg->ExtTrigEn == 4)
					{
						syncarg->ExtTrigEn = 5;
						StartExtTrig(syncarg->SensorHandle);
						SetExtTriggerSensorI2c(syncarg->SensorHandle);
						exttrigrefresh = 1;
					}
					else if(syncarg->ExtTrigEn == 5)// turn off ext trigger...
					{
						if((exttrigrefresh == 1) || (exttrigrefresh == 10) || (exttrigrefresh == 100))
						{
							if(syncarg->nightsh1 > 2500)
								Pulse1WidthExtTrig(syncarg->SensorHandle, 2500);
							else
								Pulse1WidthExtTrig(syncarg->SensorHandle, syncarg->nightsh1);
						}
						else if((exttrigrefresh == 2) || (exttrigrefresh == 11) || (exttrigrefresh == 101))
						{
							if(syncarg->nightsh2 > 2500)
								Pulse2WidthExtTrig(syncarg->SensorHandle, 2500);
							else
								Pulse2WidthExtTrig(syncarg->SensorHandle, syncarg->nightsh2);
						}
						exttrigrefresh++;
						if(exttrigrefresh > 200)
							exttrigrefresh = 100;
					}

					else if(syncarg->ExtTrigEn == 16)// turn off ext trigger...
					{
						SetNormalSensorI2c(syncarg->SensorHandle);
						syncarg->ExtTrigEn = 17;
					}
					else if(syncarg->ExtTrigEn == 17)
					{
						StopExtTrig(syncarg->SensorHandle);
						syncarg->ExtTrigEn = 0;
					}
				}
				prevVsysncFlag = VsysncFlag;
			}
			usleep(500);
		}
	}
	closeGpio();
	return arg;
}

typedef struct EncThreadArg {
COMPRESS_PC_SHARED_RESOURCES 	*compressPcShr;
NvBuffer			*nvbuffer1;
NvBuffer			*nvbuffer2;
NvBuffer			*nvbuffer3;
unsigned char			imagestatus[3];// 0=free, 1=filled, 2=compress
unsigned char			imageflush;// 0=no action... >0=pause cap(cap). 2=flush(enc), 3=restart from first(enc)-(capwill clear this and restart). 
unsigned char			CameraHeaderData[3][256];
unsigned char			radar_data[3][2048];
unsigned char			CameraFooterData[3][2][256];
unsigned char			logdata[3][256];
int 				processhd[3];
int				processvga[3];
int				processevd[3];
int				processlane[3];
int				qualityStream1;
int				qualityStream2;
int				Zoom_On;
int				Zoom_left;
int				Zoom_right;
int				Zoom_up;
int				Zoom_down;
} ENC_THREAD_ARG;


void *Jpegencthread(void * arg)
{
	ENC_THREAD_ARG		*encarg;
	IMAGE_PARAMETERS	*imgparams[4];
	const unsigned char 	zeromemory[32] = {0};
	unsigned char 		imageparamsmem[4][128];
	int 			imagecompressed = 0;
	NvJPEGEncoder 		*jpegenc;
	int			imageprocnumber = 0, i, j, k;
	struct disk_fat	*imageData;
	unsigned char 		*outimage;
	unsigned long 		sizejpeg = 0;
	COMPRESS_PC_SHARED_RESOURCES *compressPcShr;
	unsigned long 		starttime2 = 0, endtime2 = 0;
	struct timeval		t1;
	unsigned char 		triggerdataflag[32];
	int			Iseq=0, Hseq = 0, Lseq = 0, Eseq = 0, Seq = 0;
	int			rZoom_On;
	int			rZoom_pan = 5;
	int			rZoom_tilt = 5;
	
	
	encarg = (ENC_THREAD_ARG *)arg;// get all the data var..
	compressPcShr = encarg->compressPcShr;
	jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
	imgparams[0] = (IMAGE_PARAMETERS *)imageparamsmem[0];
	imgparams[1] = (IMAGE_PARAMETERS *)imageparamsmem[1];
	imgparams[2] = (IMAGE_PARAMETERS *)imageparamsmem[2];
	imgparams[3] = (IMAGE_PARAMETERS *)imageparamsmem[3];
	imgparams[0]->header = 0x54629871;
	imgparams[0]->org_image_width = 3840;
	imgparams[0]->org_image_height = 2160;
	imgparams[0]->org_image_aspect_x = 16;
	imgparams[0]->org_image_aspect_y = 9;
	for(i = 1; i < 4; i++)
	{
		imgparams[i]->header = imgparams[i-1]->header;
		imgparams[i]->org_image_width = imgparams[i-1]->org_image_width;
		imgparams[i]->org_image_height = imgparams[i-1]->org_image_height;
		imgparams[i]->org_image_aspect_x = imgparams[i-1]->org_image_aspect_x;
		imgparams[i]->org_image_aspect_y = imgparams[i-1]->org_image_aspect_y;
	}
	outimage = (unsigned char *)malloc(3840 * 2160 * 2);
	while(1)
	{
		if(encarg->imageflush == 0)
		{
			if(encarg->imagestatus[imageprocnumber] == 1)
			{
				encarg->imagestatus[imageprocnumber] = 2;
				imagecompressed = 0;
				imageData = (struct disk_fat*)(&encarg->CameraHeaderData[imageprocnumber][0]);
				gettimeofday(&t1, NULL);
				starttime2 = (t1.tv_sec*1000000) + t1.tv_usec;
				if(compressPcShr->PcViewList[3] && encarg->processhd[imageprocnumber])// for ai and for ONVIF...
				{

					i = compressPcShr->PcImageWrNo[3];
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						compressPcShr->PcImageStatus[3][i][j] = IMGFRE;
					}
					sizejpeg = 3840 * 2160 * 1.5;
					jpegenc->setCropRect(0, 0, 3840, 2160);
					if(imageprocnumber == 0)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer1, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
					else if(imageprocnumber == 1)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer2, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
					else //if(imageprocnumber == 0)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer3, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
					if(sizejpeg > MAXTXIMAGESIZE)
						sizejpeg = MAXTXIMAGESIZE;
					imagecompressed = sizejpeg;// size and flag..
					imageData->resolution = EIGHT_MP;
					imgparams[3]->image_resize_type = 0;
					imgparams[3]->image_resize_xfact = 10;
					imgparams[3]->image_resize_yfact = 10;
					imgparams[3]->image_xoffset = 0;
					imgparams[3]->image_yoffset = 0;
					imgparams[3]->image_width = 3840;
					imgparams[3]->image_height = 2160;
					imgparams[3]->image_aspect_x = 16;
					imgparams[3]->image_aspect_y = 9;
					imgparams[3]->image_compressed_size = sizejpeg;
					mempcpy(compressPcShr->PcImageOutEHMemory[i], outimage, sizejpeg);
					mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], zeromemory, 32);
					sizejpeg += 31;
					sizejpeg &= 0xFFFE0;
					mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], imageparamsmem[3], 128);
					sizejpeg += 128;//
					mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], encarg->radar_data[imageprocnumber], 2048);
					sizejpeg += 2048;
					mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], encarg->CameraFooterData[imageprocnumber][0], 192);
					mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
					compressPcShr->PcImageSize[3][i] = sizejpeg+192;
					//--------- populating img fat info..
					imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
					imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
					imageData->channel_info = 0x100;// evidence..
					imageData->longimg_size = sizejpeg+192;
					imageData->logical_channel = 3;
					mempcpy(compressPcShr->CameraImageHeaderData[3][i], encarg->CameraHeaderData[imageprocnumber], 128);
					
					
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						if(compressPcShr->PcImageInit[3][j] == 2)
						{
							compressPcShr->PcImageRdNo[3][j] = compressPcShr->PcImageWrNo[3];
							for(k = 0; k < MAX_RAW_IMAGE_PC_BUFFER; k++)
								compressPcShr->PcImageStatus[3][k][j] = IMGFRE;
							if(compressPcShr->PcImageStreamReq[3][j])
								compressPcShr->PcImageStatus[3][i][j] = IMGRDY;
							compressPcShr->PcImageInit[3][j] = 3;
						}
						else if(compressPcShr->PcImageInit[3][j] == 3)
						{
							if(compressPcShr->PcImageStreamReq[3][j])
								compressPcShr->PcImageStatus[3][i][j] = IMGRDY;
						}
					}
					i++;
					if(i >= MAX_RAW_IMAGE_PC_BUFFER)
						i = 0;
					compressPcShr->PcImageWrNo[3] = i;
				}
				if(compressPcShr->PcViewList[0] && encarg->processlane[imageprocnumber])// need to compress this image
				{
					i = compressPcShr->PcImageWrNo[0];
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						compressPcShr->PcImageStatus[0][i][j] = IMGFRE;
					}
					if(imagecompressed == 0)
					{
						sizejpeg = 3840 * 2160 * 1.5;
						jpegenc->setCropRect(0, 0, 3840, 2160);
						if(imageprocnumber == 0)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer1, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
						else if(imageprocnumber == 1)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer2, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
						else//if(imageprocnumber == 0)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer3, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream1);
						imagecompressed = sizejpeg;// size and flag..
						if(sizejpeg > MAXTXIMAGESIZE)
							sizejpeg = MAXTXIMAGESIZE;
					}
					else
					{
						sizejpeg = imagecompressed;
					}
					imageData->resolution = EIGHT_MP;
					imgparams[0]->image_resize_type = 0;
					imgparams[0]->image_resize_xfact = 10;
					imgparams[0]->image_resize_yfact = 10;
					imgparams[0]->image_xoffset = 0;
					imgparams[0]->image_yoffset = 0;
					imgparams[0]->image_width = 3840;
					imgparams[0]->image_height = 2160;
					imgparams[0]->image_aspect_x = 16;
					imgparams[0]->image_aspect_y = 9;
					imgparams[0]->image_compressed_size = sizejpeg;
					mempcpy(compressPcShr->PcImageOutHMemory[i], outimage, sizejpeg);
					mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], zeromemory, 32);
					sizejpeg += 31;
					sizejpeg &= 0xFFFE0;
					mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], imageparamsmem[0], 128);
					sizejpeg += 128;
					mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], encarg->radar_data[imageprocnumber], 2048);
					sizejpeg += 2048;
					for(j = 0; j < 8; j++)
					{
						if(encarg->CameraFooterData[imageprocnumber][0][j*24] != 0)
						{
							triggerdataflag[j*2] = encarg->CameraFooterData[imageprocnumber][0][(j*24) + 3];
							triggerdataflag[(j*2) + 1] = encarg->CameraFooterData[imageprocnumber][0][(j*24) + 4];
						}
						else
						{
							triggerdataflag[j*2] = 0;
							triggerdataflag[(j*2) + 1] = 0;
						}
					}
					mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], encarg->CameraFooterData[imageprocnumber][0], 192);
					mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
					compressPcShr->PcImageSize[0][i] = sizejpeg+192;
					imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
					imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
					imageData->channel_info = 0x100;// evidence..
					imageData->longimg_size = sizejpeg+192;
					imageData->logical_channel = 0;
					mempcpy(compressPcShr->CameraImageHeaderData[0][i], encarg->CameraHeaderData[imageprocnumber], 128);
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						if(compressPcShr->PcImageInit[0][j] == 2)
						{
							compressPcShr->PcImageRdNo[0][j] = compressPcShr->PcImageWrNo[0];
							for(k = 0; k < MAX_RAW_IMAGE_PC_BUFFER; k++)
								compressPcShr->PcImageStatus[0][k][j] = IMGFRE;
							if(compressPcShr->PcImageStreamReq[0][j])
								compressPcShr->PcImageStatus[0][i][j] = IMGRDY;
							compressPcShr->PcImageInit[0][j] = 3;
						}
						else if(compressPcShr->PcImageInit[0][j] == 3)
						{
							if(compressPcShr->PcImageStreamReq[0][j])
								compressPcShr->PcImageStatus[0][i][j] = IMGRDY;
						}
					}
					i++;
					if(i >= MAX_RAW_IMAGE_PC_BUFFER)
						i = 0;
					compressPcShr->PcImageWrNo[0] = i;
				}
				imagecompressed = 0;
				if(compressPcShr->PcViewList[2] && encarg->processevd[imageprocnumber])// need to compress this image
				{
					i = compressPcShr->PcImageWrNo[2];
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						compressPcShr->PcImageStatus[2][i][j] = IMGFRE;
					}
					sizejpeg = 3840 * 2160 * 1.5;
					jpegenc->setCropRect(0, 0, 3840, 2160);
					jpegenc->setScaledEncodeParams(1280, 720);
					if(imageprocnumber == 0)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer1, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
					else if(imageprocnumber == 1)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer2, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
					else //if(imageprocnumber == 0)
						jpegenc->encodeFromBuffer(*encarg->nvbuffer3, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
					if(sizejpeg > MAXTXIMAGESIZE)
						sizejpeg = MAXTXIMAGESIZE;
					imagecompressed = sizejpeg;// size and flag..
					imageData->resolution = EIGHT_MP;
					imgparams[2]->image_resize_type = 2;
					imgparams[2]->image_resize_xfact = 30;
					imgparams[2]->image_resize_yfact = 30;
					imgparams[2]->image_xoffset = 0;
					imgparams[2]->image_yoffset = 0;
					imgparams[2]->image_width = 1280;
					imgparams[2]->image_height = 720;
					imgparams[2]->image_aspect_x = 16;
					imgparams[2]->image_aspect_y = 9;
					imgparams[2]->image_compressed_size = sizejpeg;
					mempcpy(compressPcShr->PcImageOutELMemory[i], outimage, sizejpeg);
					mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], zeromemory, 32);
					sizejpeg += 31;
					sizejpeg &= 0x7FFE0;
					mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], imageparamsmem[2], 128);
					sizejpeg += 128;
					mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], encarg->radar_data[imageprocnumber], 2048);
					sizejpeg += 2048;
					for(j = 0; j < 8; j++)
					{
						if(encarg->CameraFooterData[imageprocnumber][1][j*24] != 0)
						{
							triggerdataflag[j*2] = encarg->CameraFooterData[imageprocnumber][1][(j*24) + 3];
							triggerdataflag[(j*2) + 1] = encarg->CameraFooterData[imageprocnumber][1][(j*24) + 4];
						}
						else
						{
							triggerdataflag[j*2] = 0;
							triggerdataflag[(j*2) + 1] = 0;
						}
					}
					mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], encarg->CameraFooterData[imageprocnumber][1], 192);
					mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
					compressPcShr->PcImageSize[2][i] = sizejpeg+192;
					//--------- populating img fat info..
					imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
					imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
					imageData->channel_info = 0x100;// evidence..
					imageData->longimg_size = sizejpeg+192;
					imageData->logical_channel = 2;
					mempcpy(compressPcShr->CameraImageHeaderData[2][i], encarg->CameraHeaderData[imageprocnumber], 128);
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						if(compressPcShr->PcImageInit[2][j] == 2)
						{
							compressPcShr->PcImageRdNo[2][j] = compressPcShr->PcImageWrNo[2];
							for(k = 0; k < MAX_RAW_IMAGE_PC_BUFFER; k++)
								compressPcShr->PcImageStatus[2][k][j] = IMGFRE;
							if(compressPcShr->PcImageStreamReq[2][j])
								compressPcShr->PcImageStatus[2][i][j] = IMGRDY;
							compressPcShr->PcImageInit[2][j] = 3;
						}
						else if(compressPcShr->PcImageInit[2][j] == 3)
						{
							if(compressPcShr->PcImageStreamReq[2][j])
								compressPcShr->PcImageStatus[2][i][j] = IMGRDY;
						}
					}
					i++;
					if(i >= MAX_RAW_IMAGE_PC_BUFFER)
						i = 0;
					compressPcShr->PcImageWrNo[2] = i;
				}
				if(compressPcShr->PcViewList[1] && encarg->processvga[imageprocnumber])// need to compress this image
				{
					i = compressPcShr->PcImageWrNo[1];
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						compressPcShr->PcImageStatus[1][i][j] = IMGFRE;
					}
				
					if(imagecompressed == 0)
					{
						sizejpeg = 3840 * 2160 * 1.5;
						if(encarg->Zoom_On == 0)					
						{
							jpegenc->setCropRect(0, 0, 3840, 2160);
							jpegenc->setScaledEncodeParams(1280, 720);
							rZoom_On = 0;
							rZoom_pan = 3;
							rZoom_tilt = 3;
						}
						else
						{
							if(rZoom_On == 0)
							{
								rZoom_On = 1;
								rZoom_pan = 3;
								rZoom_tilt = 3;
							}
							else
							{
								if(encarg->Zoom_left)
								{
									encarg->Zoom_left = 0;
									if(rZoom_pan > 0)
										rZoom_pan--;
								}
								else if(encarg->Zoom_right)
								{
									encarg->Zoom_right = 0;
									if(rZoom_pan < 5)
										rZoom_pan++;
								}
								else if(encarg->Zoom_up)
								{
									encarg->Zoom_up = 0;
									if(rZoom_tilt > 0)
										rZoom_tilt--;
								}
								else if(encarg->Zoom_down)
								{
									encarg->Zoom_down = 0;
									if(rZoom_tilt < 5)
										rZoom_tilt++;
								}
							}
							jpegenc->setCropRect((rZoom_pan * 512), (rZoom_tilt * 288), 1280, 720);
						}
						if(imageprocnumber == 0)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer1, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
						else if(imageprocnumber == 1)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer2, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
						else //if(imageprocnumber == 0)
							jpegenc->encodeFromBuffer(*encarg->nvbuffer3, JCS_YCbCr, &outimage, sizejpeg, encarg->qualityStream2);
						if(sizejpeg > MAXTXIMAGESIZE)
							sizejpeg = MAXTXIMAGESIZE;
					}
					else
					{
						sizejpeg = imagecompressed;
					}
					imageData->resolution = EIGHT_MP;
					imgparams[1]->image_resize_type = 2;
					imgparams[1]->image_resize_xfact = 30;
					imgparams[1]->image_resize_yfact = 30;
					imgparams[1]->image_xoffset = 0;
					imgparams[1]->image_yoffset = 0;
					imgparams[1]->image_width = 1280;
					imgparams[1]->image_height = 720;
					imgparams[1]->image_aspect_x = 16;
					imgparams[1]->image_aspect_y = 9;
					imgparams[1]->image_compressed_size = sizejpeg;

					//------------------------------Maheen--------------------------
					Iseq++;		
					if(imageData->alarm_state > 0)
					{
						Lseq++;
						if(Seq == 0)
							Eseq++;
						Seq = 0;
					}
					else
					{
						Hseq++;							
						if(Seq == 1)
							Eseq++;
						Seq = 1;
					}
					if(imageData->alarm_state == 0)// only high images....
					{
						//------------Maheen--------------------------------------------------------------------------
						WriteJPEGFrameToRTSPServerShdMem(outimage, sizejpeg);	
						//------------Maheen--------------------------------------------------------------------------
					}
					//-----------------------------Maheen------------------------------------
					if((imageData->img_seq_no & 0x1F) == 0)
					{
						//pri ntf("images compressed %d, I=%d, H=%d, L=%d, Err=%d\n",imageData->img_seq_no, Iseq, Hseq, Lseq, Eseq);
						Iseq = 0; Hseq = 0; Lseq = 0; Eseq = 0;
					}

					mempcpy(compressPcShr->PcImageOutLMemory[i], outimage, sizejpeg);
					mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], zeromemory, 32);
					sizejpeg += 31;
					sizejpeg &= 0x7FFE0;
					mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], imageparamsmem[1], 128);
					sizejpeg += 128;
					mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], encarg->radar_data[imageprocnumber], 2048);
					sizejpeg += 2048;
					mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], encarg->CameraFooterData[imageprocnumber][1], 192);
					compressPcShr->PcImageSize[1][i] = sizejpeg+192;
					imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
					imageData->image_type = 1;//MEDIA_JPEG;// 5 jpeg..
					imageData->channel_info = 0x100;// evidence..
					imageData->longimg_size = sizejpeg+192;
					imageData->logical_channel = 1;
					memset(imageData->last_alarm_name, 0, 20);
					mempcpy(compressPcShr->CameraImageHeaderData[1][i], encarg->CameraHeaderData[imageprocnumber], 256);
					for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
					{
						if(compressPcShr->PcImageInit[1][j] == 2)
						{
							compressPcShr->PcImageRdNo[1][j] = compressPcShr->PcImageWrNo[1];
							for(k = 0; k < MAX_RAW_IMAGE_PC_BUFFER; k++)
								compressPcShr->PcImageStatus[1][k][j] = IMGFRE;
							if(compressPcShr->PcImageStreamReq[1][j])
								compressPcShr->PcImageStatus[1][i][j] = IMGRDY;
							compressPcShr->PcImageInit[1][j] = 3;
						}
						else if(compressPcShr->PcImageInit[1][j] == 3)
						{
							if(compressPcShr->PcImageStreamReq[1][j])
								compressPcShr->PcImageStatus[1][i][j] = IMGRDY;
						}						
					}
					i++;
					if(i >= MAX_RAW_IMAGE_PC_BUFFER)
						i = 0;
					compressPcShr->PcImageWrNo[1] = i;
				}
				gettimeofday(&t1, NULL);
				endtime2 = (t1.tv_sec*1000000) + t1.tv_usec;
				encarg->imagestatus[imageprocnumber] = 0;// free..
				imageprocnumber++;
				if(imageprocnumber > 2)
					imageprocnumber = 0;
				usleep(1000);
			}
			else 
				usleep(500);
		}
		else
		{
			if(encarg->imageflush == 1)
			{
				encarg->imageflush = 2;
				imageprocnumber = 0;
				encarg->processhd[0] = 0;
				encarg->processvga[0] = 0;
				encarg->processevd[0] = 0;
				encarg->processlane[0] = 0;
				encarg->processhd[1] = 0;
				encarg->processvga[1] = 0;
				encarg->processevd[1] = 0;
				encarg->processlane[1] = 0;
				encarg->processhd[2] = 0;
				encarg->processvga[2] = 0;
				encarg->processevd[2] = 0;
				encarg->processlane[2] = 0;
				if(encarg->imagestatus[0] > 0)
					encarg->imagestatus[0] = 0;
				if(encarg->imagestatus[1] > 0)
					encarg->imagestatus[1] = 0;
				if(encarg->imagestatus[2] > 0)
					encarg->imagestatus[2] = 0;
				encarg->imageflush = 3;// FINISHED FLUSH..
			}
			usleep(250);
		}
		
	}
	free(outimage);
}

//void CaptureMainProcess(int pipefd) 
int main()
{
	const int minFrameNumber = 1;
	const int maxFrameNumber = 1000;
	const int defFrameNumber = 10;
	int numCameras;
	int numControl;
	bool processing;
	bool saveEmbeddedData;
	void *data;
	uint32_t length;
	static pthread_t encthread, VsyncThread; 	
	void *embeddedData = NULL;
	uint32_t lengthEmbeddedData = 0;
	int i, j, k, l, SensorI2CHandle, vmax = 0, hmax = 0;
 	CIControlList controls;
	IControl *control;    
 	IControl *gaincontrol, *exposurecontrol;
	IImage image;
	int value, valueshutter = 0, initcount = 0;
	int32_t valueread;
	int image_process = 0, shm_handle, shm_handlecopc;
	void *ShMemory, *ShMemorycopc;
	SHARED_RESOURCES *shared_data;
	SHARED_CONFIG_DATA *sharedConfigData;
	COMPRESS_PC_SHARED_RESOURCES *compressPcShr;
	struct disk_fat	*imageData;
	unsigned char CameraHeaderData[256],CameraFooterData[192], radar_data[2048];
	const int frameCount = 300;//SelectValue("Numbers of frames you wish to save", minFrameNumber, maxFrameNumber, defFrameNumber);
	char folder[PATH_MAX];
	int frameSaved = 0, VsyncCount = 0, cap_seq_no;
	char frameNaming[256];
	unsigned char *rawimage, *ybuf, *ubuf, *vbuf, crntyear = 0;
	time_t prev_time_in_sec;
	time_t yrtime;
	unsigned int seqno;
	struct timeval t1, t2, tval;
	struct tm brokentime;
	unsigned long starttime = 0, endtime = 0;
	unsigned long starttime1 = 0, endtime1 = 0;
	unsigned long starttime2 = 0, endtime2 = 0;
	unsigned long starttime3 = 0, endtime3 = 0;
	int memfilefd, nvbuffd;
	int color, skipframes, skipframecount, qualityStream, img_exposure_type, prev_img_exposure_type, imginsec;
	TEMBEDDEDDATA565 embdata;
	int src_dma_fd = -1;
	int dualshutterval = 0;
	int shutterval = 0, gainval = 0;
	int shutterpairwrcnt = 0;
	int shutterset[2][2] = {0};// [current0/prev1][firstval, secondval]
	const char *queryControls[] = {"Gain", "Digital Gain", "Frame Rate", "Exposure", "Black Level"};
	unsigned short deltatimeval = 0;
	int sensorShutter = 0, sensorShutter1 = 0, sensorShutter2 = 0, sensorShutterdual1 = 0, sensorShutterdual2 = 0, sensorShutterdualval1 = 0, sensorShutterdualval2 = 0, sensorShutterAGC = 0,sensorShutterdualAGC1 = 0, sensorShutterdualAGC2 = 0,  
		flash_on_off = 0, IcrControl = 0, sensorGain = 0, sensorGain1 = 5, sensorGain2 = 5, shutter = 0, 
		dual_capture_on = 0, dual_capture_onflag = 0, sensorAperture = 0, sensorGamma = 0, sensorGamma1 = 0, imgGamma, imgGamma1 = 1, imgGamma2 = 1,nighttime = 0, lighttable = 0;
	float wbr = 1.7, wbgr = 1.0, wbgb = 1.0, wbb = 1.9;
	short roff = 40, g1off = 40, g2off = 40, boff = 40;
	unsigned long capturetime = 0, triggertime = 0;
	RADAR_ALL_VEH_BUFF 	*adjacentvehiclebuff, *adjacentvehiclemem;
	const float lineperiod = 26.9;//uSec
	int colorcapture[2] = {0};
	ENC_THREAD_ARG	encthrdvar;
	int imageprocnumber = 0;
	int ShutterTimeUS[33] = {
		55,//0
		89,
		102,
		145,
		180,
		210,
		250,
		290,
		320,
		370,
		450,//10
		500,
		600,
		710,
		800,
		900,
		1000,//16
		2000,
		3000,
		4000,
		5000,//20
		6000,
		8000,
		10000,
		12000,
		14000,//25
		16000,
		18000,
		20000,
		22000,
		25000,//30
		35000,
		40000
		};
	struct timespec		pctime, cappctime;
	int IMX264AnalogGain1[8] = {
		0,
		34,//
		69,
		103,
		137,
		171,
		206,
		240
		};
	float	gammaValue[12] = {1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4, 2.45, 2.5, 2.55, 2.6, 2.65}, loadgamma;
	float	gammaValue1[12] = {1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4, 2.45, 2.5, 2.55, 2.6, 2.65};
//	float	gammaValue1[12] = {0.95, 1.0, 1.1, 1.2, 1.4, 1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4};

	int frametime = 41667;//uS
	float linetime = 1.0;
	AGCCONTEXT AGCStream[2];
	static unsigned char gammaRevTable[256], gammaRevTable1[256];
	static int prevgamma = 5, prevgamma1 = 5;
	int avg, centeravg, avg23rds, luxLUTVal,  luxvals[512] = {0};
	int avg1, centeravg1, avg23rds1, algotarget, algodir, algotarget1, algodir1, algoGmax, algoResp, algoGmin, algoSmax, algoSmin;
	int PrevAppliedShutter = 0, PrevAppliedGain = 0, PrevAppliedShutter1 = 0, PrevAppliedGain1 = 0, crnt_dualcapimg = 0;
	char firmware[32] = {" -no/fw- "};
	FILE *fptr;
	//unsigned char cameratype = 0;// Ai only
	unsigned char cameratype = 1;// Ai only
	pid_t pid;
	VSYNC_THREAD_ARG	syncarg;
	int	ExtTrigEn = 0, nighttimeExtTrig;
	LPU_AICAM_AGC_CFG_META_DATA lightsettingsptr;

	SensorI2CHandle = 0;
	imageData = (struct disk_fat*)CameraHeaderData;
	memset(imageData->last_alarm_name, 0, 20);
	shm_handle = shm_open(SHM_NAME, O_RDWR, 0660);
	ShMemory = mmap(NULL, MAKESIZE4MASK(SHM_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handle, 0);
	shared_data = (SHARED_RESOURCES*)ShMemory;

	while(shared_data->validID != 0x46392715)
		usleep(10000);//10ms
	
	while(shared_data->PcStreamStatus == PROCESSSTATUS_IDLE)
	{
		usleep(10000);
	}
	shm_handlecopc = shm_open(SHM_NAME_COPC, O_RDWR, 0660);
	ShMemorycopc = mmap(NULL, MAKESIZE4MASK(SHM_SIZE_COPC), PROT_READ | PROT_WRITE, MAP_SHARED, shm_handlecopc, 0);
	compressPcShr = (COMPRESS_PC_SHARED_RESOURCES*)ShMemorycopc;
	adjacentvehiclebuff = (RADAR_ALL_VEH_BUFF*)shared_data->radar_all_veh_data;
	adjacentvehiclemem = (RADAR_ALL_VEH_BUFF*)radar_data;
	shared_data->CaptureStart = 0;
	shared_data->captureTrigger1 = 1;
	shared_data->ImgCapRequired = 1;
	while(shared_data->SyncStatus == 0)
	{
		usleep(10000);
	}
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;// to get configuration..

	lightsettingsptr = (LPU_AICAM_AGC_CFG_META_DATA)(&sharedConfigData->womensafety_camera_parameters[0]);

	//Use sv_GetAllCameras(int* size) to get a list of available cameras
	CICameraList cameras = sv_GetAllCameras(&numCameras);
	if (numCameras == 0) 
	{
		return 0;
	}
	
	ExtTrigEn = 0;
	if((lightsettingsptr->data.Header.marker1 == AICAM_AGC_HDR_M1) && (lightsettingsptr->data.Header.marker2 == AICAM_AGC_HDR_M2))
		if(lightsettingsptr->data.Header.NightModeTrigflag == 1)
		{
			ExtTrigEn = 1;
		}
	
	//Select one of available cameras 
	ICamera *camera = cameras[0];//SelectCamera(&cameras, numCameras);

	processing = false;//SelectEnable("platform-specific processing", false);
	
	//nice(-10);

	saveEmbeddedData = true;//SelectEnable("save embedded data", false);
InitV4l2Again:   
	control = sv_camera_GetControl(camera, SV_V4L2_IMAGEFORMAT);
	if (control) {
	//SelectPixelFormat(control);
	//    	int value = 2;//12bit
		value = 1;//10bit
		sv_control_Set(control, value);
	}

	// Use sv_camera_GetControlList() to get a list of available controls
	controls = sv_camera_GetControlList(camera, &numControl);

	// Use SV_V4L2_FRAMESIZE to set resolution
	//SelectFrameSize(sv_camera_GetControl(camera, SV_V4L2_FRAMESIZE));
	control = sv_camera_GetControl(camera, SV_V4L2_FRAMESIZE);
	if (control) {
	//		int value = 0;// 4128x3008
		value = 1;// 3840x2160
		sv_control_Set(control, value);
	}

	// Set other controls matched by name

	for (i = 0; i < numControl; i++) 
	{
		const char *name = sv_control_GetName((IControl*)controls[i]); 
		if (CheckQueryBool(name, safe_strlen(name, 16), queryControls, ARRAY_LENGTH(queryControls))) 
		{
			value = 0;//SelectValue(name, sv_control_GetMinValue((IControl*)controls[i]), sv_control_GetMaxValue((IControl*)controls[i]), sv_control_GetDefaultValue((IControl*)controls[i]));
			valueread = sv_control_Get((IControl*)controls[i]);
			if(strcmp("Gain", name) == 0)
			{
				value = 240;
				//value = 400;
				gaincontrol = (IControl*)controls[i];
		    	}
			else if(strcmp("Digital Gain", name) == 0)
			{
				value = 80;
			}
			else if(strcmp("Exposure", name) == 0)
			{
				value = 40000;
				exposurecontrol = (IControl*)controls[i];
			}
			else if(strcmp("Black Level", name) == 0)
			{
				value = 60;
		    	}
			else if(strcmp("Frame Rate", name) == 0)
			{
				value = 20000000;
				control = (IControl*)controls[i];
		    	}
		    	if(value)
		   		sv_control_Set((IControl*)controls[i], value);
		}
	}
	for (i = 0; i < numControl; i++) 
	{
		const char *name = sv_control_GetName((IControl*)controls[i]); 
		if (CheckQueryBool(name, safe_strlen(name,16), queryControls, ARRAY_LENGTH(queryControls))) 
		{
			valueread = sv_control_Get((IControl*)controls[i]);
		}
	}
	if(SensorI2CHandle == 0)
		InitSensorI2c1(&SensorI2CHandle);
	initcount++;
	value = 22000000;
	const char *name = sv_control_GetName((IControl*)control); 
	valueread = sv_control_Get((IControl*)control);
	if(valueread != value)
	{
		if(initcount < 3)
			goto InitV4l2Again;
	}

//	StopExtTrig(SensorI2CHandle);

	RegInitSensorI2c(SensorI2CHandle, &vmax, &hmax);
	//Use sv_camera_StartStream() to start acquiring frames
	fptr = fopen("../version.txt","rb");
	if(fptr != NULL)
	{
		i=fread(firmware, 16, 1, fptr);
		fclose(fptr);
	}
//	cameratype = (strstr(firmware, "F248_2")!= NULL)?1:0;// 0=Ai only 1=svds with trigger

	if (sv_camera_StartStream(camera) == 0) 
	{
		return 0;
	}
	InitSensorI2c2(&SensorI2CHandle, 1);//res 0=12Mp, else 8Mp
	RegInitSensorI2c(SensorI2CHandle, &vmax, &hmax);
	//GetCurrentWorkingDir(folder);

	//Allocate processing buffers
	//IProcessedImage processedImage = sv_AllocateProcessedImage(sv_camera_GetImageInfo(camera));

	NvBuffer buffer1(V4L2_PIX_FMT_YUV420M, 3840, 2160, 0);//5m, 5m/4, 5m/4
	buffer1.planes[0].bytesused = 3840 * 2160;
	buffer1.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer1.planes[2].bytesused = (3840 * 2160) >> 2;
	buffer1.allocateMemory();
	buffer1.planes[0].bytesused = 3840 * 2160;
	buffer1.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer1.planes[2].bytesused = (3840 * 2160) >> 2;
	
	NvBuffer buffer2(V4L2_PIX_FMT_YUV420M, 3840, 2160, 0);//5m, 5m/4, 5m/4
	buffer2.planes[0].bytesused = 3840 * 2160;
	buffer2.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer2.planes[2].bytesused = (3840 * 2160) >> 2;
	buffer2.allocateMemory();
	buffer2.planes[0].bytesused = 3840 * 2160;
	buffer2.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer2.planes[2].bytesused = (3840 * 2160) >> 2;

	NvBuffer buffer3(V4L2_PIX_FMT_YUV420M, 3840, 2160, 0);//5m, 5m/4, 5m/4
	buffer3.planes[0].bytesused = 3840 * 2160;
	buffer3.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer3.planes[2].bytesused = (3840 * 2160) >> 2;
	buffer3.allocateMemory();
	buffer3.planes[0].bytesused = 3840 * 2160;
	buffer3.planes[1].bytesused = (3840 * 2160) >> 2;
	buffer3.planes[2].bytesused = (3840 * 2160) >> 2;

	image_process = 1;// to process image..
	qualityStream = sharedConfigData->general_details.primary_stream_quality[0];
	//for (i = 0; i < frameCount; i++) 
	if(sharedConfigData->general_details.primary_stream_fps[0] == 0)
	{
		skipframes = 1;	
	}
	else if(sharedConfigData->general_details.primary_stream_fps[0] == 1)
	{
		skipframes = 1;	
	}
	else if(sharedConfigData->general_details.primary_stream_fps[0] == 2)
	{
		skipframes = 2;	
	}
	else
	{
		skipframes = 3;	
	}
	skipframecount = 3;
//	colorcapture[0] = 0;// first cap
//	colorcapture[1] = 0;// second cap
	if(sharedConfigData->camera_parameters[0].DayNight_ColourMode == 0)// normal day night// no color..
	{
		colorcapture[0] = 0;// first cap
		colorcapture[1] = 0;// second cap
	}
	else if(sharedConfigData->camera_parameters[0].DayNight_ColourMode == 2)// all color ..
	{
		colorcapture[0] = 0;// first cap
		colorcapture[1] = 1;// second cap
	}
	else //if(sharedConfigData->camera_parameters[0].DayNight_ColourMode == 2)// second capture color..
	{
		colorcapture[0] = 1;// first cap
		colorcapture[1] = 1;// second cap
	}
	shared_data->CaptureStart = 1;

	//------------Maheen--------------------------------------------------------------------------
	fptr = fopen("cameraport","wb");
	if(fptr != NULL)
	{
		fwrite(sharedConfigData->ip_details.port_num, 1, 4, fptr);
		fclose(fptr);
	}
	fptr = fopen("wsport","wb");
	if(fptr != NULL)
	{
		fwrite(sharedConfigData->ip_details.ws_port_num, 1, 4, fptr);
		fclose(fptr);
	}
	fptr = fopen("h264rtspport","wb");
	if(fptr != NULL)
	{
		fwrite(sharedConfigData->ip_details.h264rtsp_portnum, 1, 4, fptr);
		fclose(fptr);
	}
	fptr = fopen("jpegrtspport","wb");
	if(fptr != NULL)
	{
		fwrite(sharedConfigData->ip_details.jpgrtsp_portnum, 1, 4, fptr);
		fclose(fptr);
	}
	
	ConfigureRTSPServerShdMem(EIGHT_MP, (unsigned char *)sharedConfigData->remote_user, (unsigned char *)&sharedConfigData->ip_details);	
	SetJPEGFPSToRTSPServerShdMem(skipframes);
	//------------Maheen--------------------------------------------------------------------------

	encthrdvar.compressPcShr = compressPcShr;
	encthrdvar.nvbuffer1 = &buffer1;
	encthrdvar.nvbuffer2 = &buffer2;
	encthrdvar.nvbuffer3 = &buffer3;
	encthrdvar.imageflush = 1;
	encthrdvar.imagestatus[0] = 0;
	encthrdvar.imagestatus[1] = 0;
	encthrdvar.imagestatus[2] = 0;

	encthrdvar.Zoom_On = 0;
	encthrdvar.Zoom_left = 0;
	encthrdvar.Zoom_right = 0;
	encthrdvar.Zoom_up = 0;
	encthrdvar.Zoom_down = 0;

	syncarg.Valid = 0;// init
	syncarg.SensorHandle = SensorI2CHandle;
	syncarg.ShutterSet = 0;
	syncarg.Shutter1Set = 0;
	syncarg.Shutter2Set = 0;
	syncarg.mode = 0;
	syncarg.ExtTrigEn = 0;
	syncarg.GainSet = 0;
	syncarg.Gain1Set = 0;
	syncarg.Gain2Set = 0;
	
	pthread_create(&VsyncThread,NULL,&Vsyncthread, (void*)&syncarg);
	usleep(1000);
	pthread_create(&encthread,NULL,&Jpegencthread, (void*)&encthrdvar);
	imageprocnumber = 0;
	pthread_setname_np(encthread, "JpegEnc");
	
	setGammaReverseCurveConstant(gammaValue[0], gammaRevTable1);
	setGammaReverseCurveConstant(gammaValue1[0], gammaRevTable);
	
	luxLUTVal = 0;
	avg = centeravg = avg23rds = 0x80;
	AGCStream[0].init = 1;
	AGCStream[0].AvgSel = 1;//0=Center, 1=center2/3rd, 2=full
	AGCStream[0].ResponseTime = 8;
	AGCStream[0].shuttermax = 1250;
	AGCStream[0].shuttermin = 400;
	AGCStream[0].gainmax = 320;
	AGCStream[0].gainmin = 0;
/*	AGCStream[0].AgcPmLowThreshold = 0x78;
	AGCStream[0].AgcTargetLowThreshold = 0x88;
	AGCStream[0].AgcTargetThreshold = 0xA0;
	AGCStream[0].AgcTargetHighThreshold = 0xB8;
	AGCStream[0].AgcPmHighThreshold = 0xC8;
	*/
	AGCStream[0].AgcPmLowThreshold		= 0x68;
	AGCStream[0].AgcTargetLowThreshold	= 0x78;
	AGCStream[0].AgcTargetThreshold	= 0x90;
	AGCStream[0].AgcTargetHighThreshold	= 0xA8;
	AGCStream[0].AgcPmHighThreshold	= 0xB8;

	AGCStream[1].init = 1;
	AGCStream[1].AvgSel = 1;
	AGCStream[1].ResponseTime = 8;
	AGCStream[1].shuttermax = 850;
	AGCStream[1].shuttermin = 22;
	AGCStream[1].gainmax = 50;
	AGCStream[1].gainmin = 0;
	AGCStream[1].AgcPmLowThreshold 	= 0x48;
	AGCStream[1].AgcTargetLowThreshold 	= 0x38;
	AGCStream[1].AgcTargetThreshold 	= 0x50;
	AGCStream[1].AgcTargetHighThreshold 	= 0x68;
	AGCStream[1].AgcPmHighThreshold 	= 0x78;

	syncarg.Valid = 1;
	
	//nice(-5);
	linetime = frametime/vmax;
/*	fptr = fopen("pidcap.txt","wb");
	if(fptr != NULL)
	{
		pid = getpid();
		spr intf(frameNaming,"Cap PID = %d\n",pid);
		fwrite(frameNaming, 1, 20, fptr);
		fclose(fptr);
	}*/
	while(1)
	{
		image = sv_camera_GetImage(camera);
		if(image_process)
		{
			if (image.data == NULL) 
			{
				usleep(500);
				continue;
			}
			else
			{
				if(encthrdvar.imageflush == 3)
				{
					encthrdvar.imageflush = 0;
					imageprocnumber = 0;
				}
			
				if(shared_data->Zoom_init)
					encthrdvar.Zoom_On = 1;
				else
					encthrdvar.Zoom_On = 0;
				if(shared_data->Zoom_left)
				{
					encthrdvar.Zoom_left = 1;
					shared_data->Zoom_left = 0;
				}
				if(shared_data->Zoom_right)
				{
					encthrdvar.Zoom_right = 1;
					shared_data->Zoom_right = 0;
				}
				if(shared_data->Zoom_up)
				{
					encthrdvar.Zoom_up = 1;
					shared_data->Zoom_up = 0;
				}
				if(shared_data->Zoom_down)
				{
					encthrdvar.Zoom_down = 1;
					shared_data->Zoom_down = 0;
				}

				if(shared_data->captureTrigger1 == 0)
					shared_data->captureTrigger1 = 1;
				VsyncCount++;
				//deltatimeval = ((image.timestamp.s & (unsigned int)0x1F)*1000)+(image.timestamp.us / 1000);// capture time in milliseconds maxvalue  32seconds.
				embeddedData = image.embeddedData;
				lengthEmbeddedData = image.embeddedDataWidth * image.embeddedDataHeight * 2;
				getembeddeddata((unsigned char *)image.embeddedData, 1, (unsigned char *)&embdata);
				shutterval = 0;
				color = 1;
				shutterval += (embdata.shutterspeed[0] << 16);
				shutterval += (embdata.shutterspeed[1] << 8);
				shutterval += (embdata.shutterspeed[2]);
				gainval = 0;
				gainval += ((embdata.gain[0] & 1) << 8);
				gainval += ( embdata.gain[1]);
				clock_gettime(CLOCK_MONOTONIC_RAW, &pctime);// time from start of pc.
				capturetime = (((pctime.tv_sec* 1000) + (pctime.tv_nsec / 1000000)) - 49);// check for
				cappctime.tv_nsec = (capturetime % 1000) * 1000000;
				cappctime.tv_sec = capturetime / 1000;
				//capturetime = (image.timestamp.s * 1000) + (image.timestamp.us / 1000);////0, triggertime = 0;
				// check for shutter... to determine first or second capture. check for previous shutter also.
				//shutterset[2][2] = {0};// [current0/prev1][firstval, secondval]
				if(dual_capture_on)
				{
					if((IcrControl) || (nighttime))
					{
						if((shutterval & 1) == 0)// even...
						{
							img_exposure_type = 0;//high
							crnt_dualcapimg = 0;
						}
						else
						{
							crnt_dualcapimg = 1;
							img_exposure_type = 1;//low
							if(IcrControl)
								color = 0;
							if(nighttime)
								color = 0;
						}
					}
					else
					{
						if((shutterval & 1) == 0)// even...
						{
							img_exposure_type = 1;//high
							crnt_dualcapimg = 0;
						}
						else
						{
							crnt_dualcapimg = 1;
							img_exposure_type = 0;//low
							if(IcrControl)
								color = 0;
							if(nighttime)
								color = 0;
						}
					}
				}
				else
				{
					crnt_dualcapimg = 1;
					img_exposure_type = 0;// always high
					if(nighttime)
						color = 0;
				}
				if((IcrControl) || (nighttime))
				{
					color = colorcapture[img_exposure_type];
					img_exposure_type = (img_exposure_type)?0:1;
				}
				gettimeofday(&tval,NULL);// for timing... capture_s capture_us
				if(img_exposure_type == 0)
					cap_seq_no = get_Seqnumber(tval.tv_sec, &crntyear, &yrtime, &seqno);
					
				img_exposure_type = (img_exposure_type)?0:1;
					
				encthrdvar.processevd[imageprocnumber] = 0;
				encthrdvar.processlane[imageprocnumber] = 0; 
				encthrdvar.processhd[imageprocnumber] = 0;
				encthrdvar.processvga[imageprocnumber] = 0;
				
				compressPcShr->PcViewList[1]=1;
				if((compressPcShr->PcViewList[0])
				|| (compressPcShr->PcViewList[1])
				|| (compressPcShr->PcViewList[2]) 
				|| (compressPcShr->PcViewList[3]))// need to compress this image
				{
					gettimeofday(&t1, NULL);
					starttime = (t1.tv_sec*1000000) + t1.tv_usec;
					//t1.tv_sec = image.timestamp.s;
					deltatimeval = ((t1.tv_sec & (unsigned int)0x1F)*1000)+(t1.tv_usec / 1000);// capture time in milliseconds maxvalue  32seconds.
					localtime_r(&t1.tv_sec, &brokentime);
					imginsec++;
					if(t1.tv_sec != prev_time_in_sec)
						imginsec = 0;
					prev_time_in_sec = t1.tv_sec;
					imageData->magic = 0xABBA;
					imageData->dvrmodel = 0xA2; // 00 model no  .. A0 for ats tk1, A1 for RLVD tk1, 0xA2=12m Camera...
					imageData->record_cam_list = cap_seq_no;
					if(dual_capture_on)
						imageData->alarm_state = (img_exposure_type)?0:1;//SensorContCapCount;//exposure_type[1][enc_read_buf[1]];//first / second image//0x10101;// treated as trigger flag
					else
						imageData->alarm_state = 0;
					imageData->img_seq_no = VsyncCount;// vsync count of frame..//channel_img_seq_no[1]++;
					imageData->channel_seq_no = cap_seq_no;
					imageData->qlevel = deltatimeval;
					imageData->ntsc_pal = 0;//signal;// red off =0, red signal on = 1;
					//imageData->ntsc_pal = (unsigned short)(t1.tv_usec/1000);// milliSecs; //0;//signal;// red off =0, red signal on = 1;
					imageData->second = brokentime.tm_sec;
					imageData->minute = brokentime.tm_min;
					imageData->hour = brokentime.tm_hour;
					imageData->date = brokentime.tm_mday;
					imageData->month = brokentime.tm_mon;
					imageData->year = brokentime.tm_year;// from 1900
					imageData->dummy = imginsec;// + svImageCapCount[j];// frame no in this sec
					luxLUTVal = shared_data->lux;
					imageData->lux_n_table_no = (luxLUTVal << 8) | lighttable;// light table and lux info..
					imageData->pts=cap_seq_no;
					if(nighttime)// night table..
					{
						switch(qualityStream)
						{
						case 0:
							encthrdvar.qualityStream1 = 85;
							encthrdvar.qualityStream2 = 85;
							break;
						case 1:
							encthrdvar.qualityStream1 = 78;
							encthrdvar.qualityStream2 = 78;
							break;
						case 2:
							encthrdvar.qualityStream1 = 70;
							encthrdvar.qualityStream2 = 70;
							break;
						default:
							encthrdvar.qualityStream1 = 60;
							encthrdvar.qualityStream2 = 60;
							break;
						}
					}
					else
					{
						switch(qualityStream)
						{
						case 0:
							encthrdvar.qualityStream1 = 80;
							encthrdvar.qualityStream2 = 80;
							break;
						case 1:
							encthrdvar.qualityStream1 = 70;
							encthrdvar.qualityStream2 = 70;
							break;
						case 2:
							encthrdvar.qualityStream1 = 60;
							encthrdvar.qualityStream2 = 60;
							break;
						default:
							encthrdvar.qualityStream1 = 50;
							encthrdvar.qualityStream2 = 50;
							break;
						}
					}
//					i = sharedConfigData->womensafety_camera_parameters[63].shutter;
//					qualityStream1 = qualityStream2 = (80 - (i * 2));
//					if(qualityStream1 > 80)
//					{
//						qualityStream1 = qualityStream2 = 80;
//					}
					skipframecount++;
					if(dual_capture_on)
					{
						if(skipframecount > skipframes)
						{
							skipframecount = 0;
							encthrdvar.processhd[imageprocnumber] = 1;
							encthrdvar.processvga[imageprocnumber] = 1;
						}
						else if(skipframecount == skipframes)
						{
							encthrdvar.processhd[imageprocnumber] = 1;
							encthrdvar.processvga[imageprocnumber] = 1;
						}
					}
					else
					{
						if(skipframecount > skipframes)
						{
							skipframecount = 0;
							encthrdvar.processhd[imageprocnumber] = 1;
							encthrdvar.processvga[imageprocnumber] = 1;
						}
					}

					if(adjacentvehiclebuff->header == 0x40f19243)// radar_data
					{
						mempcpy(radar_data, (unsigned char *)shared_data->radar_all_veh_data, 2048);
						//adjacentvehiclemem->imSecs = imageData->ntsc_pal;
						adjacentvehiclemem->imSecs = (unsigned short)(cappctime.tv_nsec / 1000000);
						localtime_r(&cappctime.tv_sec, &brokentime);
						adjacentvehiclemem->isec = brokentime.tm_sec;
						adjacentvehiclemem->imin = brokentime.tm_min;
						adjacentvehiclemem->ihour = brokentime.tm_hour;
						adjacentvehiclemem->idate = brokentime.tm_mday;
						adjacentvehiclemem->imon = brokentime.tm_mon;
						adjacentvehiclemem->iyear = brokentime.tm_year;// from 1900
					}
					memset(encthrdvar.CameraFooterData[imageprocnumber][0], 0, 192);
					memset(encthrdvar.CameraFooterData[imageprocnumber][1], 0, 192);
					for(i = 0, j = 0, k = 0; i < 8; i++)
					{
						
						if(shared_data->capture_serial_trigger[i])
						{
							if(shared_data->capture_serial_trigger_lane_ack[i] == 0)// to start lane..
							{
								triggertime = shared_data->capture_serial_trigger_timesec[i] *1000 + shared_data->capture_serial_trigger_timemilsec[i];
//								if(triggertime <= (capturetime - 150))// image ready....
//								{
									if(triggertime < capturetime)
									{
										encthrdvar.processlane[imageprocnumber] = 1;
										shared_data->capture_serial_trigger_lane_ack[i] = 1;
										mempcpy(&encthrdvar.CameraFooterData[imageprocnumber][0][j*24], shared_data->capture_serial_data[i], 24);
										j++;
									}
//								}
							}
							else if(shared_data->capture_serial_trigger_lane_ack[i] == 1)// second cap..
							{
								if(dual_capture_on)
								{
										encthrdvar.processlane[imageprocnumber] = 1;
										shared_data->capture_serial_trigger_lane_ack[i] = 2;
										mempcpy(&encthrdvar.CameraFooterData[imageprocnumber][0][j*24], shared_data->capture_serial_data[i], 24);
										j++;
								}
								else
								{
										shared_data->capture_serial_trigger_lane_ack[i] = 3;
								}
							}
							else //if(shared_data->capture_serial_trigger_lane_ack[i] > 1)// finish
							{
								shared_data->capture_serial_trigger_lane_ack[i] = 3;
							}

							if(img_exposure_type == 1)// always high
							{
								if(shared_data->capture_serial_trigger_evidence_ack[i] == 0)// to start evidence...
								{
									triggertime = shared_data->capture_serial_trigger_timesec[i] *1000 + shared_data->capture_serial_trigger_timemilsec[i];
//									if(triggertime <= capturetime)// image ready....
//									{
										if(triggertime < capturetime)
										{
											encthrdvar.processevd[imageprocnumber] = 1;
											shared_data->capture_serial_trigger_evidence_ack[i] = 1;
											mempcpy(&encthrdvar.CameraFooterData[imageprocnumber][1][k*24], shared_data->capture_serial_data[i], 24);
											k++;
										}
//									}
								}
								else if(shared_data->capture_serial_trigger_evidence_ack[i] == 1)// second cap..
								{
									if(shared_data->capture_serial_trigger[i] == 1)// violation
									{
										encthrdvar.processevd[imageprocnumber] = 1;
										shared_data->capture_serial_trigger_evidence_ack[i] = 2;
										mempcpy(&encthrdvar.CameraFooterData[imageprocnumber][1][k*24], shared_data->capture_serial_data[i], 24);
										k++;
									}
									else
									{
										shared_data->capture_serial_trigger_evidence_ack[i] = 3;
									}
								}
								else //if(shared_data->capture_serial_trigger_evidence_ack[i] > 1)// finish
								{
									shared_data->capture_serial_trigger_evidence_ack[i] = 3;
								}
							}
							if((shared_data->capture_serial_trigger_lane_ack[i] == 3) && (shared_data->capture_serial_trigger_evidence_ack[i] == 3))
								shared_data->capture_serial_trigger[i] = 0;
						}
					}
					if(cameratype == 0)// ai only
					{
						encthrdvar.processevd[imageprocnumber] = 0;
						encthrdvar.processlane[imageprocnumber] = 0;
					}
					if(imageData->alarm_state)
					{
						//imgGamma = sensorGamma & 0x07;
						imgGamma = sensorGamma1;
						loadgamma = gammaValue[imgGamma];
					}
					else
					{
						//imgGamma = (sensorGamma >> 4) & 0x07;
						imgGamma = sensorGamma;
						loadgamma = gammaValue1[imgGamma];
					}
					length = image.width * image.height * 1.5;
					// color and b&w processed in GPU
					wbr = (sharedConfigData->wb_details.wb_red_gain/(float)1000);			
					wbgr = (sharedConfigData->wb_details.wb_green1_gain/(float)1000);
					wbgb = (sharedConfigData->wb_details.wb_green2_gain/(float)1000);
					wbb = (sharedConfigData->wb_details.wb_blue_gain/(float)1000);
					roff = (sharedConfigData->wb_details.wb_red_offset > 255)? 255:(sharedConfigData->wb_details.wb_red_offset > 4)? sharedConfigData->wb_details.wb_red_offset: 4;
					g1off = (sharedConfigData->wb_details.wb_green1_offset > 255)? 255:(sharedConfigData->wb_details.wb_green1_offset > 4)? sharedConfigData->wb_details.wb_green1_offset: 4;
					g2off = (sharedConfigData->wb_details.wb_green2_offset > 255)? 255:(sharedConfigData->wb_details.wb_green2_offset > 4)? sharedConfigData->wb_details.wb_green2_offset: 4;
					boff = (sharedConfigData->wb_details.wb_blue_offset > 255)? 255:(sharedConfigData->wb_details.wb_blue_offset > 4)? sharedConfigData->wb_details.wb_blue_offset: 4;
	
					if(imageprocnumber == 0)
					{
						MTXcudaISPNVbuffer((unsigned char *)image.data, (unsigned char *)buffer1.planes[0].data, (unsigned char *)buffer1.planes[1].data, (unsigned char *)buffer1.planes[2].data, image.width, image.height, loadgamma, wbr, wbgr, wbgb, wbb, roff, g1off, g2off, boff, color);
					}
					else if(imageprocnumber == 1)
					{
						MTXcudaISPNVbuffer((unsigned char *)image.data, (unsigned char *)buffer2.planes[0].data, (unsigned char *)buffer2.planes[1].data, (unsigned char *)buffer2.planes[2].data, image.width, image.height, loadgamma, wbr, wbgr, wbgb, wbb, roff, g1off, g2off, boff, color);
					}
					else //if(imageprocnumber == 0
					{
						MTXcudaISPNVbuffer((unsigned char *)image.data, (unsigned char *)buffer3.planes[0].data, (unsigned char *)buffer3.planes[1].data, (unsigned char *)buffer3.planes[2].data, image.width, image.height, loadgamma, wbr, wbgr, wbgb, wbb, roff, g1off, g2off, boff, color);
					}
					mempcpy(encthrdvar.CameraHeaderData[imageprocnumber],CameraHeaderData,128);
					mempcpy(encthrdvar.radar_data[imageprocnumber],radar_data,2048);
					sv_camera_ReturnImage(camera, image);

					gettimeofday(&t1, NULL);
					starttime1 = (t1.tv_sec*1000000) + t1.tv_usec;
					avg = 20; centeravg = 25; avg23rds = 30;
					if(imageprocnumber == 0)
					{
						get_brightness_image((unsigned char *)buffer1.planes[0].data, image.width, image.height, &avg, &centeravg, &avg23rds);
					}
					else if(imageprocnumber == 1)
					{
						get_brightness_image((unsigned char *)buffer2.planes[0].data, image.width, image.height, &avg, &centeravg, &avg23rds);
					}
					else //if(imageprocnumber == 0)
					{
						get_brightness_image((unsigned char *)buffer3.planes[0].data, image.width, image.height, &avg, &centeravg, &avg23rds);
					}
					gettimeofday(&t2, NULL);
					endtime1 = (t2.tv_sec*1000000) + t2.tv_usec;

					imgGamma1 = sensorGamma;// & 0x07;
					if(!((IcrControl) || (nighttime)))
					{
						if(dual_capture_on)
						{
							if(crnt_dualcapimg == 0)//low image
							{
								AGCStream[1].avgfull = gammaRevTable1[avg];
								AGCStream[1].avg23rd = gammaRevTable1[avg23rds];
								AGCStream[1].avgcent = gammaRevTable1[centeravg];
								avg23rds1 = AGCStream[0].avg;
								avg1 = AGCStream[0].avg23rd;
								algotarget = AGCStream[0].AgcTargetThreshold;
								algodir = AGCStream[0].dir;
								algoGmax = AGCStream[0].gainmax;
								algoGmin = AGCStream[0].gainmin; 
								algoSmax = AGCStream[0].shuttermax;
								algoSmin = AGCStream[0].shuttermin;
								algoResp = AGCStream[0].ResponseTime;
							}
							else
							{
								imgGamma2 = sensorGamma1;//(sensorGamma >> 4) & 0x07;
								AGCStream[0].avgfull = gammaRevTable[avg];
								AGCStream[0].avg23rd = gammaRevTable[avg23rds];
								AGCStream[0].avgcent = gammaRevTable[centeravg];
								avg23rds1 = AGCStream[1].avg;
								avg1 = AGCStream[1].avg23rd;
								algotarget = AGCStream[1].AgcTargetThreshold;
								algodir = AGCStream[1].dir;
								algoGmax = AGCStream[1].gainmax;
								algoGmin = AGCStream[1].gainmin; 
								algoSmax = AGCStream[1].shuttermax;
								algoSmin = AGCStream[1].shuttermin;
								algoResp = AGCStream[1].ResponseTime;
							}

						}
						else
						{
							AGCStream[0].avgfull = gammaRevTable[avg];
							AGCStream[0].avg23rd = gammaRevTable[avg23rds];
							AGCStream[0].avgcent = gammaRevTable[centeravg];
							avg23rds1 = AGCStream[0].avg23rd;
							avg1 = AGCStream[0].avg;
							algotarget = AGCStream[0].AgcTargetThreshold;
							algodir = AGCStream[0].dir;
							algoGmax = AGCStream[0].gainmax;
							algoGmin = AGCStream[0].gainmin; 
							algoSmax = AGCStream[0].shuttermax;
							algoSmin = AGCStream[0].shuttermin;
							algoResp = AGCStream[0].ResponseTime;
						}
					}

					encthrdvar.imagestatus[imageprocnumber] = 1;// filled
					imageprocnumber++;
					if(imageprocnumber > 2)
						imageprocnumber = 0;
					//frameSaved += 1;
				}	
				else
				{
					sv_camera_ReturnImage(camera, image);
//					usleep(2500);
				}
			}
		}
		else
		{
			sv_camera_ReturnImage(camera, image);
//			usleep(2500);
		}
		// change img cap parameters... ------------------
		if(prevgamma != imgGamma1)
		{
			prevgamma = imgGamma1;
			setGammaReverseCurveConstant(gammaValue[prevgamma], gammaRevTable);
		}
		if(prevgamma1 != imgGamma2)
		{
			prevgamma1 = imgGamma2;
			setGammaReverseCurveConstant(gammaValue1[prevgamma1], gammaRevTable1);
		}


		if((IcrControl) || (nighttime))
		{
			if(nighttimeExtTrig == 0)// 
			{
//				pri ntf("=====================0========Night time on ================= %d %d\n", ExtTrigEn, sharedConfigData->ip_details.port_num[0]);
				if(ExtTrigEn)// apply....
				{
					// change to ext triggr on sensor..
					syncarg.nightsh1 = ShutterTimeUS[shared_data->ShutterIndex2];
					syncarg.nightsh2 = ShutterTimeUS[shared_data->ShutterIndex1];
					syncarg.ExtTrigEn = 1;
/*					shutter = ShutterTimeUS[shared_data->ShutterIndex2];
					if(PrevAppliedShutter > 2500)
						PrevAppliedShutter = 2500;
					Pulse1WidthExtTrig(SensorI2CHandle, PrevAppliedShutter);
					shutter = ShutterTimeUS[shared_data->ShutterIndex1];
					if(shutter > 2500)
						shutter = 2500;
					Pulse2WidthExtTrig(SensorI2CHandle, shutter);
					usleep(100);
					if(checkExtTrigger(SensorI2CHandle, PrevAppliedShutter, shutter))
					{
						// failed..
//						pri ntf("=====================0========Night time fail ================= %d %d\n", ExtTrigEn, sharedConfigData->ip_details.port_num[0]);
						usleep(1000);
						shutter = ShutterTimeUS[shared_data->ShutterIndex2];
						if(PrevAppliedShutter > 2500)
							PrevAppliedShutter = 2500;
						Pulse1WidthExtTrig(SensorI2CHandle, PrevAppliedShutter);
						usleep(1000);
						shutter = ShutterTimeUS[shared_data->ShutterIndex1];
						if(shutter > 2500)
							shutter = 2500;
						Pulse2WidthExtTrig(SensorI2CHandle, shutter);
						usleep(1000);
					}
					SetExtTriggerSensorI2c(SensorI2CHandle);
					StartExtTrig(SensorI2CHandle);*/
				}
			}
			nighttimeExtTrig = 1;
			if(dual_capture_on)
			{
				AGCStream[0].init = 1;
				AGCStream[1].init = 1;
				if(dualshutterval == 0)
				{
					dualshutterval = 1;
					//SetShutterSensorI2c(SensorI2CHandle, vmax, sensorShutterdual2);
					if(sensorShutterdual1 != shared_data->ShutterIndex2)
						sensorShutterdual1 = shared_data->ShutterIndex2;
					if(sensorShutterdualval1 != ShutterTimeUS[sensorShutterdual1])
					{
			   			sensorShutterdualval1 = ShutterTimeUS[sensorShutterdual1];
						shutter = vmax - (int)(sensorShutterdualval1 / linetime);
						if(shutter <= 0)							
							shutter = 3;
						if(shutter > vmax)							
							shutter = vmax;
						value = (shutter & 0x1ffC) + 1;
						//SetShutterLineSensorI2c(SensorI2CHandle, value);
//						if(syncarg.Shutter2 != value)
//						{
							syncarg.Shutter2SetVal = value;
							syncarg.Shutter2Set = 1;
//						}
			   			PrevAppliedShutter = value;
					}
					else
					{
						//SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter);
//						if(syncarg.Shutter2 != PrevAppliedShutter)
//						{
							syncarg.Shutter2SetVal = PrevAppliedShutter;
							syncarg.Shutter2Set = 1;
//						}
					}
					if((IcrControl) || (nighttime))
					{
						//SetGainSensorI2c(SensorI2CHandle, sensorGain2, 0);
						sensorGain2 = shared_data->GainIndex2;
						PrevAppliedGain = IMX264AnalogGain1[sensorGain2];
//						if(syncarg.Gain2 != PrevAppliedGain)
//						{
							syncarg.Gain2SetVal = PrevAppliedGain;
							syncarg.Gain2Set = 1;
//						}
					}
					else
					{
						//SetGainSensorI2c(SensorI2CHandle, sensorGain1, 0);
						PrevAppliedGain = IMX264AnalogGain1[sensorGain1];
//						if(syncarg.Gain2 != PrevAppliedGain)
//						{
							syncarg.Gain2SetVal = PrevAppliedGain;
							syncarg.Gain2Set = 1;
//						}
					}
				}
				else
				{
					dualshutterval = 0;
					if(sensorShutterdual2 != shared_data->ShutterIndex1)
						sensorShutterdual2 = shared_data->ShutterIndex1;
					if(sensorShutterdualval2 != ShutterTimeUS[sensorShutterdual2])
					{
						sensorShutterdualval2 = ShutterTimeUS[sensorShutterdual2];
						shutter = vmax - (int)(sensorShutterdualval2 / linetime);
						if(shutter <= 0)							
							shutter = 2;
						value = shutter & 0x1ffC;
						//SetShutterLineSensorI2c(SensorI2CHandle, value);
//						if(syncarg.Shutter1 != value)
//						{
							syncarg.Shutter1SetVal = value;
							syncarg.Shutter1Set = 1;
//						}
			   			PrevAppliedShutter1 = value;
					}
					else
					{
						//SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter1);
//						if(syncarg.Shutter1 != PrevAppliedShutter)
//						{
							syncarg.Shutter1SetVal = PrevAppliedShutter1;
							syncarg.Shutter1Set = 1;
//						}
					}
					if(dual_capture_onflag == 0)
					{
						dual_capture_on = 0;
						syncarg.mode = 0;
					}
					if((IcrControl) || (nighttime))
					{
						sensorGain1 = shared_data->GainIndex1;
						//SetGainSensorI2c(SensorI2CHandle, sensorGain1, 0);
						//PrevAppliedGain1 = sensorGain1;
						PrevAppliedGain = IMX264AnalogGain1[sensorGain1];
//						if(syncarg.Gain1 != PrevAppliedGain)
//						{
							syncarg.Gain1SetVal = PrevAppliedGain;
							syncarg.Gain1Set = 1;
//						}
					}
					else
					{
						//SetGainSensorI2c(SensorI2CHandle, sensorGain2, 0);
						//PrevAppliedGain1 = sensorGain2;
						PrevAppliedGain = IMX264AnalogGain1[sensorGain2];
//						if(syncarg.Gain1 != PrevAppliedGain)
//						{
							syncarg.Gain1SetVal = PrevAppliedGain;
							syncarg.Gain1Set = 1;
//						}
			   		}
				}
			}
			else
			{
				if(sensorShutter != sensorShutter1)
				{
					sensorShutter = sensorShutter1;
					dualshutterval = 0;
					shutter = vmax - (int)( ShutterTimeUS[sensorShutter1] / linetime);
					if(shutter <= 0)							
						shutter = 2;
					value = shutter & 0xffC;
					SetShutterLineSensorI2c(SensorI2CHandle, value);
		   			PrevAppliedShutter = value;
				}
				if(dual_capture_onflag)
				{
					dual_capture_on = 1;
					syncarg.mode = 1;
					shutterset[0][0] = shutterset[1][0] = 0;
					shutterset[1][0] = shutterset[1][1] = 1;
					sensorShutterdualval1 = 0;
					sensorShutterdualval2 = 0;
					sensorShutterdualAGC1 = 0;
					sensorShutterdualAGC2 = 0;
				}
				if(sensorGain != sensorGain1)
				{
					sensorGain = sensorGain1;
					SetGainSensorI2c(SensorI2CHandle, sensorGain, 0);
					PrevAppliedGain = sensorGain;
				}
			}
		}
		else
		{
			if(nighttimeExtTrig == 1)// 
			{
//				prin tf("====================1=========Night time off ===================\n");
				if(ExtTrigEn)// apply....
				{
					syncarg.ExtTrigEn = 16;
					// change to Normal triggr on sensor..
					//SetNormalSensorI2c(SensorI2CHandle);
				}
				nighttimeExtTrig = 2;
			}
			else if(nighttimeExtTrig == 2)
			{
//				prin tf("====================2=========Night time off ===================\n");
				if(ExtTrigEn)// apply....
				{
					// change to Normal triggr on sensor..
					//StopExtTrig(SensorI2CHandle);
				}
				nighttimeExtTrig = 0;
			}
			if(dual_capture_on)
			{
				if(dualshutterval == 0)
				{
					dualshutterval = 1;
					AgcProcess(&AGCStream[1]);
					if(PrevAppliedGain != AGCStream[1].Gain)
					{
						value = AGCStream[1].Gain;
						//SetGainValSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedGain = value;
			   			sensorGain = value;
					}
					//else
					//{
						//SetGainValSensorI2c(SensorI2CHandle, PrevAppliedGain);
					//}
					if(sensorShutterdualAGC1 != AGCStream[1].shutter)
					{
						shutter = vmax - (int)(AGCStream[1].shutter / linetime);
						if(shutter <= 0)							
							shutter = 3;
						value = (shutter & 0xffC) + 1;
						//SetShutterLineSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedShutter = value;
			   			sensorShutterdualAGC1 = AGCStream[1].shutter;
						shutterset[1][1] = shutterset[0][1];
						shutterset[0][1] = value;
					}
					//else
					//{
						//SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter);
					//}
//					if(syncarg.Gain1 != PrevAppliedGain)
//					{
						syncarg.Gain1SetVal = PrevAppliedGain;
						syncarg.Gain1Set = 1;
//					}
//					if(syncarg.Shutter1 != PrevAppliedShutter)
//					{
						syncarg.Shutter1SetVal = PrevAppliedShutter;
						syncarg.Shutter1Set = 1;
//					}
				}
				else
				{
					dualshutterval = 0;
					AgcProcess(&AGCStream[0]);
					if(PrevAppliedGain1 != AGCStream[0].Gain)
					{
						value = AGCStream[0].Gain;
						//SetGainValSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedGain1 = value;
			   			sensorGain = value;
					}
					//else
					//	SetGainValSensorI2c(SensorI2CHandle, PrevAppliedGain1);
					if(sensorShutterdualAGC2 != AGCStream[0].shutter)
					{
						shutter = vmax - (int)(AGCStream[0].shutter / linetime);
						if(shutter <= 0)							
							shutter = 2;
						value = shutter & 0xffC;
						//SetShutterLineSensorI2c(SensorI2CHandle, value);
						sensorShutterdualAGC2 = AGCStream[0].shutter;
			   			PrevAppliedShutter1 = value;
			   			shutterset[1][0] = shutterset[0][0];
						shutterset[0][0] = value;//GetShutterSensorI2c(SensorI2CHandle);
					}
					//else
					//{
						//SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter1);
					//}
//					if(syncarg.Gain2 != PrevAppliedGain)
//					{
						syncarg.Gain2SetVal = PrevAppliedGain1;
						syncarg.Gain2Set = 1;
//					}
//					if(syncarg.Shutter2 != PrevAppliedShutter)
//					{
						syncarg.Shutter2SetVal = PrevAppliedShutter1;
						syncarg.Shutter2Set = 1;
//					}
					if(dual_capture_onflag == 0)
					{
						dual_capture_on = 0;
						syncarg.mode = 0;
					}
				}
			}
			else
			{
//				AGCStream[0].avgfull = avg;
//				AGCStream[0].avg23rd = avg23rds;
//				AGCStream[0].avgcent = centeravg;
				AgcProcess(&AGCStream[0]);
				if(PrevAppliedGain != AGCStream[0].Gain)
				{
					value = AGCStream[0].Gain;
					SetGainValSensorI2c(SensorI2CHandle, value);
		   			PrevAppliedGain = value;
		   			sensorGain = value;
				}
				if(sensorShutterAGC != AGCStream[0].shutter)
				{
					sensorShutterAGC = AGCStream[0].shutter;
					shutter = vmax - (int)(sensorShutterAGC / linetime);
					if(shutter <= 0)							
						shutter = 2;
					value = shutter & 0xffC;
					SetShutterLineSensorI2c(SensorI2CHandle, value);
		   			PrevAppliedShutter = value;
				}

				if(dual_capture_onflag)
				{
					dual_capture_on = 1;
					syncarg.mode = 1;
					sensorShutterdualAGC1 = 0;
					sensorShutterdualAGC2 = 0;
					sensorShutterdualval1 = 0;
					sensorShutterdualval2 = 0;
		   			shutterset[0][0] = shutterset[1][0] = 1;
		   			shutterset[0][1] = shutterset[1][1] = 2;
				}
			}
			usleep(1000);
		}

		if(lighttable != shared_data->crntLightTable)
		{
			lighttable = shared_data->crntLightTable;
			if(lighttable < 2)
				nighttime = 1;
			else
				nighttime = 0;
		}
		if(IcrControl != shared_data->IcrControl)
			IcrControl = shared_data->IcrControl;
		if(flash_on_off != shared_data->flashEnabled) 
			flash_on_off = shared_data->flashEnabled;
		if(dual_capture_onflag != shared_data->DualCaptureEnabled)
		{
			dual_capture_onflag = shared_data->DualCaptureEnabled;
			if(nighttime)
				if(dual_capture_onflag)
				{
					sensorShutterdual1 = sensorShutter2;
					sensorShutterdual2 = sensorShutter1;
				}
		}
		if(sensorShutter1 != shared_data->ShutterIndex1) 
		{
			sensorShutter1 = shared_data->ShutterIndex1;
			if(IcrControl)
				if(dual_capture_onflag)
				{
					sensorShutterdual2 = sensorShutter1;
				}
		}
		if(sensorShutter2 != shared_data->ShutterIndex2) 
		{
			sensorShutter2 = shared_data->ShutterIndex2;
			if(IcrControl)
				if(dual_capture_onflag)
				{
					sensorShutterdual1 = sensorShutter2;
				}
		}
		if(sensorGain1 != shared_data->GainIndex1)
			sensorGain1 = shared_data->GainIndex1;
		if(sensorGain2 != shared_data->GainIndex2)
			sensorGain2 = shared_data->GainIndex2;
		if(sensorGamma != shared_data->GammaIndex1)
			sensorGamma = shared_data->GammaIndex1;
		if(sensorGamma1 != shared_data->GammaIndex2)
			sensorGamma1 = shared_data->GammaIndex2;

		// AGC values
		if(AGCStream[0].AvgSel != shared_data->AvgSel1)
		{
			AGCStream[0].AvgSel 	= shared_data->AvgSel1;
		}
		if(AGCStream[1].AvgSel != shared_data->AvgSel2)
		{
			AGCStream[1].AvgSel 	= shared_data->AvgSel2;
		}
		AGCStream[0].ResponseTime 	= (1 << shared_data->ResponseTime1) << 1;
		AGCStream[1].ResponseTime 	= (1 << shared_data->ResponseTime2) << 1;
		AGCStream[0].gainmax 		= shared_data->gainmax1;
		AGCStream[1].gainmax 		= shared_data->gainmax2;
		AGCStream[0].shuttermax 	= shared_data->shuttermax1;
		AGCStream[0].shuttermin 	= shared_data->shuttermin1;
		AGCStream[1].shuttermax 	= shared_data->shuttermax2;
		AGCStream[1].shuttermin 	= shared_data->shuttermin2;
		
		AGCStream[0].AgcTargetThreshold = shared_data->AgcTargetHigh;
		AGCStream[0].AgcPmLowThreshold  = shared_data->AgcPLowThreshold1;
		AGCStream[0].AgcTargetLowThreshold = shared_data->AgcTargetLowThreshold1;
		AGCStream[0].AgcTargetHighThreshold = shared_data->AgcTargetHighThreshold1;
		AGCStream[0].AgcPmHighThreshold = shared_data->AgcPHighThreshold1;
		
		AGCStream[1].AgcTargetThreshold = shared_data->AgcTargetLow;
		AGCStream[1].AgcPmLowThreshold  = shared_data->AgcPLowThreshold2;
		AGCStream[1].AgcTargetLowThreshold = shared_data->AgcTargetLowThreshold2;
		AGCStream[1].AgcTargetHighThreshold = shared_data->AgcTargetHighThreshold2;
		AGCStream[1].AgcPmHighThreshold = shared_data->AgcPHighThreshold2;
	}
	buffer1.deallocateMemory();
	buffer2.deallocateMemory();
	buffer3.deallocateMemory();

	sv_camera_StopStream(camera);
	close_i2c(&SensorI2CHandle);


	return 0;
}


