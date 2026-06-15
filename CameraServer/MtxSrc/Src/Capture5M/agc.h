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

