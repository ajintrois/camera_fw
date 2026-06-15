#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "agc.h"

void setGammaReverseCurveConstant(float gamma, unsigned char *gammaRevTable)
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

void get_brightness_image(unsigned char *data, int width, int height, int *avg, int *centeravg, int *avg23rds)
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

