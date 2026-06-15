/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/
#define MAX_REMOTE_USER	4

#define THREE_MP	15
#define FIVE_MP		17
#define EIGHT_MP	20

#define HIGH		1
#define MEDIUM		2
#define LOW		3

#define RTSP_UNICAST		0
#define RTSP_MULTICAST		1

#define ADMIN 			0x00
#define USER 			0x01
#define GUEST			0x02

#define TRUE	1
#define FALSE	0

/*
 * File Operation Utilities
 */

#define DEFINE_FILE				FILE *testfptr;

#define LOGTOFILE(filename,buff,size)		testfptr=fopen(filename,"w");	\
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	

#define APPENDTOFILE(filename,buff,size)	testfptr=fopen(filename,"a");	\
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	

#pragma pack(1)
typedef struct rtsp_details
{
	//User Account Settings  
	char 		remote_user_name[MAX_REMOTE_USER][9];
	unsigned char	remote_user_password[MAX_REMOTE_USER][6];
	unsigned char	remote_user_level[MAX_REMOTE_USER];  
	unsigned char	blank2[16];

	//RTSP Settings
	unsigned char 	rtsp_portnum[5];
	unsigned char 	rtp_audio_portnum[5];
	unsigned char 	rtcp_audio_portnum[5];
	unsigned char 	rtp_video_portnum[5];
	unsigned char 	rtcp_video_portnum[5];
	unsigned char	multicast_ip_address[4];
	unsigned char 	rtsp_mode;
	unsigned char 	rtsp_authentication;
	unsigned char 	rtsp_stream_name[9];
	
	unsigned char	fps;
	unsigned char	resolution;
	unsigned char	onvif_portnum[5];
	unsigned char	model_name[16];
	unsigned char	hardware_ID[16];
	unsigned char	serial_no[16];
	unsigned char	firmware_version[16];
	unsigned char	manufacturer[16];
		
}RTSPVAR;

typedef struct camera_remo
{
  char		name[9];
  unsigned char	password[6];
  unsigned char	level;
  unsigned char	blank[32-16];	
}T_CAMERA_REMO;

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
typedef struct ShmRTSPServer
{
	unsigned char start;	
	unsigned char prev_start;
	unsigned char buff_cnt;		
	unsigned char counter_rtsp;	
	unsigned char total_cnt;
	struct timeval presentation_time[5];
	unsigned char filled[5];
	unsigned char type[5];
	int counter[5];
	unsigned long size[5];
	unsigned char payload[5][4*1024*1024];
	
	RTSPVAR rtsp_config_details;
}SHM_RTSP_SERVER;
#pragma pack(0)


#pragma pack(1)
typedef struct ShmX264
{
	unsigned char filled[2];
	int buff_cnt;
	unsigned long size[2];
	int jpgframecnt[2];
	unsigned char payload[2][4*1024*1024];
	
	unsigned char yuv_filled[2];
	int yuv_buff_cnt;
	unsigned long yuv_size[2];
	int yuvframecnt[2];	
	unsigned char yuv_payload[2][13*1024*1024];
	
}SHM_X264;
#pragma pack(0)


