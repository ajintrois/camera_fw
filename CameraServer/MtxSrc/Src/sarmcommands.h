/************************************************************************
Commands:
0x01 - S-E-VIO-CAP-RCMD,	Violation capture command
0x02 - S-E-ANP-CAP-RCMD,	ANPR capture command
0x03 - S-E-SIM-CAP-RCMD,	Simulation capture command
0x04 - S-E-SYNCH-RCMD,		RLVDS / XFPS trigger command
0x05 - S-E-ADJ-VEH-CMD-1,	Adjacent vehicle data command 1
0x06 - S-E-ADJ-VEH-CMD-2,	Adjacent vehicle data command 2
0x07 - S-E-ADJ-VEH-CMD-3,	Adjacent vehicle data command 3
0x08 - S-E-ADJ-VEH-CMD-4,	Adjacent vehicle data command 4
************************************************************************/
/************************************************************************
Basic type definitions
************************************************************************/
#pragma pack(1)
typedef struct CMD_N_DEST
{
	unsigned char command:3;
	//command type
	unsigned char destination:5;
	/*Bit oriented data. Bit set to 1 enables the recipient to process the data.*/
	/*B0 � E2V Evidence, B1�4 � Lane 0�3 front and rear E2Vs*/
}T_CMD_N_DEST;
typedef struct E2V_CMD_HEADER
{
	unsigned short headerStartMarker;
	/* headerStartMarker is 0xA235 */
	T_CMD_N_DEST cmdNDestination;
	/*see typedef*/
	unsigned char checkSum;
	/*Check-sum, sum of all other bytes in this command*/	
}T_E2V_CMD_HEADER;
typedef struct FLAGS
{
	unsigned char redSignal:1;
	//Red Signal status.
	//0: OFF, 1: ON
	unsigned char flagNData:7;
	/*other flags/data according to command type*/
}T_FLAGS;
typedef struct E2V_CAPTURE_CMD_FLAGS
{
	unsigned char redSignal:1;
	//Red Signal status.
	//0: OFF, 1: ON
	unsigned char xFpsCapture:1;
	//6 fps capture included or not
	//0: not included, 1: included
	unsigned char vehicleDirection:1;
	//Vehicle direction
	//0: approaching, 1: receding
	unsigned char vehicleType:3;
	//Vehicle Type
	//0: not included, 1: included
	unsigned char lane:2;
	//Lane number 0...3
}T_E2V_CAPTURE_CMD_FLAGS;
/************************************************************************
General command structure
************************************************************************/
typedef struct E2V_GENERAL_CMD
{
	T_E2V_CMD_HEADER header;
	/*see typedef*/
	T_FLAGS flags;
	/*see typedef*/
	unsigned char data[19];
	/*command data according to command type*/
}T_E2V_GENERAL_CMD;

/************************************************************************
Command structure for commands S-E-VIO-CAP-RCMD, S-E-ANP-CAP-RCMD and  S-E-SIM-CAP-RCMD
************************************************************************/
typedef struct E2V_CAPTURE_CMD
{
	T_E2V_CMD_HEADER header;
	/*see typedef*/
	T_E2V_CAPTURE_CMD_FLAGS flags;
	/*see typedef*/	
	unsigned short vehSpeed;
	/*speed of capturing vehicle*/
	unsigned short delay;
	/*delay in capturing vehicle*/
	unsigned int violationNumber;
	/*violation number for this capture*/	
	unsigned int xFpsSequenceNumber;
	/*if flags.xFpsCapture is not set, this is the last ANPR NVR capture-sequence number used*/
	/*if flags.xFpsCapture is set, ANPR NVR capture-sequence number for this capture*/
	unsigned char vehID;
	/*ID of the capturing vehicle received from the radar*/
	short xCoordinate;
	short yCoordinate;
	/*x & y coordinates of captured vehicle received from radar*/
	unsigned short reserved;
	/*reserved for debugging*/
}T_E2V_CAPTURE_CMD;

/************************************************************************
Command structure for command S-E-SYNCH-RCMD
************************************************************************/
typedef struct E2V_SYNCH_CMD
{
	T_E2V_CMD_HEADER header;
	/*see typedef*/
	T_FLAGS flags;
	/*see typedef*/
	unsigned char reserved1[4];
	/*not used*/	
	uint32_t violationNumber;
	/*last used violation number*/
	uint32_t xFpsSequenceNumber;
	/*ANPR NVR capture-sequence number for this capture*/
	unsigned char reserved2[7];
	/*not used*/
}T_E2V_SYNCH_CMD;

/************************************************************************
Command wrapper for authentication
************************************************************************/
typedef struct WRAP_FLAGS_SEQ
{
	unsigned short seqindex:10;	/* 10 bit sequence Index for auth data*/
	unsigned short newSeqdata:1; 	/* switch to new data block if avilable.*/
	unsigned short rebootfl:1;	/* reboot device, set the flag with toggle bit.*/
	unsigned short factorydef:1;	/* factory default device, set the flag with toggle bit.*/
	unsigned short encryption:1;	/* the payload data is encrypted if feature is availabe*/
	unsigned short sync:1;		/* the trigger device is present*/
	unsigned short toggle:1;	/* for registering the important flags*/
}T_WRAP_FLAGS_SEQ;

typedef struct WRAP_AUTH_DATA
{
	unsigned char auth0;
	unsigned char auth1;
	unsigned char auth2;
}T_WRAP_AUTH_DATA;

typedef struct E2V_WRAP_FOR_AUTH
{
	unsigned short header;// 0xC3A5
	/*see typedef*/
	T_WRAP_FLAGS_SEQ flags;
	/*see typedef*/
	T_WRAP_AUTH_DATA authdata;
	/*see typedef*/
	unsigned char csum;
	/*see typedef*/
	T_E2V_GENERAL_CMD payload;
	/*command data according to command type*/
}T_E2V_WRAP_FOR_AUTH;
#pragma pack()

