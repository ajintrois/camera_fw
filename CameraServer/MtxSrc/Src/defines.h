#include "circfifo.h"

// ---------------------- defines / macros --------------------
#define PROCESSSTATUS_IDLE		0
#define PROCESSSTATUS_INIT		1
#define PROCESSSTATUS_WAIT		2
#define PROCESSSTATUS_RUN		3
#define PROCESSSTATUS_CONFIG		4
#define PROCESSSTATUS_STOP		5

#define IMGFRE				0
#define IMGRDY				1
#define IMGUSE				2
#define IMGFIN				3

#define SOCK_DISCONNECTED		0
#define SOCK_READY			2
#define SOCK_CONNECTED			1
#define SOCK_CLOSE			3
#define SOCK_LISTEN			4

#define NO_OF_CLIENT_SOCKETS		4
#define NO_OF_SERVER_SOCKETS		4
#define NO_OF_DEBUG_SERVER_SOCKETS	4

#define SEQUENCE_NO			3
#define PACKET_LENGTH			4

#define SOCKET_SEND_BUFF_LENGTH		(1024 * 8)

#define NO_OF_PROCESS			2
#define POLL_TIMEOUT_MSEC		8/* min 20 ms*/

#define MAX_RAW_IMAGE_CAPTURE_BUFFER	8
#define MAX_RAW_IMAGE_CONVERT_BUFFER	8
#define MAX_RAW_IMAGE_PC_BUFFER		16

#define NO_OF_PC_STREAMS		4	/*0=lane, 1=vga, 2=evidence*/

#define SENSOR_IMG_WIDTH		4224
#define SENSOR_IMG_HEIGHT		3008

#define CAPTURE_IMG_WIDTH		3840
#define CAPTURE_IMG_HEIGHT		2160
#define CAPTURE_IMG_VOFFSET		0

#define MAX_PROCESS_LOOP_TIME		3000/* mSec timout for all process to check flag and loop*/

#define MAX_LANE_NO			4
#define MAX_IMG_DATA_COUNT		4
#define KEEPALIVE_TIMEOUT_COUNT	16
#define MAX_WDT_COUNT			16

#define MAXHIRESJPEGIMGSIZE		1024*1024*2	
#define MAXLORESJPEGIMGSIZE		1024*1024	

#define MAXTXIMAGESIZE			((1024*1024) - (1024 * 128))

#define PIPE_RD_END			0
#define PIPE_WR_END			1

#define MEDIA_H264		0x85
#define MEDIA_JPEG		0x80
#define MEDIA_MPEG4		0x84

#define RES640X368		11
#define RES640X480		12
#define ONE_MP			13

#define THREE_MP		15
#define FIVE_MP			17
#define EIGHT_MP		20
#define TWELVE_MP		24

#define HIGH			0x01
#define MEDIUM			0x02
#define LOW			0x03

#define	SWITCHOFF		0
#define SWITCHON		1

#define ADMIN 			0x00
#define USER 			0x01
#define GUEST			0x02

#define WB_AUTO_MODE	0
#define WB_MANUAL_MODE	1

#define OTDR_MODE	0
#define INDR_MODE	1

#define N_O			0x00
#define N_C			0x01
#define MOMENTARY		0x00
#define LATCHED			0x01
#define ACTIVE_LOW		0x00
#define ACTIVE_HIGH		0x01
#define OFF 			0x00
#define ON 			0x01

#define SHUTTER_CEIL_1MS	5
#define SHUTTER_CEIL_25MS	4
#define SHUTTER_CEIL_5MS	3
#define SHUTTER_CEIL_10MS	2
#define SHUTTER_CEIL_20MS	1
#define SHUTTER_CEIL_40MS	0

#define RTSP_UNICAST		0
#define RTSP_MULTICAST		1

///----------------------------------------------------	Motion Detection
#define MOTION_DETECT_ENABLED	1
#define MOTION_DETECT_DISABLED	0

#define NOREC 	 		0x00
#define ACTIVITY 		0x01
#define NORMAL	 		0x02

#define MOTION_DETECTION_TIMEOUT	20
///----------------------------------------------------	Motion Detection

///---------------------------------------------------- anpr
#define LANE_CAMERA1		0
#define LANE_CAMERA2		1
#define LANE_CAMERA3		2
#define LANE_CAMERA4		3
#define EVIDENCE_CAMERA1	4
#define EVIDENCE_CAMERA2	5
#define EVIDENCE_CAMERA3	6
#define EVIDENCE_CAMERA4	7

#define NORMAL_DN_MODE		0
#define ALWAYS_COLOUR_MODE	1
#define ALWAYS_MONOCHROME_MODE	2
///---------------------------------------------------- anpr

//#define
// ---------------------- structure defines --------------------
struct tst {
  unsigned int		sock_number;			// connection number for tracing..
  unsigned int		connection_status;		// READY=2/CONNECTED=1/DISCONNECTED=0
  char			ip[32];				// ip no
  char			port_no[8];			// port no
  unsigned int		local_host_keep_alive;		// local keep alive
  unsigned int		client_keep_alive_tx_count;	// remote keep alive tx delay.
  unsigned int		client_keep_alive_tx_status;	// remote keep alive tx delay.
  unsigned int		remote_keep_alive_count;	// remote keep alive status
  unsigned int		remote_keep_alive_count_reload;	// remote keep alive reload value
  unsigned int		send_communication_init;	// start comm command 0x8A..
  int			socket_handle;			// handle...
  struct cirfifo	*remote_tx_fifo;		//
  struct cirfifo	*remote_rx_fifo;		//
  unsigned short	*rx_data_buffer;
  unsigned short	*tx_data_buffer;
  struct wdt		*wdt_ptr;
  void			*shmem;
  void			*shmemcopc;
  unsigned int		streaming_flag;
  int			level;
  int			debugpipe;
  int			streamSelect;
  int			streams;
  int			imgstxed;
  int			sentlogtxt;	//0=no log.. 0x10=syslog, 0x20=kernlog, 0x30=authlog, 0x40=ufwlog, 0x50=stunnellog,.. 
  					//0x11=syslogbak, 0x21=kernlogbak, 0x31=authlogbak,.. etc  
  					//last 900k of file will only be sent..
  unsigned int		clientAccess;
  unsigned short	keyseed;
  unsigned int		keyoffset;
  unsigned char		keydata[128];
  unsigned int		SecondsConnected;
};

struct wdt {
  unsigned int	enabled;
  unsigned int  res1;
  unsigned int  res2;
  unsigned int  res3;
  unsigned int *keep_alive_timer;
  unsigned int *keep_alive_reload;
  unsigned int *aux_keep_alive_timer;
  unsigned int *err;
};

#pragma pack(1)
typedef struct radarvehicledata {
  uint16_t	header; // 0x9531
  uint16_t	packetrunningcnt; 
  uint8_t	rhour;// recieved time..
  uint8_t	rmin;
  uint8_t	rsec;
  uint8_t	ryear;
  uint8_t	rmon;
  uint8_t	rdate;
  uint16_t	rmSecs;
  uint8_t  	packetdata[24];
}RADAR_ALL_VEH_DATA;

#define MAX_RADAR_ALL_VEH_DATA	56

typedef struct radarvehiclebuff {
  uint32_t	header; // 0x40f19243
  uint8_t	ihour;// image capture time.. ~+-5mS.
  uint8_t	imin;
  uint8_t	isec;
  uint8_t	iyear;
  uint8_t	imon;
  uint8_t	idate;
  uint16_t	imSecs;
  uint8_t  	no_ofdata;// no of packets rxed in this buff
  uint8_t  	lastdatapos;// where last data is entered
  uint16_t	lastpacketrunningcnt;// running count of last packet
  uint8_t 	res[32-16];
  RADAR_ALL_VEH_DATA allvehicledata[MAX_RADAR_ALL_VEH_DATA];
}RADAR_ALL_VEH_BUFF;
// do not read all data.. when data starts looping always ignore first data..
// so max avialable vehicle data count is MAX_RADAR_ALL_VEH_DATA-1

struct disk_fat
{
	unsigned short  magic;	// 0xabba					//  0
	unsigned char   ifile_offset[8];	// offset to image in disk 	//  2
	unsigned short  image_size; // image size in dwords			// 10
	unsigned int   rfile_index; // index					// 12
	unsigned short  dvrmodel; // model number				// 16
	unsigned int   record_cam_list;	// Sep 24, 2008				// 18
	unsigned short  channel_info; // frame type and channel no		// 22
	unsigned short  record_speed; // record speed				// 24
	unsigned short  resolution; // CIF, field, D1				// 26
	unsigned short  qlevel;	// quality level				// 28
	unsigned short  year;							// 30
	unsigned short  month;							// 32
	unsigned short  date;							// 34
	unsigned short  hour;							// 36
	unsigned short  minute;							// 38
	unsigned short  second;							// 40
	unsigned short  dummy; // dummy image flag - if reqd			// 42
	unsigned short  alarm_state; // state of 16 alarms			// 44
	unsigned short  codec_no;						// 46
	unsigned short  logical_channel;					// 48
	unsigned short  img_seq_no;						// 50
	unsigned short  frame_type;						// 52
	unsigned char 	ifat_offset[8];	// offset to fat in disk		// 54
	unsigned char   camera_name[20]; // drtdvr025				// 62
	unsigned short  clip_index;						// 82
	unsigned char   last_alarm_name[20];					// 84
	unsigned short  ntsc_pal; // ntsc or pal				//104
	unsigned short  image_type; // mpeg4 or jpg				//106
	unsigned short  channel_seq_no; // mpeg4 or jpg				//108
	unsigned int	longimg_size;						//110
	unsigned int	pts;							//114
	unsigned char	anprnvr_ifat_offset[8];					//118
	unsigned short 	lux_n_table_no;						//126
};

typedef struct image_parameters {//packed 32 bytes...
uint32_t	header;// 0x54629871;
uint16_t	org_image_width;
uint16_t	org_image_height;
uint8_t		org_image_aspect_x;// 4
uint8_t		org_image_aspect_y;// 3
uint8_t		image_resize_type; //0=original, 1= crop, 2=resize 3= resize and crop
uint8_t		reserved01;
uint8_t		image_resize_xfact;//10=1.0x or 25=2.5x, etc
uint8_t		image_resize_yfact;//10=1.0x or 25=2.5x, etc
uint16_t	image_xoffset; //xoffset from original image start in pixels
uint16_t	image_yoffset; //yoffset from original image start in pixels
uint16_t	image_width;
uint16_t	image_height;
uint8_t		image_aspect_x;// 16
uint8_t		image_aspect_y;// 9
uint16_t	reserved02;
uint32_t	image_compressed_size;
uint32_t	reserved03;
} IMAGE_PARAMETERS;
#pragma pack()



