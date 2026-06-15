/********************************************************************************************/
/*		Project		:	IP Camera					    */
/*		Filename	:	eeprom.h					    */
/*		Functionality	:	EEPROM header file				    */
/*		Author		:	Maheen Rasheed					    */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.		    */
/********************************************************************************************/


/********************************************************************************************/
/*                          Macro	                                                    */
/********************************************************************************************/
#define MAX_REMOTE_USER		6
#define MAX_SCH_TIMING		1
#define MAX_SCH_CHANGES		1

#define GENERAL_DETAILS_OFFSET		0x0000
#define CAMERA_PARAMETERS_OFFSET	(GENERAL_DETAILS_OFFSET + ((sizeof(struct gnrl)) + 31) & 0xffe0)
#define REMOTE_USER_OFFSET		(CAMERA_PARAMETERS_OFFSET + (((sizeof(struct cam))*1)   + 31) & 0xffe0)
#define IP_DETAILS_OFFSET		(REMOTE_USER_OFFSET+ (((sizeof(struct remo))*MAX_REMOTE_USER) + 31) & 0xffe0)
#define SCHEDULE_OFFSET		(IP_DETAILS_OFFSET + ((sizeof (struct ip)) +31) & 0xffe0)
#define DYDNS_OFFSET			(SCHEDULE_OFFSET + (((sizeof (struct sch))*MAX_SCH_TIMING*MAX_SCH_CHANGES) + 31) & 0xffe0)
#define ALARM_OFFSET			(DYDNS_OFFSET + ((sizeof (struct dydns)) + 31) & 0xffe0)
#define RTSP_OFFSET			(ALARM_OFFSET + ((sizeof (struct alrm)) + 31) & 0xffe0)
#define WB_OFFSET			(RTSP_OFFSET + ((sizeof (struct rtsp_stream)) + 31) & 0xffe0)
#define WOMEN_SAFETY_CAM_OFFSET	(WB_OFFSET + ((sizeof(struct wb))+31) & 0xffe0)

#define CHECKSUM_OFFSET	 	(WOMEN_SAFETY_CAM_OFFSET +(((sizeof(struct womensafety_cam)*65)+31) & 0xffe0))


#define MR_GENERAL_DETAILS_OFFSET	0x1000
#define MR_CAMERA_PARAMETERS_OFFSET	(0x1000 + CAMERA_PARAMETERS_OFFSET)
#define MR_REMOTE_USER_OFFSET		(0x1000 + REMOTE_USER_OFFSET)
#define MR_IP_DETAILS_OFFSET		(0x1000 + IP_DETAILS_OFFSET)
#define MR_SCHEDULE_OFFSET		(0x1000 + SCHEDULE_OFFSET)
#define MR_DYDNS_OFFSET		(0x1000 + DYDNS_OFFSET)
#define MR_ALARM_OFFSET		(0x1000 + ALARM_OFFSET)
#define MR_RTSP_OFFSET			(0x1000 + RTSP_OFFSET)
#define MR_WB_OFFSET			(0x1000 + WB_OFFSET)
#define MR_WOMEN_SAFETY_CAM_OFFSET	(0x1000 + WOMEN_SAFETY_CAM_OFFSET)

#define NO_CHECKSUM_OFFSET		(0x1000 + CHECKSUM_OFFSET)

#define WRPR_OFFSET			0x0000	
#define WRPR_CHECKSUM_OFFSET		0x0040	

//#define WOMEN_SAFETY_CAM_OFFSET	0x12F0
//#define WOMEN_SAFETY_CHECKSUM_OFFSET	(WOMEN_SAFETY_CAM_OFFSET + (((sizeof (struct womensafety_cam))*65) +31) & 0xffe0)

//#define MR_WOMEN_SAFETY_CAM_OFFSET	0x1720
/********************************************************************************************/
/*                          Defines                                                         */
/********************************************************************************************/
#pragma pack(push)
#pragma pack(1)
struct wrpr
{
  char 			model_num[9];
  char			product_ver[9];
  char 			device_type;
  unsigned short 	serial_num;		
  unsigned char		date_of_mfg;		
  unsigned char		month_of_mfg;		
  unsigned short	year_of_mfg;		
  unsigned char		mac_address[6];		
  unsigned char 	sensor_model;
};
#pragma pack(0)


#pragma pack(1)
struct gnrl
{
  unsigned char	dome_type;			
  unsigned char	dome_baud;			    
  unsigned char primary_stream_enabled[4]; 
  unsigned char	primary_stream_resolution;	
  unsigned char primary_stream_type;
  unsigned char	primary_stream_fps[4];
  unsigned char	primary_stream_quality[4];   
  unsigned char	second_stream_enabled;		
  unsigned char	second_stream_resolution;	
  unsigned char second_stream_type;
  unsigned char	second_stream_fps[4];
  unsigned char	second_stream_quality[4];	  
  unsigned char third_stream_enabled;
  unsigned char third_stream_type;
  unsigned char third_stream_resolution;
  unsigned char	EnableAdaptiveAGC;
  unsigned char MaxHiAvgBrightness;
  unsigned char MaxLoAvgBrightness;
  unsigned char MinHiAvgBrightness;  
  unsigned char third_stream_quality[4];  
  unsigned char	relay_out_polarity;
  unsigned char	device_type;  
  char 		name[9];
  unsigned char agc_mode; 
  unsigned char vertical_flip;  
  unsigned char E2VTestMode;  
  unsigned char wb_mode;
  unsigned char gamma_value;  
  unsigned char ir_intensity;
  unsigned char antiflicker_mode;
  unsigned char af_enable_hour;
  unsigned char af_enable_minute;
  unsigned char af_disable_hour;
  unsigned char af_disable_minute;
  unsigned char ir_on_threshold;
  unsigned char ir_off_threshold;
  unsigned char agc_shutter_ceil;
  unsigned char MinLoAvgBrightness;
};
#pragma pack(0)

#pragma pack(1)
struct alrm
{
  unsigned char	input_polarity;
  unsigned char	input_type;
  unsigned char duration;
  unsigned char	enabled;
  unsigned char blank[16-4];
};
#pragma pack(0)

#pragma pack(1)
struct cam
{
  char 		name[9];
  unsigned char	brightness;
  unsigned char	contrast;
  unsigned char	saturation; 
  unsigned char	type;			//fixed / dome
  unsigned char	dome_address;		//fixed as 01,02,03,04	
  unsigned char	inactivity_delay;	
  unsigned char activity_data[64];
///----------------------------------------------------	audio
  unsigned char speakervolume;
  unsigned char micvolume;
///----------------------------------------------------	audio  
///---------------------------------------------------- anpr  
  unsigned char Lane_or_Evidence;
  unsigned char DayNight_ColourMode;
  unsigned char FocusAdjustDay;
  unsigned char FocusAdjustNight; 
  unsigned char FramesPerTrigger;   
///---------------------------------------------------- anpr
  
  unsigned char	blank[10];
};
#pragma pack(0)

#pragma pack(1)
struct wb
{
  unsigned int	wb_red_gain;
  unsigned int	wb_green1_gain;
  unsigned int	wb_green2_gain;
  unsigned int	wb_blue_gain;
  unsigned int	wb_red_offset;
  unsigned int	wb_green1_offset;
  unsigned int	wb_green2_offset;
  unsigned int	wb_blue_offset;  
};
#pragma pack(0)

#pragma pack(1)
struct ip
{
  unsigned char	ip_address[4];
  unsigned char	subnet_mask[4];
  unsigned char	gateway[4];
  unsigned char	port_num[4];
  unsigned char	ws_port_num[4];
  unsigned char	h264rtsp_portnum[4];
  unsigned char	jpgrtsp_portnum[4];
  unsigned char	blank[4];
};
#pragma pack(0)

#pragma pack(1)
struct remo
{
  char		name[9];
  unsigned char	password[6];
  unsigned char	level;
  unsigned char	blank[32-16];	
};
#pragma pack(0)

#pragma pack(1)
struct rtsp_stream
{
  unsigned char rtsp_portnum[5];
  unsigned char rtp_portnum[5];
  unsigned char rtsp_sock_mode;
  unsigned char media_type;  
  unsigned char authentication;
  unsigned char blank[16-13];
};
#pragma pack(0)

#pragma pack(1)
struct chsm
{
  unsigned short general_details_checksum;
  unsigned short camera_parameters_checksum;
  unsigned short remote_user_checksum;
  unsigned short ip_details_checksum;
  unsigned short schedule_checksum;
  unsigned short dydns_data_checksum;
  unsigned short alarm_data_checksum;
  unsigned short rtsp_stream_checksum;
  unsigned short wb_checksum;
  unsigned short womensafety_cam_checksumold;// for defaulting if firmware is reverting(backward compatibility)
	
	
  unsigned short mr_general_details_checksum;
  unsigned short mr_camera_parameters_checksum;
  unsigned short mr_remote_user_checksum;
  unsigned short mr_ip_details_checksum;
  unsigned short mr_schedule_checksum;
  unsigned short mr_dydns_data_checksum;
  unsigned short mr_alarm_data_checksum;
  unsigned short mr_rtsp_stream_checksum;
  unsigned short mr_wb_checksum;
  unsigned short mr_womensafety_cam_checksumold;
  unsigned short womensafety_cam_checksum;
  unsigned short mr_womensafety_cam_checksum;
  
  unsigned char blank[64-44];
};
#pragma pack(0)

#pragma pack(1)
struct nocs
{
  unsigned char	relay_out_onoff;
  unsigned char	dome_default[4];
  unsigned char second_framerate;	
  unsigned char second_bitrate;	
  unsigned char second_index;
  unsigned char	configured;	
  
  unsigned char general_details_default_loaded;	
  unsigned char	camera_parameters_default_loaded;
  unsigned char	remote_user_default_loaded;	
  unsigned char	ip_details_default_loaded;	
  unsigned char	schedule_default_loaded;	
  unsigned char	dydns_data_default_loaded;	
  unsigned char	alarm_data_default_loaded;
  unsigned char	rtsp_stream_default_loaded; 
  unsigned char	wb_default_loaded; 
///----------------------------------------------------	IR Control Zoo
  unsigned char	ir_cntl_zoo; 
///----------------------------------------------------	IR Control Zoo  
  
  unsigned char blank[32-19];
  
};
#pragma pack(0)

#pragma pack(1)
struct sch	
{
/*  unsigned char hour;
  unsigned char minute;
  unsigned char config;
  unsigned char record;
  unsigned char relay;*/
  unsigned char blank[16];
};
#pragma pack(0)

#pragma pack(1)
struct dydns
{
  unsigned short enabled;
  char 		serviceType[10];
  char 		username[20];
  char 		password[20];
  char 		hostname[40];
  unsigned char dns1[4];
  unsigned char dns2[4];
  unsigned char reserved[28];
};
#pragma pack(0)

struct eeprom_verify
{
  unsigned short length;
  unsigned short main_offset;
  unsigned short mirror_offset;
  unsigned short *main_checksum;
  unsigned short *mirror_checksum;
  unsigned char *start_address;
  unsigned char *new_start_address;
  const char	*display_string;
  void 		(*default_eeprom)(void);  
};

#pragma pack(1)
struct womensafety_cam
{
  unsigned char reserved[16];
};
#pragma pack(pop)

typedef struct SharedConfigData {
struct wrpr write_protected_data;
struct gnrl general_details;
struct cam camera_parameters[4];
struct ip ip_details;
struct remo remote_user[MAX_REMOTE_USER];
struct nocs no_checksum_data;
struct sch schedule[MAX_SCH_TIMING][MAX_SCH_CHANGES];
struct alrm alarm_data;
struct rtsp_stream rtsp_details;
struct wb wb_details;
struct dydns dydns_data;
struct womensafety_cam womensafety_camera_parameters[65];
}SHARED_CONFIG_DATA;


