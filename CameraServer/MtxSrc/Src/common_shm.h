#define SHM_NAME      "/IVTS56746MAIN"
#define SHM_NAME_CACO "/IVTS56746CACO"
#define SHM_NAME_COCO "/IVTS56746COCO"
#define SHM_NAME_COPC "/IVTS56746COPC"

typedef struct Shared_Resources{

	volatile unsigned int 	app_exit;		// 0=ok 1=exit;
	volatile unsigned int	validID;		// 0x46392715;
	volatile unsigned int 	MainProcStatus;	// 0=idle 1=active 2=exit;
	volatile unsigned int 	CaptureStatus;		// 0=idle 1=active 2=exit;

	volatile unsigned int 	ConvertProcStatus;	// 0=idle 1=active 2=exit;
	volatile unsigned int 	CompressStatus;	// 0=idle 1=active 2=exit;
	volatile unsigned int 	PcStreamStatus;	// 0=idle 1=active 2=exit;
	volatile unsigned int	SyncStatus;		// 0=wait for sync 1=synced after init.
	volatile unsigned int 	RtcTimeUpdateSync;	// 0=no update 1=Rtc updated. Now send update time to all
	
	volatile unsigned char	setrtctimecode[6];	// just for ref.. do not use this.. instead get time from system
	volatile unsigned char	time_error;
	volatile unsigned char	settime_flag;
	volatile unsigned char	rtctimecode[6];
	volatile unsigned char	applyCurrentLightSettings;
	volatile unsigned char	SystemReset;
	//volatile unsigned char	SystemShutdown;


	volatile unsigned int	CaptureWidth;
	volatile unsigned int	CaptureHeight;
	volatile unsigned int	CaptureVoffset;
	volatile unsigned int	Stream2Voffset;

	volatile unsigned int	ImgCapRequired;
	volatile unsigned int	captureTrigger1;// used as keep alive
	volatile unsigned int	flashEnabled;
	volatile unsigned int	DualCaptureEnabled;
	volatile unsigned int	IcrControl;

	volatile unsigned int	lux;
	volatile unsigned int	crntLightTable;
	volatile unsigned int	vmax;
	volatile unsigned int	serialcapdatatime;
	volatile unsigned int	FactoryDefaults;
	volatile unsigned int	IPDefaults;

	volatile unsigned int	ShutterIndex1;
	volatile unsigned int	ShutterIndex2;
	volatile unsigned int	GainIndex1;
	volatile unsigned int	GainIndex2;
	volatile unsigned int	GammaIndex1;
	volatile unsigned int	GammaIndex2;

	volatile unsigned int	AgcTargetHigh;
	volatile unsigned int	AgcTargetLow;
	volatile unsigned int	AvgSel1;
	volatile unsigned int	AvgSel2;
	volatile unsigned int	ResponseTime1;
	volatile unsigned int	ResponseTime2;
	volatile unsigned int	AgcPLowThreshold1;
	volatile unsigned int	AgcPLowThreshold2;
	volatile unsigned int	AgcTargetLowThreshold1;
	volatile unsigned int	AgcTargetLowThreshold2;
	volatile unsigned int	AgcTargetHighThreshold1;
	volatile unsigned int	AgcTargetHighThreshold2;
	volatile unsigned int	AgcPHighThreshold1;
	volatile unsigned int	AgcPHighThreshold2;
	volatile unsigned int	shuttermax1;
	volatile unsigned int	shuttermax2;
	volatile unsigned int	shuttermin1;
	volatile unsigned int	shuttermin2;
	volatile unsigned int	gainmax1;
	volatile unsigned int	gainmax2;
	volatile unsigned int	gainmin1;
	volatile unsigned int	gainmin2;
	
	volatile unsigned int	CaptureStart;
	volatile unsigned int	focusDayAdj;
	volatile unsigned int	focusNightAdj;
	volatile unsigned int	Serialport4fd;
	volatile unsigned int	Capturecmdcnt;
	volatile unsigned int	xfpscmdcnt;

	volatile unsigned int	PcConnectionStatus[NO_OF_SERVER_SOCKETS];		// connection status.
	
//-----------------------------Maheen-----------------------
	volatile unsigned char	RTCCmd;				//0-None 1-Read 2-Write 3-Read Complete 4-Write Complete 5- Read Fail 6-Write Fail		
//-----------------------------Maheen-----------------------	


	volatile unsigned char	configdata[32*1024];
	
	volatile unsigned int	FwUpgrade;
	volatile unsigned int   SensorReset;
	volatile unsigned int   SensorResetwidth;
	volatile unsigned int   SensorErrorCnt;
	volatile unsigned int   SensorTemprature;
	volatile unsigned int   CameraType;		// lane/evidence 0-3lane, 4-7evidence
	volatile unsigned int   CameraNumber;	// lane/evidence 0-3lane, 4-7evidence
	volatile unsigned int   CaptureMode;	// 0= fps 1=trigger
	volatile unsigned int	capture_serial_violationNo[8];	
	volatile unsigned int	capture_serial_xfpSeqNo[8];	
	volatile unsigned int	capture_serial_trigger_timesec[8];			//seccount from pc start..
	volatile unsigned int	capture_serial_trigger_timemilsec[8];		// milliseccount from pc start..
	volatile unsigned char	capture_serial_trigger[8];					// 0=no trigger, 1=violation 2=xfps
	volatile unsigned char	capture_serial_trigger_lane_ack[8];			// 0=no trigger, 1=violation first image, 2 = second
	volatile unsigned char	capture_serial_trigger_evidence_ack[8];		// 0=no trigger, 1=violation first image, 2 = second 
	unsigned char		capture_serial_data[8][24];
	volatile unsigned char	radar_all_veh_data[2048];
	volatile unsigned char	Zoom_init;// 0= no zoom, 1 zoom on socket0
	volatile unsigned char	Zoom_left;// 0 = no action, 1=go left
	volatile unsigned char	Zoom_right;
	volatile unsigned char	Zoom_up;
	volatile unsigned char	Zoom_down;
	volatile unsigned char	newcapturecmdauthdata;
	volatile unsigned char	capturecmdauthdata0[1024];
	volatile unsigned char	capturecmdauthdata1[1024];
	volatile unsigned char	capturecmdauthdata2[1024];
	volatile unsigned char	resdata[24*1024];

} SHARED_RESOURCES;// max 1024kByte

typedef struct Capture_Convert_Shared_Resources{
  volatile unsigned int		validID0;// 0x54367356;
  volatile unsigned int		CaptureImageStatus[MAX_RAW_IMAGE_CAPTURE_BUFFER];	// Raw image filled status..
  volatile unsigned int		CaptureImagetype[MAX_RAW_IMAGE_CAPTURE_BUFFER][4];	// Raw image type..// type 0=trigger+lane, 1=vga, 2=trigger+evidence, 3=evidence, 
  volatile unsigned int		CaptureImageRdNo;					// Raw image read ptr.
  volatile unsigned int		CaptureImageWrNo;					// Raw image write ptr.

  volatile unsigned char	capture_serial_trigger[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];				// 0=no trigger, 1=violation
  volatile unsigned char	capture_serial_trigger_lane_ack[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];			// 0=no trigger, 1=violation first image, 2 = second
  volatile unsigned char	capture_serial_trigger_evidence_ack[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];		// 0=no trigger, 1=violation first image, 2 = second 
  unsigned char				capture_serial_data_buff[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8][24];

  unsigned char				CameraImageHeaderData[MAX_RAW_IMAGE_CAPTURE_BUFFER][256];
  unsigned char				CameraImageFooterData[MAX_RAW_IMAGE_CAPTURE_BUFFER][256];

//  unsigned char 			captureMemory[MAX_RAW_IMAGE_CAPTURE_BUFFER][SENSOR_IMG_WIDTH*SENSOR_IMG_HEIGHT*2];// raw image..
				
  unsigned int				indexOfCaptureMemory[MAX_RAW_IMAGE_CAPTURE_BUFFER];

} CAPTURE_CONVERT_SHARED_RESOURCES;// 

typedef struct Convert_Compress_Shared_Resources{
  volatile unsigned int		validID1;// 0x86240637;
  volatile unsigned int		ConvertImageStatus[MAX_RAW_IMAGE_CONVERT_BUFFER];	// Raw image filled status..
  volatile unsigned int		ConvertImagetype[MAX_RAW_IMAGE_CAPTURE_BUFFER][4];	// Raw image type..// type 0=trigger+lane, 1=vga, 2=trigger+evidence, 3=evidence, 
  volatile unsigned int		convertImageRdNo;						// Raw image read ptr.
  volatile unsigned int		convertImageWrNo;	

  volatile unsigned char	capture_serial_trigger[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];				// 0=no trigger, 1=violation 2=xfps
  volatile unsigned char	capture_serial_trigger_lane_ack[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];			// 0=no trigger, 1=violation first image, 2 = second
  volatile unsigned char	capture_serial_trigger_evidence_ack[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8];		// 0=no trigger, 1=violation first image, 2 = second 
  unsigned char				capture_serial_data_buff[MAX_RAW_IMAGE_CAPTURE_BUFFER][4][8][24];

  unsigned char				convertImageOutmemory[MAX_RAW_IMAGE_CONVERT_BUFFER][SENSOR_IMG_WIDTH*(SENSOR_IMG_HEIGHT*3)/2];
  unsigned char				CameraImageHeaderData[MAX_RAW_IMAGE_CONVERT_BUFFER][256];
  unsigned char				CameraImageFooterData[MAX_RAW_IMAGE_CAPTURE_BUFFER][256];
} CONVERT_COMPRESS_SHARED_RESOURCES;// 

typedef struct Compress_Pc_Shared_Resources{
  volatile unsigned int		validID2;// 0x79312648;
  volatile unsigned int		PcImageStatus[NO_OF_PC_STREAMS][MAX_RAW_IMAGE_PC_BUFFER][NO_OF_SERVER_SOCKETS];// jpeg image filled status.
  volatile unsigned int		PcImageInit[NO_OF_PC_STREAMS][NO_OF_SERVER_SOCKETS];			// jpeg image read ptr.
  volatile unsigned int		PcImageRdNo[NO_OF_PC_STREAMS][NO_OF_SERVER_SOCKETS];			// jpeg image read ptr.
  volatile unsigned int		PcImageWrNo[NO_OF_PC_STREAMS];						// jpeg image write ptr.
  volatile unsigned char		PcImageWrLog[NO_OF_PC_STREAMS][MAX_RAW_IMAGE_PC_BUFFER][256];					// jpeg image write logs.
  volatile unsigned int		PcImageSize[NO_OF_PC_STREAMS][MAX_RAW_IMAGE_PC_BUFFER];// jpeg image filled status.
  volatile unsigned int		PcViewList[NO_OF_PC_STREAMS];
  volatile unsigned int		PcImageStreamReq[NO_OF_PC_STREAMS][NO_OF_SERVER_SOCKETS];
  unsigned char				PcImageOutHMemory[MAX_RAW_IMAGE_PC_BUFFER][MAXHIRESJPEGIMGSIZE];
  unsigned char				PcImageOutELMemory[MAX_RAW_IMAGE_PC_BUFFER][MAXLORESJPEGIMGSIZE];
  unsigned char				PcImageOutLMemory[MAX_RAW_IMAGE_PC_BUFFER][MAXLORESJPEGIMGSIZE];
  unsigned char				PcImageOutEHMemory[MAX_RAW_IMAGE_PC_BUFFER][MAXHIRESJPEGIMGSIZE];
  unsigned char				CameraImageHeaderData[NO_OF_PC_STREAMS][MAX_RAW_IMAGE_PC_BUFFER][256];
} COMPRESS_PC_SHARED_RESOURCES;// 


#define MAKESIZE4MASK(size)	(unsigned long)((size < 1024)?1024:\
				((size < 2048)?2048:\
				((size < 4096)?4096:\
				((size < (1024*8))?(1024*8):\
				((size < (1024*16))?(1024*16):\
				((size < (1024*32))?(1024*32):\
				((size < (1024*64))?(1024*64):\
				((size < (1024*128))?(1024*128):\
				((size < (1024*256))?(1024*256):\
				((size < (1024*512))?(1024*512):\
				((size < (1024*1024))?(1024*1024):\
				((size < (1024*1024*2))?(1024*1024*2):\
				((size < (1024*1024*4))?(1024*1024*4):\
				((size < (1024*1024*8))?(1024*1024*8):\
				((size < (1024*1024*16))?(1024*1024*16):\
				((size < (1024*1024*32))?(1024*1024*32):\
				((size < (1024*1024*64))?(1024*1024*64):\
				((size < (1024*1024*128))?(1024*1024*128):\
				((size < (1024*1024*256))?(1024*1024*256):(1024*1024*512))))))))))))))))))))


#define SHM_SIZE		(sizeof(SHARED_RESOURCES))
#define SHM_SIZE_CACO		(sizeof(CAPTURE_CONVERT_SHARED_RESOURCES))
#define SHM_SIZE_COCO		(sizeof(CONVERT_COMPRESS_SHARED_RESOURCES))
#define SHM_SIZE_COPC		(sizeof(COMPRESS_PC_SHARED_RESOURCES))


