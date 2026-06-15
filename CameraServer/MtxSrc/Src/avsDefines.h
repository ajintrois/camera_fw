#pragma pack(push)
#pragma pack(1)
struct wrpr_avs
{
  char 			model_num[21];
  char			product_ver[9];
  char 			device_type;
  unsigned short 	serial_num;		// default value 0x0001
  unsigned char		date_of_mfg;		//
  unsigned char		month_of_mfg;		//
  unsigned short	year_of_mfg;		//
  unsigned char		mac_address[6];		//
  unsigned char		blank[64-43];
};
#pragma pack(0)

#pragma pack(1)
struct gnrl_avs
{
  unsigned char	video_system;			// 00
  unsigned char	dome_type;			// 01
  unsigned char	dome_baud;			// 02
  unsigned char	primary_stream_resolution;	// 03
  unsigned char	primary_stream_type;		// 04
  unsigned char	primary_stream_tl_mode[4];	// 05-08
  unsigned char	primary_stream_quality[4];	// 09-0c
  unsigned char	primary_stream_encode[4];	// 0d-10
  unsigned char	second_stream_enabled;		// 11
  unsigned char	second_stream_resolution;	// 12
  unsigned char	second_stream_type;		// 13
  unsigned char	second_stream_tl_mode[4];	// 14-17
  unsigned char	second_stream_quality[4];	// 18-1b
  unsigned char	second_stream_encode[4];	// 1c-1f
  unsigned char	relay_out_polarity;		// 20
  unsigned char	blank1;				// 21
  unsigned char hdd_installed;			// 22
  unsigned char	device_type;			// 23
  char		name[21];			// 24-38	
  //unsigned char sensor_type;
  unsigned char agc_mode;
  unsigned char	blank[64-58];			// 39-3f
};
#pragma pack(0)

#pragma pack(1)
struct alrm_avs
{
  unsigned char	input_polarity;		
  unsigned char	input_type;		// NOT USED HERE	
  unsigned char duration;		// NOT USED HERE	
  unsigned char	enabled;		// NOT USED HERE
  unsigned char	dome_valid[4];		// NOT USED HERE
  unsigned char dome_default[4];	// NOT USED HERE
  unsigned char blank[32-12];	
};
#pragma pack(0)

#pragma pack(1)
struct cam_avs
{
  char 		name[21];
  unsigned char	brightness;
  unsigned char	contrast;
  unsigned char	saturation;
  unsigned char	hue;
  unsigned char	type;			//fixed / dome
  unsigned char	dome_address;		// fixed as 01,02,03,04			//x
  unsigned char	inactivity_delay;	
  unsigned char	activity_data[64];	//x
  unsigned char	blank[4];
};
#pragma pack(0)

#pragma pack(1)
struct ip_avs
{
  unsigned char	ip_address[4];
  unsigned char	subnet_mask[4];
  unsigned char	gateway[4];
  unsigned char	port_num[4];
  unsigned char	ws_port_num[4];
  unsigned char	blank[12];
};
#pragma pack(0)

#pragma pack(1)
struct remo_avs
{
  char		name[9];	//
  unsigned char	password[6];	//
  unsigned char	level;		//
  unsigned char	covert_list[4];	// yy
  char		nick_name[13];	// yy
  
  unsigned int	wb_data_1;
  unsigned int	wb_data_2;
  
  unsigned char	blank[64-41];	//
};
#pragma pack(0)

#pragma pack(1)
struct dydns_avs
{
  unsigned short enabled;
  unsigned char serviceType[10];
  unsigned char username[20];
  unsigned char password[20];
  unsigned char hostname[40];
  unsigned char dns1[4];
  unsigned char dns2[4];
  unsigned char reserved[28];
};
#pragma pack(0)


#pragma pack(1)
struct chsm_avs
{
  unsigned short	general_details_checksum;
  unsigned short	camera_parameters_checksum;
  unsigned short	remote_user_checksum;
  unsigned short	ip_details_checksum;
  unsigned short	schedule_checksum;
  unsigned short	dydns_data_checksum;
  unsigned short 	alarm_data_checksum;

  unsigned short	mr_general_details_checksum;
  unsigned short	mr_camera_parameters_checksum;
  unsigned short	mr_remote_user_checksum;
  unsigned short	mr_ip_details_checksum;
  unsigned short	mr_schedule_checksum;
  unsigned short	mr_dydns_data_checksum;
  unsigned short  	mr_alarm_data_checksum;

  unsigned char 	blank[64-28];
};
#pragma pack(0)

#pragma pack(1)
struct nocs_avs
{
  unsigned char	relay_out_onoff;
  unsigned char audio_out_onoff;	// NOT USED HERE
  unsigned char	dome_default[4];	
  unsigned char second_framerate;	
  unsigned char second_bitrate;		
  unsigned char second_index;		
  unsigned char	user_prompted_record;	// NOT USED HERE
  unsigned char dvr_restart_count;	
  unsigned char	scheduled_recording;	// NOT USED HERE
  unsigned char	display_format;		// 00:quad, 01:seq
  unsigned char	seq_dwell;		// range: 3-20
  unsigned char	codec_restart_count;	
  unsigned char	configured;				// new in nvr avs
  unsigned char general_details_default_loaded;		// new in nvr avs
  unsigned char	camera_parameters_default_loaded;	// new in nvr avs
  unsigned char	remote_user_default_loaded;		// new in nvr avs
  unsigned char	ip_details_default_loaded;		// new in nvr avs
  unsigned char	schedule_default_loaded;		// new in nvr avs
  unsigned char	dydns_data_default_loaded;		// new in nvr avs
  unsigned char	alarm_data_default_loaded;		// new in nvr avs

  unsigned char	blank[32-23];
};
#pragma pack(0)

#pragma pack(1)
struct sch_avs					//  NOT USED HERE
{
  unsigned char hour;
  unsigned char minute;
  unsigned char config;
  unsigned char record;
  unsigned char relay;
  unsigned char blank[3];
};
#pragma pack(0)

#pragma pack(1)
struct ip_device_status_to_nvr
{
  unsigned char new_data;
  unsigned char live_video_status[4];
  unsigned char live_alarm_input[4];
  unsigned char relay_out_status[4];
  unsigned char blank[64-13];
};
#pragma pack(0)	

#pragma pack(1)
struct ip_device_status_from_nvr
{
  unsigned char new_data;

  unsigned char encode[4];
  unsigned char tl_mode[4];
  unsigned char quality[4];
  unsigned char encode_mask[4];

  unsigned char second_stream;	//on/off
  unsigned char encode_change;

  unsigned char relay_out_onoff[4];
  unsigned char relay_out_mask[4];

  unsigned char dome_default[4];
  unsigned char dome_default_delay[4];
  unsigned char dome_mask[4];

  unsigned char alarm_dome_position[4];
  unsigned char alarm_dome_delay[4];
  unsigned char alarm_dome_mask[4];

  unsigned char blank[64-51];

};
#pragma pack(pop)

