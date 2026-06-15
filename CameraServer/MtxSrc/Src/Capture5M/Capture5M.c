/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

#include <linux/videodev2.h>

#include "../defines.h"
#include "../common_shm.h"
#include "../eeprom.h" 
#include "agc.h"

#include "NvJpegEncoder.h"
#include "NvBufSurface.h"
#include "NvUtils.h"


#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define THREE_MP	15
#define FIVE_MP		17
#define EIGHT_MP	20

extern void MTXcudaISPNVbuffer(
				unsigned char 	*src, 
				unsigned char 	*dsty, 
				unsigned char 	*dstu, 
				unsigned char 	*dstv, 
				short 		srcwidth, 
				short 		srcheight,
				float		gamma,
				float		wbgainR,
				float		wbgainG1,
				float		wbgainG2,
				float		wbgainB,
			   	short		roffset,
			   	short		g1offset,
			   	short		g2offset,
			   	short		boffset,
				int		color);

extern int DeadDelayMS(int ms);
extern int InitSensorI2c(int * pSensorI2CHandle);
extern int RegInitSensorI2c(int SensorI2CHandle);
extern int CloseSensorI2c(int SensorI2CHandle);
extern int SetShutterSensorI2c(int SensorI2CHandle, int vmax, int shutterindex);
extern int check_ShutterSensorI2c(int SensorI2CHandle, int vmax, int shutterindex);
extern int SetGainSensorI2c(int SensorI2CHandle, int gainindex, int aperture);
extern int GetShutterGainSensorI2c(int SensorI2CHandle);
extern int SetShutterGainSensorI2c(int SensorI2CHandle, int vmax, int shutterindex, int gainindex, int aperture);
extern int SetShutterLineSensorI2c(int i2c_dev_handle, int shutterLines);
extern int SetGainValSensorI2c(int i2c_dev_handle, int gainval);

extern void setGammaReverseCurveConstant(float gamma, unsigned char *gammaRevTable);
extern void get_brightness_image(unsigned char *data, int width, int height, int *avg, int *centeravg, int *avg23rds);
extern void AgcProcess(AGCCONTEXT *AGCContext);

//------------Maheen--------------------------------------------------------------------------
#define THREE_MP	15
#define FIVE_MP		17
#define EIGHT_MP	20
extern void ConfigureRTSPServerShdMem(unsigned char resolution, unsigned char *userdata, unsigned char *ipdetails);
extern void WriteJPEGFrameToRTSPServerShdMem(unsigned char *imgbuffer, unsigned long length);
extern void SetJPEGFPSToRTSPServerShdMem(unsigned char fps);
//------------Maheen--------------------------------------------------------------------------

struct buffer {
        void   *start;
        size_t  length;
};

static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

int showtime() {
    struct timeval tv;
    struct tm *tm_info;

    // Get the current time
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday failed");
        return EXIT_FAILURE;
    }

    // Convert seconds to local time
    tm_info = localtime(&tv.tv_sec);
    if (tm_info == NULL) {
        perror("localtime failed");
        return EXIT_FAILURE;
    }

    // Print formatted time with microseconds
    printf("Current time: %02d:%02d:%02d.%06ld\n",
           tm_info->tm_hour,
           tm_info->tm_min,
           tm_info->tm_sec,
           tv.tv_usec);

    return EXIT_SUCCESS;
}

static int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
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
	sync();
	return(sec);
}



int main(int argc, char **argv)
{
	// Captured image is 2688 x 2088 pixels..
	//  active frame is  2460 x 2048 pixels = 5038080 total pixels(5M)
	static const char       *dev_name = "/dev/video0";
	static int              fd = -1;
	struct buffer          *buffers;
	static unsigned int     n_buffers;
	static int              out_buf;
	static int              force_format;
	static int              frame_count = 250;
	static int              frame_number = 0;
	static short		Cap_width = 2688;
	static short		Cap_height = 2064;
	static short		Active_width = 2464;
	static short		Active_height = 2048;
	static float		gamma = 2.1;
	static float		wbr = 1.7;
	static float		wbgr = 1.0;
	static float		wbgb = 1.0;
	static float		wbb = 1.9;
	static short		roff = 0x60;
	static short		g1off = 0x35;
	static short		g2off = 0x35;
	static short		boff = 0x60;
	static int		color = 1;
	static int		gain = 1;// 0.1dB x Gain
        struct stat 		st;
        struct v4l2_capability 	cap;
        struct v4l2_cropcap 	cropcap;
        struct v4l2_crop 	crop;
        struct v4l2_format 	fmt;
        unsigned int 		min;
        unsigned long 		sizejpeg;
        struct v4l2_requestbuffers req;
        enum v4l2_buf_type 	type;
        unsigned int 		i, j, k;
	unsigned char 		*d_src;
	unsigned char 		*outimage;
	char 			filename[32];
	FILE 			*fp;	
	int			SensorI2CHandle, vmax, shutterindex, gainindex, aperture;
	NvJPEGEncoder *jpegenc;
	IMAGE_PARAMETERS	*imgparams[4];
	const unsigned char 	zeromemory[32] = {0};
	unsigned char 		imageparamsmem[4][128];
	int 			imagecompressed = 0;
	int 			image_process = 0, 
				shm_handle, 
				shm_handlecopc;
	void 			*ShMemory, 
				*ShMemorycopc;
	SHARED_RESOURCES 	*shared_data;
	SHARED_CONFIG_DATA 	*sharedConfigData;
	COMPRESS_PC_SHARED_RESOURCES *compressPcShr;
	struct disk_fat		*imageData;
	unsigned char 		CameraHeaderData[256],CameraFooterData[192], radar_data[2048];
	RADAR_ALL_VEH_BUFF 	*adjacentvehiclebuff, *adjacentvehiclemem;
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
	int 			frametime = 40010;//uS
	float 			linetime = 1.0;
	float			gammaValue[12] = {1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4, 2.45, 2.5, 2.55, 2.6, 2.65}, loadgamma;
	float			gammaValue1[12] = {1.6, 1.7, 1.85, 1.97, 2.1, 2.3, 2.4, 2.45, 2.5, 2.55, 2.6, 2.65};
	int			processhd, processlane, processevd, processvga;
	int			qualityStream;
	int			qualityStream1;
	int			qualityStream2;
	int			skipframes, skipframecount;
	struct timeval		t1;
	struct tm 		brokentime;
	time_t 			prev_time_in_sec;
	unsigned short 		deltatimeval = 0;
	int 			VsyncCount = 0, cap_seq_no, imginsec, img_exposure_type = 0, luxLUTVal;
	unsigned char 		triggerdataflag[32];
	static unsigned char 	gammaRevTable[256], gammaRevTable1[256];
	static int 		prevgamma = 5, prevgamma1 = 5;
	int 			sensorShutter = 0, sensorShutter1 = 0, sensorShutter2 = 0, sensorShutterdual1 = 0, sensorShutterdual2 = 0, sensorShutterdualval1 = 0, sensorShutterdualval2 = 0, sensorShutterAGC = 0,sensorShutterdualAGC1 = 0, sensorShutterdualAGC2 = 0,  
				flash_on_off = 0, IcrControl = 0, sensorGain = 0, sensorGain1 = 5, sensorGain2 = 5, shutter = 0, 
				dual_capture_on = 0, dual_capture_onflag = 0, sensorAperture = 0, sensorGamma = 0, sensorGamma1 = 0, imgGamma, imgGamma1 = 1, imgGamma2 = 1,nighttime = 0, lighttable = 0;
	int 			avg, centeravg, avg23rds, luxvals[512] = {0};
	int 			avg1, centeravg1, avg23rds1, algotarget, algodir, algotarget1, algodir1, algoGmax, algoResp, algoGmin, algoSmax, algoSmin;
	int 			PrevAppliedShutter = 0, PrevAppliedGain = 0, PrevAppliedShutter1 = 0, PrevAppliedGain1 = 0, crnt_dualcapimg = 0;
	AGCCONTEXT 		AGCStream[2];
	int 			dualshutterval = 0, value= 0;
	int 			shutterset[2][2] = {0};// [current0/prev1][firstval, secondval]
	int 			colorcapture[2] = {0};
	unsigned char 		crntyear = 0;
	time_t 			yrtime;
	unsigned int 		seqno;
	FILE			*fptr;
	
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
	jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
	imgparams[0] = (IMAGE_PARAMETERS *)imageparamsmem[0];
	imgparams[1] = (IMAGE_PARAMETERS *)imageparamsmem[1];
	imgparams[2] = (IMAGE_PARAMETERS *)imageparamsmem[2];
	imgparams[3] = (IMAGE_PARAMETERS *)imageparamsmem[3];
	imgparams[0]->header = 0x54629871;
	imgparams[0]->org_image_width = 2464;
	imgparams[0]->org_image_height = 2064;
	imgparams[0]->org_image_aspect_x = 5;
	imgparams[0]->org_image_aspect_y = 4;
	for(i = 1; i < 4; i++)
	{
		imgparams[i]->header = imgparams[i-1]->header;
		imgparams[i]->org_image_width = imgparams[i-1]->org_image_width;
		imgparams[i]->org_image_height = imgparams[i-1]->org_image_height;
		imgparams[i]->org_image_aspect_x = imgparams[i-1]->org_image_aspect_x;
		imgparams[i]->org_image_aspect_y = imgparams[i-1]->org_image_aspect_y;
	}
	imageData = (struct disk_fat*)CameraHeaderData;
	shared_data->CaptureStart = 0;
	shared_data->captureTrigger1 = 1;
	shared_data->ImgCapRequired = 1;
	while(shared_data->SyncStatus == 0)
	{
		usleep(10000);
	}
	sharedConfigData = (SHARED_CONFIG_DATA *)shared_data->configdata;// to get configuration..
	qualityStream = sharedConfigData->general_details.primary_stream_quality[0];
	switch(qualityStream)
	{
	case 0:
		qualityStream1 = 80;
		qualityStream2 = 80;
		break;
	case 1:
		qualityStream1 = 70;
		qualityStream2 = 70;
		break;
	case 2:
		qualityStream1 = 60;
		qualityStream2 = 60;
		break;
	default:
		qualityStream1 = 50;
		qualityStream2 = 50;
		break;
	}

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
	setGammaReverseCurveConstant(gammaValue[0], gammaRevTable1);
	setGammaReverseCurveConstant(gammaValue1[0], gammaRevTable);

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
	
	ConfigureRTSPServerShdMem(FIVE_MP, (unsigned char *)sharedConfigData->remote_user, (unsigned char *)&sharedConfigData->ip_details);	
	SetJPEGFPSToRTSPServerShdMem(skipframes);
	//------------Maheen--------------------------------------------------------------------------
	
	compressPcShr->PcViewList[1]=1;
	
	NvBuffer buffer8(V4L2_PIX_FMT_YUV420M, Cap_width, Cap_height, 0);// 5m, 5m/4, 5m/4
	buffer8.allocateMemory();
	printf("yuv buffer alloc length of buffer 0= %d, 1=%d, 2=%d\n", buffer8.planes[0].length, buffer8.planes[1].length, buffer8.planes[2].length);
	printf("yuv buffer alloc bytesused of buffer 0= %d, 1=%d, 2=%d\n", buffer8.planes[0].bytesused, buffer8.planes[1].bytesused, buffer8.planes[2].bytesused);
	printf("yuv buffer alloc no of planes = %d\n", buffer8.n_planes);
	buffer8.planes[0].bytesused = Cap_width * Cap_height;
	buffer8.planes[1].bytesused = (Cap_width * Cap_height) >> 2;
	buffer8.planes[2].bytesused = (Cap_width * Cap_height) >> 2;
	printf("yuv buffer alloc bytesused of buffer 0= %d, 1=%d, 2=%d\n", buffer8.planes[0].bytesused, buffer8.planes[1].bytesused, buffer8.planes[2].bytesused);

	InitSensorI2c(&SensorI2CHandle);
	
        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }


        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }
        
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf(stderr, "%s does not support streaming i/o\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }
        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }


        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
        fmt.fmt.pix.width       = 2650;
        fmt.fmt.pix.height      = 2088;
        
	// Captured image is 2688 x 2088 pixels..
	//  active frame is  2460 x 2048 pixels = 5038080 total pixels(5M)
	//				
        
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB12; 
        fmt.fmt.pix.field       = V4L2_FIELD_NONE;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                errno_exit("VIDIOC_S_FMT");

        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;

        CLEAR(req);

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = (struct buffer *)calloc(req.count, sizeof(*buffers));

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;
                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");
                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);
		printf("req buffer count = %d size = %u start %lu, offset %u\n", req.count , buf.length, (unsigned long int)buffers[n_buffers].start, buf.m.offset);
		memset(buffers[n_buffers].start, 0x55, 2048);
                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
        printf("req buffer count = %d\n", req.count );

        for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                errno_exit("VIDIOC_STREAMON");
               
	vmax = RegInitSensorI2c(SensorI2CHandle);
	shutterindex = 32;
	gainindex = 0;
	aperture = 0;
	SetShutterGainSensorI2c(SensorI2CHandle, vmax, shutterindex, gainindex, aperture);
	outimage = (unsigned char *)malloc( (Cap_width * Cap_width * 3 )/2);
	linetime = frametime/vmax;
        while(1)
        {
                fd_set fds;
                struct timeval tv;
                int r;

                FD_ZERO(&fds);
                FD_SET(fd, &fds);

                /* Timeout. */
                tv.tv_sec = 2;
                tv.tv_usec = 0;

                r = select(fd + 1, &fds, NULL, NULL, &tv);

                if (-1 == r) {
                        if (EINTR == errno)
                                continue;
                        errno_exit("select");
                }

                if (0 == r) {
                        fprintf(stderr, "select timeout\n");
                        exit(EXIT_FAILURE);
                }
		struct v4l2_buffer buf;
	        CLEAR(buf);

	        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	        buf.memory = V4L2_MEMORY_MMAP;

	        if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
	                switch (errno) {
	                case EAGAIN:
	                        return 0;

	                case EIO:
	                        /* Could ignore EIO, see spec. */

	                        /* fall through */

	                default:
	                        errno_exit("VIDIOC_DQBUF");
	                }
	        }

	        assert(buf.index < n_buffers);
	        frame_number++;
		skipframecount++;
		DeadDelayMS(5);
		if(skipframecount >= skipframes)
		{
			skipframecount = 0;
			processhd = 1;
			processvga = 1;
		}

		if(shared_data->captureTrigger1 == 0)
			shared_data->captureTrigger1 = 1;
		if(sensorGamma != shared_data->GammaIndex1)
			sensorGamma = shared_data->GammaIndex1;
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
		if(dual_capture_on)
		{
			if((IcrControl) || (nighttime))
			{
				if(crnt_dualcapimg == 1)// even...
				{
					img_exposure_type = 0;//high
					crnt_dualcapimg = 0;
				}
				else
				{
					crnt_dualcapimg = 1;
					img_exposure_type = 1;//low
				}
			}
			else
			{
				if(crnt_dualcapimg == 1)// even...
				{
					img_exposure_type = 1;//high
					crnt_dualcapimg = 0;
				}
				else
				{
					crnt_dualcapimg = 1;
					img_exposure_type = 0;//low
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
			
		img_exposure_type = (img_exposure_type)?0:1;
			

		gettimeofday(&t1, NULL);
		deltatimeval = ((t1.tv_sec & (unsigned int)0x1F)*1000)+(t1.tv_usec / 1000);// capture time in milliseconds maxvalue  32seconds.
		localtime_r(&t1.tv_sec, &brokentime);
		imginsec++;
		if(t1.tv_sec != prev_time_in_sec)
			imginsec = 0;
		prev_time_in_sec = t1.tv_sec;
		if(img_exposure_type == 0)
			cap_seq_no = get_Seqnumber(t1.tv_sec, &crntyear, &yrtime, &seqno);
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
		imageData->lux_n_table_no = (luxLUTVal << 8);// light table and lux info..
		imageData->pts=cap_seq_no;

		color = 1;
		gamma = gammaValue[sensorGamma];
		if((IcrControl) || (nighttime))
		{
			color = 0;
			gamma = gammaValue[sensorGamma];
		}
		if(dual_capture_on)
		{
			if(imageData->alarm_state)
			{
				imgGamma = sensorGamma1;
				gamma = gammaValue[imgGamma];
			}
			else
			{
				//imgGamma = (sensorGamma >> 4) & 0x07;
				imgGamma = sensorGamma;
				gamma = gammaValue1[imgGamma];
			}
		}
		wbr = (sharedConfigData->wb_details.wb_red_gain/(float)1000);			
		wbgr = (sharedConfigData->wb_details.wb_green1_gain/(float)1000);
		wbgb = (sharedConfigData->wb_details.wb_green2_gain/(float)1000);
		wbb = (sharedConfigData->wb_details.wb_blue_gain/(float)1000);
		roff = (sharedConfigData->wb_details.wb_red_offset > 255)? 255:(sharedConfigData->wb_details.wb_red_offset > 4)? sharedConfigData->wb_details.wb_red_offset: 4;
		g1off = (sharedConfigData->wb_details.wb_green1_offset > 255)? 255:(sharedConfigData->wb_details.wb_green1_offset > 4)? sharedConfigData->wb_details.wb_green1_offset: 4;
		g2off = (sharedConfigData->wb_details.wb_green2_offset > 255)? 255:(sharedConfigData->wb_details.wb_green2_offset > 4)? sharedConfigData->wb_details.wb_green2_offset: 4;
		boff = (sharedConfigData->wb_details.wb_blue_offset > 255)? 255:(sharedConfigData->wb_details.wb_blue_offset > 4)? sharedConfigData->wb_details.wb_blue_offset: 4;
		MTXcudaISPNVbuffer((unsigned char *)buffers[buf.index].start, buffer8.planes[0].data, buffer8.planes[1].data, buffer8.planes[2].data, Cap_width, Cap_height, gamma, wbr, wbgr, wbgb, wbb, roff, g1off, g2off, boff, color);
		get_brightness_image((unsigned char *)buffer8.planes[0].data, Cap_width, Cap_height, &avg, &centeravg, &avg23rds);


		sizejpeg = (Cap_width * Cap_height * 3 )/2;

		buffer8.planes[0].bytesused = Cap_width * Cap_height;
		buffer8.planes[1].bytesused = (Cap_width * Cap_height) >> 2;
		buffer8.planes[2].bytesused = (Cap_width * Cap_height) >> 2;

		if(compressPcShr->PcViewList[3] && processhd)// for ai and for ONVIF...
		{

			i = compressPcShr->PcImageWrNo[3];
			for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
			{
				compressPcShr->PcImageStatus[3][i][j] = IMGFRE;
			}
			sizejpeg = 3840 * 2160 * 1.5;
			jpegenc->setCropRect(0, 0, Active_width, Active_height);
			jpegenc->encodeFromBuffer(buffer8, JCS_YCbCr, &outimage, sizejpeg, qualityStream1);
			if(sizejpeg > MAXTXIMAGESIZE)
				sizejpeg = MAXTXIMAGESIZE;
			imagecompressed = sizejpeg;// size and flag..
			imageData->resolution = FIVE_MP;
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
			mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], radar_data, 2048);
			sizejpeg += 2048;
			mempcpy(&compressPcShr->PcImageOutEHMemory[i][sizejpeg], CameraFooterData, 192);
			mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
			compressPcShr->PcImageSize[3][i] = sizejpeg+192;
			//--------- populating img fat info..
			imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
			imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
			imageData->channel_info = 0x100;// evidence..
			imageData->longimg_size = sizejpeg+192;
			imageData->logical_channel = 3;
			mempcpy(compressPcShr->CameraImageHeaderData[3][i], CameraHeaderData, 128);
			
			
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
		if(compressPcShr->PcViewList[0] && processlane)// need to compress this image
		{
			i = compressPcShr->PcImageWrNo[0];
			for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
			{
				compressPcShr->PcImageStatus[0][i][j] = IMGFRE;
			}
			if(imagecompressed == 0)
			{
				sizejpeg = 3840 * 2160 * 1.5;
				jpegenc->setCropRect(0, 0, Active_width, Active_height);
				jpegenc->encodeFromBuffer(buffer8, JCS_YCbCr, &outimage, sizejpeg, qualityStream1);
				imagecompressed = sizejpeg;// size and flag..
				if(sizejpeg > MAXTXIMAGESIZE)
					sizejpeg = MAXTXIMAGESIZE;
			}
			else
			{
				sizejpeg = imagecompressed;
			}
			imageData->resolution = FIVE_MP;
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
			mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], radar_data, 2048);
			sizejpeg += 2048;
			for(j = 0; j < 8; j++)
			{
				if(CameraFooterData[j*24] != 0)
				{
					triggerdataflag[j*2] = CameraFooterData[(j*24) + 3];
					triggerdataflag[(j*2) + 1] = CameraFooterData[(j*24) + 4];
				}
				else
				{
					triggerdataflag[j*2] = 0;
					triggerdataflag[(j*2) + 1] = 0;
				}
			}
			mempcpy(&compressPcShr->PcImageOutHMemory[i][sizejpeg], CameraFooterData, 192);
			mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
			compressPcShr->PcImageSize[0][i] = sizejpeg+192;
			imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
			imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
			imageData->channel_info = 0x100;// evidence..
			imageData->longimg_size = sizejpeg+192;
			imageData->logical_channel = 0;
			mempcpy(compressPcShr->CameraImageHeaderData[0][i], CameraHeaderData, 128);
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
		if(compressPcShr->PcViewList[2] && processevd)// need to compress this image
		{
			i = compressPcShr->PcImageWrNo[2];
			for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
			{
				compressPcShr->PcImageStatus[2][i][j] = IMGFRE;
			}
			sizejpeg = 3840 * 2160 * 1.5;
			jpegenc->setCropRect(32, 16, 2400, 2016);
			jpegenc->setScaledEncodeParams(800, 672);
			jpegenc->encodeFromBuffer(buffer8, JCS_YCbCr, &outimage, sizejpeg, qualityStream2);
			if(sizejpeg > MAXTXIMAGESIZE)
				sizejpeg = MAXTXIMAGESIZE;
			imagecompressed = sizejpeg;// size and flag..
			imageData->resolution = FIVE_MP;
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
			mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], radar_data, 2048);
			sizejpeg += 2048;
			for(j = 0; j < 8; j++)
			{
				if(CameraFooterData[j*24] != 0)
				{
					triggerdataflag[j*2] = CameraFooterData[(j*24) + 3];
					triggerdataflag[(j*2) + 1] = CameraFooterData[(j*24) + 4];
				}
				else
				{
					triggerdataflag[j*2] = 0;
					triggerdataflag[(j*2) + 1] = 0;
				}
			}
			mempcpy(&compressPcShr->PcImageOutELMemory[i][sizejpeg], CameraFooterData, 192);
			mempcpy(imageData->last_alarm_name, triggerdataflag, 20);
			compressPcShr->PcImageSize[2][i] = sizejpeg+192;
			//--------- populating img fat info..
			imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
			imageData->image_type = 0;//MEDIA_JPEG;// 5 jpeg..
			imageData->channel_info = 0x100;// evidence..
			imageData->longimg_size = sizejpeg+192;
			imageData->logical_channel = 2;
			mempcpy(compressPcShr->CameraImageHeaderData[2][i], CameraHeaderData, 128);
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
		if(compressPcShr->PcViewList[1] && processvga)// need to compress this image
		{
			i = compressPcShr->PcImageWrNo[1];
			for(j = 0; j < NO_OF_SERVER_SOCKETS; j++)
			{
				compressPcShr->PcImageStatus[1][i][j] = IMGFRE;
			}

			if(imagecompressed == 0)
			{
				jpegenc->setCropRect(32, 16, 2400, 2016);
				jpegenc->setScaledEncodeParams(800, 672);
				jpegenc->encodeFromBuffer(buffer8, JCS_YCbCr, &outimage, sizejpeg, qualityStream2);
				if(sizejpeg > MAXTXIMAGESIZE)
					sizejpeg = MAXTXIMAGESIZE;
			}
			else
			{
				sizejpeg = imagecompressed;
			}
			imageData->resolution = FIVE_MP;
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
			if(imageData->alarm_state == 0)// only high images....
			{
				//------------Maheen--------------------------------------------------------------------------
				WriteJPEGFrameToRTSPServerShdMem(outimage, sizejpeg);	
				//------------Maheen--------------------------------------------------------------------------
			}
			//-----------------------------Maheen------------------------------------
			mempcpy(compressPcShr->PcImageOutLMemory[i], outimage, sizejpeg);
			mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], zeromemory, 32);
			sizejpeg += 31;
			sizejpeg &= 0x7FFE0;
			mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], imageparamsmem[1], 128);
			sizejpeg += 128;
			mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], radar_data, 2048);
			sizejpeg += 2048;
			mempcpy(&compressPcShr->PcImageOutLMemory[i][sizejpeg], CameraFooterData, 192);
			compressPcShr->PcImageSize[1][i] = sizejpeg+192;
			imageData->image_size = 0;//(sizejpeg+1)/2;// size of words..
			imageData->image_type = 1;//MEDIA_JPEG;// 5 jpeg..
			imageData->channel_info = 0x100;// evidence..
			imageData->longimg_size = sizejpeg+192;
			imageData->logical_channel = 1;
			memset(imageData->last_alarm_name, 0, 20);
			mempcpy(compressPcShr->CameraImageHeaderData[1][i], CameraHeaderData, 256);
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

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		{
			printf("VIDIOC_QBUF err break...\n");
			break;
		}
		/* EAGAIN - continue select loop. */
		if(frame_number > 12)
			frame_number = 0;

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
			if(dual_capture_on)
			{
				AGCStream[0].init = 1;
				AGCStream[1].init = 1;
				if(dualshutterval == 0)
				{
					dualshutterval = 1;
					//SetShutterSensorI2c(SensorI2CHandle, vmax, sensorShutterdual2);
					if(sensorShutterdualval1 != ShutterTimeUS[sensorShutterdual1])
					{
			   			sensorShutterdualval1 = ShutterTimeUS[sensorShutterdual1];
						shutter = vmax - (int)(sensorShutterdualval1 / linetime);
						if(shutter <= 0)							
							shutter = 3;
						if(shutter > vmax)							
							shutter = vmax;
						value = (shutter & 0x1ffC) + 1;
						SetShutterLineSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedShutter = value;
					}
					else
						SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter);
					if((IcrControl) || (nighttime))
					{
						SetGainSensorI2c(SensorI2CHandle, sensorGain2, 0);
						PrevAppliedGain = sensorGain2;
							
					}
					else
					{
						SetGainSensorI2c(SensorI2CHandle, sensorGain1, 0);
						PrevAppliedGain = sensorGain1;
					}
				}
				else
				{
					dualshutterval = 0;
					if(sensorShutterdualval2 != ShutterTimeUS[sensorShutterdual2])
					{
						sensorShutterdualval2 = ShutterTimeUS[sensorShutterdual2];
						shutter = vmax - (int)(sensorShutterdualval2 / linetime);
						if(shutter <= 0)							
							shutter = 2;
						value = shutter & 0x1ffC;
						SetShutterLineSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedShutter1 = value;
					}
					else
						SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter1);

					if(dual_capture_onflag == 0)
					{
						dual_capture_on = 0;
					}
					if((IcrControl) || (nighttime))
					{
						SetGainSensorI2c(SensorI2CHandle, sensorGain1, 0);
						PrevAppliedGain1 = sensorGain1;
					}
					else
					{
						SetGainSensorI2c(SensorI2CHandle, sensorGain2, 0);
						PrevAppliedGain1 = sensorGain2;
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
			if(dual_capture_on)
			{
				if(dualshutterval == 0)
				{
					dualshutterval = 1;
					AgcProcess(&AGCStream[1]);
					if(PrevAppliedGain != AGCStream[1].Gain)
					{
						value = AGCStream[1].Gain;
						SetGainValSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedGain = value;
			   			sensorGain = value;
					}
					else
						SetGainValSensorI2c(SensorI2CHandle, PrevAppliedGain);
					if(sensorShutterdualAGC1 != AGCStream[1].shutter)
					{
						shutter = vmax - (int)(AGCStream[1].shutter / linetime);
						if(shutter <= 0)							
							shutter = 3;
						value = (shutter & 0xffC) + 1;
						SetShutterLineSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedShutter = value;
			   			sensorShutterdualAGC1 = AGCStream[1].shutter;
						shutterset[1][1] = shutterset[0][1];
						shutterset[0][1] = value;
					}
					else
					{
						SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter);
					}
				}
				else
				{
					dualshutterval = 0;
					AgcProcess(&AGCStream[0]);
					if(PrevAppliedGain1 != AGCStream[0].Gain)
					{
						value = AGCStream[0].Gain;
						SetGainValSensorI2c(SensorI2CHandle, value);
			   			PrevAppliedGain1 = value;
			   			sensorGain = value;
					}
					else
						SetGainValSensorI2c(SensorI2CHandle, PrevAppliedGain1);
					if(sensorShutterdualAGC2 != AGCStream[0].shutter)
					{
						shutter = vmax - (int)(AGCStream[0].shutter / linetime);
						if(shutter <= 0)							
							shutter = 2;
						value = shutter & 0xffC;
						SetShutterLineSensorI2c(SensorI2CHandle, value);
						sensorShutterdualAGC2 = AGCStream[0].shutter;
			   			PrevAppliedShutter1 = value;
			   			shutterset[1][0] = shutterset[0][0];
						shutterset[0][0] = value;//GetShutterSensorI2c(SensorI2CHandle);
					}
					else
					{
						SetShutterLineSensorI2c(SensorI2CHandle, PrevAppliedShutter1);
					}
					if(dual_capture_onflag == 0)
					{
						dual_capture_on = 0;
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
					sensorShutterdualAGC1 = 0;
					sensorShutterdualAGC2 = 0;
					sensorShutterdualval1 = 0;
					sensorShutterdualval2 = 0;
		   			shutterset[0][0] = shutterset[1][0] = 1;
		   			shutterset[0][1] = shutterset[1][1] = 2;
				}
			}
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

/*--------------AGC values---------------------------------------------------------------------------*/
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
/*--------------AGC values---------------------------------------------------------------------------*/

        }
        
	free(outimage);
	buffer8.deallocateMemory();
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                errno_exit("VIDIOC_STREAMOFF");
        for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap(buffers[i].start, buffers[i].length))
                        errno_exit("munmap");
        free(buffers);
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
        fprintf(stderr, "\n");
	CloseSensorI2c(SensorI2CHandle);
         return 0;
}
