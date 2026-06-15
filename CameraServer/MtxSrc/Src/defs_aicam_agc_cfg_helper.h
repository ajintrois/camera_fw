#ifndef _DEFS_AICAM_AGC_CFG_HELPER_H_
#define _DEFS_AICAM_AGC_CFG_HELPER_H_

#include <stdint.h>

#ifndef SIZE_AICAM_AGC_CFG_HELPER_DATA
#define SIZE_AICAM_AGC_CFG_HELPER_DATA			1040
#endif

/*
Note For 64-Bit Compilation
uncomment the below line 
#define __ARCH_COMPILE_64	1
for 64-bit compatibility

//Modify the typedef for 32-bit and 64-bit suitably
*/

#define __ARCH_COMPILE_64	1

#ifdef __ARCH_COMPILE_64
	typedef unsigned int _ulong32_;
#else
	typedef unsigned long _ulong32_;
#endif

#ifndef ON_FLAG
#define ON_FLAG     1
#endif

#ifndef OFF_FLAG
#define OFF_FLAG    0
#endif

#ifndef AICAM_AGC_HDR_M1	
#define AICAM_AGC_HDR_M1	0x6556
#endif


#ifndef AICAM_AGC_HDR_M2	
#define AICAM_AGC_HDR_M2	0x7667
#endif

#pragma region ATEMS-AICAM-AGC-CFGDATA

#pragma pack(1)
typedef struct tag_AICAM_AGC_CFG_META_DATA_HEADER
{
	uint16_t	marker1;	//0x6556 : AICAM_AGC_HDR_M1
	uint16_t	marker2;	//0x7667 : AICAM_AGC_HDR_M2

	uint8_t	EnabledFlag;    // 0 -disable/off, 1 -enable/on

	uint8_t 	DualCapFlag;    // 0 -disable/off, 1 -enable/on
	uint8_t 	AgcTargetHigh;  // 0x80, AGC Target Value for First Cap
	uint8_t 	AgcTargetLow;   // 0x00, AGC low Value For Second Cap when DualCapFlag is ON

	uint8_t 	CapSerialProtocol;// 0= deafult advanced trigger, 1=normal serial trigger protocol
	uint8_t 	NightModeTrigflag;// 0= camera timed.. 1=ext trigger
	uint8_t 	reserved[6];
} AICAM_AGC_CFG_META_DATA_HEADER, *LPAICAM_AGC_CFG_META_DATA_HEADER;
#pragma pack()

#pragma pack(1)
typedef struct tag_AICAM_AGC_CFG_META_DATA_PACKED_VALUES
{
	uint8_t 	AvgSel;		// def = 1, Area in image, 0=Center, 1=center2/3rd, 2=full
	uint8_t 	ResponseTime; 		// def = 3, brightness running average in frames.[0<->7] values=(2,4,8,16,32,64,128,256) 
	uint8_t 	GammaIndex;		// [0<->7] defult=0, for dark image default=1
	uint8_t 	AgcPLowThreshold; 	// AgcTargetLowThreshold - 0x10
	uint8_t 	AgcTargetLowThreshold;  // AgcTarget - 0x18
	uint8_t 	AgcTargetHighThreshold; // AgcTarget + 0x18
	uint8_t 	AgcPHighThreshold; 	// AgcTargetHighThreshold + 0x10
	uint16_t 	shuttermax;	 	// in uSec, 1500
	uint16_t 	shuttermin;		// in uSec, 22
	uint16_t 	gainmax;	        // 320 == 20db
	uint16_t 	gainmin;	        // 0 == 1db

	uint8_t 	reserved[32];

} AICAM_AGC_CFG_META_DATA_PACKED_VALUES, *LPAICAM_AGC_CFG_META_DATA_PACKED_VALUES;
#pragma pack()

#pragma pack(1)
typedef struct tag_DUAL_CAP_BYTE_VALUE
{
	uint8_t 	First;
	uint8_t 	Second;
	
}DUAL_CAP_BYTE_VALUE, *LPDUAL_CAP_BYTE_VALUE;
#pragma pack()

#pragma pack(1)
typedef struct tag_AICAM_AGC_CFG_META_DATA_NIGHT_VALUES
{
	uint8_t 	NightModeLUXThresholdValue;	// 0 to 255 LUX at which changeover should affect for night switchover
	uint8_t 	DayModeLUXThresholdValue;	// 0 to 255 LUX at which changeover should affect for day switchover
	uint8_t 	NightStart;			//24 hours value. 0 to 23. Value at which night mode would start
	uint8_t 	NightEnd;			//24 hours value. 0 to 23. Value at which night mode would end. Probably next day

	uint8_t 	DualCaptureFlag;		// 0 - OFF, 1 - ON
	uint8_t 	Flash_enabled;			// 0 - OFF, 1 - ON

	DUAL_CAP_BYTE_VALUE Gamma;
	DUAL_CAP_BYTE_VALUE Gain;
	DUAL_CAP_BYTE_VALUE Shutter;

	uint8_t 	reserved[16];

} AICAM_AGC_CFG_META_DATA_NIGHT_VALUES, *LPAICAM_AGC_CFG_META_DATA_NIGHT_VALUES;
#pragma pack()

#pragma pack(1)
typedef struct tagAICAM_AGC_CFG_META_DATA
{    
	AICAM_AGC_CFG_META_DATA_HEADER          Header;
	AICAM_AGC_CFG_META_DATA_NIGHT_VALUES    Night_Settings;
	AICAM_AGC_CFG_META_DATA_PACKED_VALUES   AGC_Settings[2];

} AICAM_AGC_CFG_META_DATA, *LPAICAM_AGC_CFG_META_DATA;
#pragma pack()

#pragma pack(1)
typedef union tagU_AICAM_AGC_CFG_META_DATA
{
	AICAM_AGC_CFG_META_DATA 	data;
	unsigned char 			bytes[SIZE_AICAM_AGC_CFG_HELPER_DATA];
}U_AICAM_AGC_CFG_META_DATA, *LPU_AICAM_AGC_CFG_META_DATA;
#pragma pack()

#pragma endregion ATEMS-AICAM-AGC-CFGDATA

#endif /* _DEFS_AICAM_AGC_CFG_HELPER_H_ */
