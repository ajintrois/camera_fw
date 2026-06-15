/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/********************************************************************************************/
/*		Project	   	:	Jetson ORIN Camera/NVR			       */
/*		Author/Modified By 	:	Maheen Rasheed				       */
/*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
/********************************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "NvUtils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "jpeg_decode.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define PERF_LOOP   300

using namespace std;

#ifndef APP_VERSION
#define APP_VERSION	"2.1.3"
#endif

static uint64_t
get_file_size(ifstream * stream)
{
    uint64_t size = 0;
    streampos current_pos = stream->tellg();
    stream->seekg(0, stream->end);
    size = stream->tellg();
    stream->seekg(current_pos, stream->beg);
    return size;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->perf = false;
    ctx->use_fd = false;
    ctx->stress_test = 0;
    ctx->current_file = 0;
}



#include <signal.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h> 
#include <linux/watchdog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

//#include "nvosd.h"

/*
 * File Operation Utilities
 */
/*
#define DEFINE_FILE				FILE *testfptr;

#define LOGTOFILE(filename,buff,size)		testfptr=fopen(filename,"wb"); \
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	

#define APPENDTOFILE(filename,buff,size)	testfptr=fopen(filename,"a");	\
						fwrite(buff,1,size,testfptr);	\
						fflush(testfptr);            	\
						fsync(fileno(testfptr));	\
						fclose(testfptr);	
*/						
#include "../CameraServer/MtxSrc/Src/MediaServer/MtxStreamer/RTSPDefines.h"  

DEFINE_FILE						
						
/*#pragma pack(1)
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

*/

SHM_X264 *X264ShmImgPtr;

/**
 * Class NvJPEGDecoder decodes JPEG image to YUV.
 * NvJPEGDecoder::decodeToBuffer() decodes to software buffer memory
 * which can be access by CPU directly.
 * NvJPEGDecoder::decodeToFd() decodes to hardware buffer memory which is faster
 * than NvJPEGDecoder::decodeToBuffer() since the latter involves conversion
 * from hardware buffer memory to software buffer memory.
 *
 * When using NvJPEGDecoder::decodeToFd(), NvUtils is used to
 * convert NvJPEGDecoder output YUV hardware buffer memory (DMA buffer fd) to
 * MMAP buffer so CPU can access it to write it to file.
 */
 
 int capcount=500;
 int buff_counter=0;

#include <opencv2/opencv.hpp>
#include "nvbufsurface.h"

enum TimestampPos {
    TOP_LEFT, TOP_CENTER, TOP_RIGHT,
    CENTER_LEFT, CENTER_MIDDLE, CENTER_RIGHT,
    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT
};

enum TextBackgroundMode {
    TEXT_BORDER, OPAQUE_MODE, SEMI_TRANSPARENT_MODE, TRANSPARENT_MODE
};

int addOverlaytextToNvBuffer(NvBuffer *buffer, uint32_t width, uint32_t height, const char* text, TimestampPos pos, TextBackgroundMode bkMode) {
    if (!buffer || !text ) return -1;

    buffer->map();
    NvBuffer::NvBufferPlane &yPlane = buffer->planes[0];
    if (yPlane.data == nullptr) {
        buffer->unmap();
        return -1;
    }

    cv::Mat yMat(height, width, CV_8UC1, yPlane.data, yPlane.fmt.stride);

    /*
	// 1. Prepare Timestamp String
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);
	*/

    // 2. Configuration for the Font
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    // Base scaling for 720p
    double baseScale = 0.8;
    int baseThickness = 2;

    // Calculate scale relative to 720p height
    double fontScale = (height / 720.0) * baseScale;
    int thickness = std::max(1, (int)((height / 720.0) * baseThickness));

    // Margin should also be relative
    // Margin from the edges
    int margin = (int)(height * 0.03); // 3% of height

    int baseline = 0;

    // 3. Calculate Text Size to determine offsets
    cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
    
    cv::Point textOrg;

    // 4. Determine Position
    switch (pos) {
        case TOP_LEFT:
            textOrg = cv::Point(margin, textSize.height + margin);
            break;
        case TOP_CENTER:
            textOrg = cv::Point((width - textSize.width) / 2, textSize.height + margin);
            break;
        case TOP_RIGHT:
            textOrg = cv::Point(width - textSize.width - margin, textSize.height + margin);
            break;
        case CENTER_LEFT:
            textOrg = cv::Point(margin, (height + textSize.height) / 2);
            break;
        case CENTER_MIDDLE:
            textOrg = cv::Point((width - textSize.width) / 2, (height + textSize.height) / 2);
            break;
        case CENTER_RIGHT:
            textOrg = cv::Point(width - textSize.width - margin, (height + textSize.height) / 2);
            break;
        case BOTTOM_LEFT:
            textOrg = cv::Point(margin, height - margin);
            break;
        case BOTTOM_CENTER:
            textOrg = cv::Point((width - textSize.width) / 2, height - margin);
            break;
        case BOTTOM_RIGHT:
            textOrg = cv::Point(width - textSize.width - margin, height - margin);
            break;
    }

    // 5. Draw the text (Scalar 255 = White)
    // cv::putText(yMat, text, textOrg, fontFace, fontScale, cv::Scalar(255), thickness);

	if(bkMode == TEXT_BORDER){
		// 1. Draw the "Outline" or "Shadow"
		// Use Black (Scalar 0 in YUV) and a thicker line
		// Thickness + 2 or 3 creates a nice border
		cv::putText(yMat, text, textOrg, fontFace, fontScale, 
					cv::Scalar(0), thickness + 3, cv::LINE_AA);

		// 2. Draw the actual text on top
		// Use White (Scalar 255 in YUV) and standard thickness
		cv::putText(yMat, text, textOrg, fontFace, fontScale, 
					cv::Scalar(255), thickness, cv::LINE_AA);
	}
	else if(bkMode == SEMI_TRANSPARENT_MODE){
		// Draw a semi-transparent-looking black box
		// 2. Define the Box Area
		int padding = 10;
		cv::Rect roi(textOrg.x - padding, 
					textOrg.y - textSize.height - padding, 
					textSize.width + (padding * 2), 
					textSize.height + (padding * 2));

		// Boundary check to prevent segfaults at image edges
		roi &= cv::Rect(0, 0, width, height);

		// 3. APPLY SEMI-TRANSPARENCY (The "Dimming" effect)
		// We iterate over the Y-plane data in the ROI and darken the pixels
		for (int y = roi.y; y < roi.y + roi.height; y++) {
			uint8_t* row_ptr = yPlane.data + (y * yPlane.fmt.stride);
			for (int x = roi.x; x < roi.x + roi.width; x++) {
				// Right-shift by 1 is the same as dividing by 2 (50% transparency)
				// It makes the background darker but keeps the image visible
				row_ptr[x] >>= 1; 
			}
		}

		// 4. Draw the White Text over the dimmed box
		cv::Mat yMat(height, width, CV_8UC1, yPlane.data, yPlane.fmt.stride);
		cv::putText(yMat, text, textOrg, cv::FONT_HERSHEY_SIMPLEX, 
					fontScale, cv::Scalar(255), thickness, cv::LINE_AA);
	}
	else if(bkMode == OPAQUE_MODE){
		// Define the rectangle area based on text size + margin
		cv::Rect backgroundBox(textOrg.x - 5, textOrg.y - textSize.height - 5, 
							textSize.width + 10, textSize.height + 15);
		
		// Draw solid black rectangle
		cv::rectangle(yMat, backgroundBox, cv::Scalar(0), cv::FILLED);

		// Draw white text over it
		cv::putText(yMat, text, textOrg, fontFace, fontScale, cv::Scalar(255), thickness);
	}
	else if(bkMode == TRANSPARENT_MODE){
    	// Draw the text (Scalar 255 = White)
    	cv::putText(yMat, text, textOrg, fontFace, fontScale, cv::Scalar(255), thickness);
	}
	else{ 
		//Default: Transparent Mode
		cv::putText(yMat, text, textOrg, fontFace, fontScale, cv::Scalar(255), thickness);
	}

    buffer->unmap();
    return 0;
}


static int jpeg_decode_proc(context_t& ctx, int len, char *jpgbuffer)
{
    int ret = 0;
    //int error = 0;
   // int fd = 0;
    uint32_t width, height, pixfmt;
    //int i = 0;
    //int iterator_num = 1;
    //int dst_dma_fd = -1;
    //int out_pixfmt = 2;

    /*set_defaults(&ctx);
    
    ret = parse_csv_args(&ctx, argc, argv);
    TEST_ERROR(ret < 0, "Error parsing commandline arguments", cleanup);

    for(i = 0; i < ctx.num_files; i++)
    {
      ctx.in_file[i] = new ifstream(ctx.in_file_path[i]);
      TEST_ERROR(!ctx.in_file[i]->is_open(), "Could not open input file", cleanup);

      ctx.out_file[i] = new ofstream(ctx.out_file_path[i]);
      TEST_ERROR(!ctx.out_file[i]->is_open(), "Could not open output file", cleanup);
    }

    ctx.jpegdec = NvJPEGDecoder::createJPEGDecoder("jpegdec");
    TEST_ERROR(!ctx.jpegdec, "Could not create Jpeg Decoder", cleanup);

    if (ctx.perf)
    {
      iterator_num = PERF_LOOP;
      ctx.jpegdec->enableProfiling();
    }*/

#if 0
    for(i = 0; i < ctx.num_files; i++)
    {
      ctx.in_file_size = get_file_size(ctx.in_file[i]);
      ctx.in_buffer = new unsigned char[ctx.in_file_size];
      ctx.in_file[i]->read((char *) ctx.in_buffer, ctx.in_file_size);

      /**
       * Case 1:
       * Decode to software buffer memory by decodeToBuffer() and write to
       * local file.
       */
      if (!ctx.use_fd)
      {
        NvBuffer *buffer;

        for (int i = 0; i < iterator_num; ++i)
        {
          ret = ctx.jpegdec->decodeToBuffer(&buffer, ctx.in_buffer,
                ctx.in_file_size, &pixfmt, &width, &height);
          TEST_ERROR(ret < 0, "Could not decode image", cleanup);
        }

        cout << "Image Resolution - " << width << " x " << height << endl;
        write_video_frame(ctx.out_file[i], *buffer);
        delete buffer;
        goto cleanup;
      }

      /**
       * Case 2:
       * Decode to hardware buffer memory by decodeToFd(), convert to
       * other format then write to local file.
       */
      for (int i = 0; i < iterator_num; ++i)
      {
        ret = ctx.jpegdec->decodeToFd(fd, ctx.in_buffer, ctx.in_file_size, pixfmt,
            width, height);
        TEST_ERROR(ret < 0, "Could not decode image", cleanup);
      }

      cout << "Image Resolution - " << width << " x " << height << endl;

      NvBufSurf::NvCommonAllocateParams params;
      /* Create PitchLinear output buffer for transform. */
      params.memType = NVBUF_MEM_SURFACE_ARRAY;
      params.width = width;
      params.height = height;
      params.layout = NVBUF_LAYOUT_PITCH;
      if (out_pixfmt == 1)
        params.colorFormat = NVBUF_COLOR_FORMAT_NV12;
      else if (out_pixfmt == 2)
        params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
      else if (out_pixfmt == 3)
        params.colorFormat = NVBUF_COLOR_FORMAT_NV16;
      else if (out_pixfmt == 4)
        params.colorFormat = NVBUF_COLOR_FORMAT_NV24;

      params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

      ret = NvBufSurf::NvAllocate(&params, 1, &dst_dma_fd);
      TEST_ERROR(ret == -1, "create dmabuf failed", cleanup);

      /* Clip & Stitch can be done by adjusting rectangle. */
      NvBufSurf::NvCommonTransformParams transform_params;
      transform_params.src_top = 0;
      transform_params.src_left = 0;
      transform_params.src_width = width;
      transform_params.src_height = height;
      transform_params.dst_top = 0;
      transform_params.dst_left = 0;
      transform_params.dst_width = width;
      transform_params.dst_height = height;
      transform_params.flag = NVBUFSURF_TRANSFORM_FILTER;
      transform_params.flip = NvBufSurfTransform_None;
      transform_params.filter = NvBufSurfTransformInter_Nearest;
      ret = NvBufSurf::NvTransform(&transform_params, fd, dst_dma_fd);
      TEST_ERROR(ret == -1, "Transform failed", cleanup);

      /* Write raw video frame to file. */
      if (ctx.out_file)
      {
          int index = ctx.current_file++;
          /* Dumping two planes for NV12, NV16, NV24 and three for I420 */
          dump_dmabuf(dst_dma_fd, 0, ctx.out_file[index]);
          dump_dmabuf(dst_dma_fd, 1, ctx.out_file[index]);
          if (out_pixfmt == 2)
          {
              dump_dmabuf(dst_dma_fd, 2, ctx.out_file[index]);
          }
      }

cleanup:
      if (ctx.perf)
      {
        ctx.jpegdec->printProfilingStats(cout);
      }

      if(dst_dma_fd != -1)
      {
          ret = NvBufSurf::NvDestroy(dst_dma_fd);
          dst_dma_fd = -1;
      }

      delete[] ctx.in_buffer;
    }
    

    for(i = 0; i < ctx.num_files; i++)
    {

      delete ctx.in_file[i];
      delete ctx.out_file[i];

      free(ctx.in_file_path[i]);
      free(ctx.out_file_path[i]);
    }
    /**
     * Destructors do all the cleanup, unmapping and deallocating buffers
     * and calling v4l2_close on fd
     */
    delete ctx.jpegdec;

    return -error;
#endif

      /**
       * Case 1:
       * Decode to software buffer memory by decodeToBuffer() and write to
       * local file.
       */
       
	//if (!ctx.use_fd)
	//{
	//width = 1280;
	//height = 720;
	/*
	// 1. Check if we need to allocate or re-allocate
	if (global_surf == NULL || 
		global_surf->surfaceList[0].width != width || 
		global_surf->surfaceList[0].height != height) 
	{
		if (global_surf) NvBufSurfaceDestroy(global_surf);
		
		// 1. Allocate the hardware surface
		NvBufSurfaceCreateParams params = {0};
		params.gpuId = 0;
		params.width = width;
		params.height = height;
		params.size = 0;
		params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
		params.layout = NVBUF_LAYOUT_PITCH;
		params.memType = NVBUF_MEM_DEFAULT;

		if (NvBufSurfaceCreate(&global_surf, 1, &params) != 0) {
			printf("Error: NvBufSurfaceCreate failed\n");
		}

		// 2. Manual NvBuffer allocation (Bypass all Constructor/Enum errors)
		if (global_buffer) {
			free(global_buffer);
		}
		
		// Allocate raw memory for the NvBuffer structure
		global_buffer = (NvBuffer*) malloc(sizeof(NvBuffer));
		if (global_buffer == NULL) {
			printf("Error: Failed to allocate global_buffer memory\n");
			return -1;
		}
		
		// Clear the memory to avoid garbage values
		memset(global_buffer, 0, sizeof(NvBuffer));
		
		// Manually set the required members for YUV420
		global_buffer->n_planes = 3;
		
		printf("Successfully allocated and forged NvBuffer for %dx%d\n", width, height);
	}
	// 3. Link the planes (Every frame or just once in the IF block)
	global_buffer->n_planes = 3; 
	for (uint32_t i = 0; i < global_buffer->n_planes; i++) {
		global_buffer->planes[i].fd = (int)global_surf->surfaceList[0].bufferDesc;
		
		// Set geometry correctly
		global_buffer->planes[i].fmt.width = (i == 0) ? width : width / 2;
		global_buffer->planes[i].fmt.height = (i == 0) ? height : height / 2;
		global_buffer->planes[i].fmt.stride = global_surf->surfaceList[0].pitch;
	}	*/

	
		NvBuffer *buffer = NULL;


		printf("Decoding jpeg: %d X %d, Size: %d bytes", width, height, len);
		ret = ctx.jpegdec->decodeToBuffer(&buffer, (unsigned char*)jpgbuffer,len, &pixfmt, &width, &height);
		if(ret < 0)
		{
			printf("Could not decode image\n");
			return -1;
		}
		else
		{
			
			//write_video_frame(ctx.out_file[i], *buffer);
			
			/*char* osd_text = "hello world gc camera";
			NvOSD_TextParams textParams;
			void *nvosd_context;
			
			textParams.display_text = osd_text ? : strdup("nvosd overlay text");
			textParams.x_offset = 130;
			textParams.y_offset = 130;
			textParams.font_params.font_name = strdup("Arial");
			textParams.font_params.font_size = 30;
			textParams.font_params.font_color.red = 1.0;
			textParams.font_params.font_color.green = 0.0;
			textParams.font_params.font_color.blue = 1.0;
			textParams.font_params.font_color.alpha = 1.0;

			nvosd_context = nvosd_create_context();
			nvosd_put_text(nvosd_context, MODE_CPU, buffer, 1, &textParams);
			nvosd_destroy_context(nvosd_context);*/

			for (uint32_t i = 0; i < buffer->n_planes; i++) {
				// Accessing the Plane elements
				int plane_fd = buffer->planes[i].fd;
				uint32_t plane_width = buffer->planes[i].fmt.width;
				uint32_t plane_height = buffer->planes[i].fmt.height;
				uint32_t plane_stride = buffer->planes[i].fmt.stride;

				printf("Plane %d: FD=%d, Size=%ux%u, Stride=%u\n", 
						i, plane_fd, plane_width, plane_height, plane_stride);
			}

			// 1. Prepare Timestamp String
			time_t now = time(NULL);
			struct tm *t = localtime(&now);
			char timeStr[64];
			//strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);
			strftime(timeStr, sizeof(timeStr), "%d-%b-%Y %H:%M:%S", t);
			/*
			// Generate timestamp string
			char ts[64];
			time_t now = time(0);
			strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
			*/

			// Apply OSD to NVBuffer
			addOverlaytextToNvBuffer(buffer, width, height, timeStr, BOTTOM_LEFT, TEXT_BORDER);
			
			if((X264ShmImgPtr->yuv_filled[X264ShmImgPtr->yuv_buff_cnt]==0))
			{

			    uint32_t i, j;
			    char *data;
			    unsigned long tmpSize;
			    tmpSize=0;
			    
			    for (i = 0; i < buffer->n_planes; i++)
			    {
					NvBuffer::NvBufferPlane &plane = buffer->planes[i];
					size_t bytes_to_write =  plane.fmt.bytesperpixel * plane.fmt.width;
				
					//printf("JPG Decode %d %d %d\n",buffer->n_planes,plane.fmt.height,);
					data = (char *) plane.data;
					for (j = 0; j < plane.fmt.height; j++)
					{
						//stream->write(data, bytes_to_write);
						mempcpy((char *)X264ShmImgPtr->yuv_payload[X264ShmImgPtr->yuv_buff_cnt]+tmpSize,data,bytes_to_write);
						tmpSize+=bytes_to_write;
						
						data += plane.fmt.stride;
					}
			    }			
				


				X264ShmImgPtr->yuv_size[X264ShmImgPtr->yuv_buff_cnt]=tmpSize;
				X264ShmImgPtr->yuvframecnt[X264ShmImgPtr->yuv_buff_cnt]=X264ShmImgPtr->jpgframecnt[buff_counter];
				//printf("Image Resolution %d %d %ld %d %ld\n",width,height,tmpSize,X264ShmImgPtr->yuvframecnt[X264ShmImgPtr->yuv_buff_cnt],X264ShmImgPtr->size[buff_counter]);

				X264ShmImgPtr->yuv_filled[X264ShmImgPtr->yuv_buff_cnt]=1;
				
				X264ShmImgPtr->yuv_buff_cnt++;
				if(X264ShmImgPtr->yuv_buff_cnt>1)
				{
					X264ShmImgPtr->yuv_buff_cnt=0;
				}
			}	
			if(buffer)				
				delete buffer;
			return 1;
		}		
		
      //}
      //else
      //{
     // 		return -1;
     // }

}

void ConfigureX264ServerSharedMemory(void)
{  
	void *X264SharedMemPtr;
	int X264SharedMemHandle,nRet;

	struct stat statbuf;
	
    	X264SharedMemHandle = shm_open("rtsp_x264_shm.dat", O_RDWR, 0660);
     	fstat(X264SharedMemHandle, &statbuf);
    	X264SharedMemPtr = mmap(NULL,statbuf.st_size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, X264SharedMemHandle, 0);  
    	X264ShmImgPtr = (SHM_X264*)X264SharedMemPtr; 
}
void DebugPrint(unsigned char * source, unsigned short buffer_length)
{
	unsigned short i,j,k;
	char c;
	
	k=buffer_length%8;
	printf("******Debug Print**********\n");
	for(i = 0; i < (buffer_length/8); i++)
	{
		printf("%04d :",i*8);
		for(j = 0; j < 8; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
		}
		printf("\t");
		for(j = 0; j < 8; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");
	}
	
	if(k)
	{
		printf("%04d :",i*8);
		for(j = 0; j < k; j++)
		{
			printf(" %02x",(unsigned char) *(source+j+i*8));
		}
		for(j = 0; j < (8-k); j++)
		{
			printf("   ");
		}
		printf("\t");
		for(j = 0; j < k; j++)
		{
			c = *(source+j+i*8);
			printf("%c",((c<0x20)||(c>0x7e))?'.':c );
		}
		printf("\n");    
	}
	printf("-----------Debug Print------------\n");
}
int main(int argc, char *argv[])
{
	context_t ctx;
	int ret = 0;
	char filename[256];

	if (argc >= 2)
	{
		if(strcmp(argv[1], "--version") == 0)
		{
			printf("jpeg_decode %s", APP_VERSION);
			exit(0);
		}
		else if(strcmp(argv[1], "-v") == 0)
		{
			printf("-----------------------\n Version Details\n-----------------------\n");
			printf("jpeg_decode %s\n", APP_VERSION);
			printf("Build DateTime: %s %s\n", __DATE__, __TIME__);
			exit(0);
		}
	}

	sleep(2);

	
	
	ConfigureX264ServerSharedMemory();
    set_defaults(&ctx);

	/*
	 * Create and destroy decoder for every frame.
	 * This is an issue with jp5.1.2 
	 * For jp6 , this is not required. 
	 * Create only once.			
	 */
	ctx.jpegdec = NvJPEGDecoder::createJPEGDecoder("jpegdec");
	if(!ctx.jpegdec)
	{
		printf("Could not create Jpeg Decoder\n");
		return -1;
	}

	while(1)
	{
		if((X264ShmImgPtr->filled[buff_counter]==1))
		{
			/*
			 * Create and destroy decoder for every frame.
			 * This is an issue with jp5.1.2 
			 * For jp6 , this is not required.			
			 */
			
/*			ctx.jpegdec = NvJPEGDecoder::createJPEGDecoder("jpegdec");
			if(!ctx.jpegdec)
			{
				printf("Could not create Jpeg Decoder\n");
				return -1;
			}*/
			
			//DebugPrint(X264ShmImgPtr->payload[buff_counter],16);
			ret = jpeg_decode_proc(ctx, X264ShmImgPtr->size[buff_counter],(char *)X264ShmImgPtr->payload[buff_counter]);			
			if(ret<0)
			{
				goto cleanup;
			}
			else
			{
				X264ShmImgPtr->filled[buff_counter]=0;
				buff_counter++;
				if(buff_counter>1)
				{
					buff_counter=0;
				}
			}
			
			/*
			 * Create and destroy decoder for every frame.
			 * This is an issue with jp5.1.2 
			 * For jp6 , this is not required.			
			 */
			//delete ctx.jpegdec;
		
			printf(">>>>>>>>>>>Haaaiiii33<<<<<<\n");
				
		}
		else
		{				
			usleep(1000);		
		}
	} 
   
cleanup:

	/**
	* Destructors do all the cleanup, unmapping and deallocating buffers
	* and calling v4l2_close on fd
	*/
	
	
	return -1;
}

#if 0
int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;

    /* save iterator number */
    int iterator_num = 0;

    do
    {
        ret = jpeg_decode_proc(ctx, argc, argv);
        iterator_num++;
    } while((ctx.stress_test != iterator_num) && ret == 0);

    if (ret)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }

    return ret;
}

#endif

